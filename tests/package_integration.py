"""Exercise release EXEs with only Windows directories on PATH and isolated data."""
import os,json,time,subprocess,hashlib,socket
from pathlib import Path
import network_integration as n
import workspace_integration as w
ROOT=n.ROOT;OUT=ROOT/'tests/package-output'/str(time.time_ns());OUT.mkdir(parents=True)
VERSION=os.environ.get('TINGLAN_TEST_VERSION',(ROOT/'VERSION').read_text().strip());DIST=ROOT/f'dist/v{VERSION}';checks=[]
env={k.upper():v for k,v in os.environ.items() if not k.upper().startswith('QT_')}
env['PATH']=os.pathsep.join([env['SYSTEMROOT'],str(Path(env['SYSTEMROOT'])/'System32')])
env['TINGLAN_HOME']=str(OUT/'userdata');env['LOCALAPPDATA']=str(OUT/'appdata')
def check(ok,text):
    if not ok:raise AssertionError(text)
    checks.append(text);print('PASS',text,flush=True)
def run(args,timeout=180):return subprocess.run([str(x) for x in args],cwd=OUT,env=env,timeout=timeout,capture_output=True)
def wait(test,seconds=30):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        if test():return
        time.sleep(.1)
    raise TimeoutError('Runtime readiness timed out')
def open_port(port):
    try:
        s=socket.create_connection(('127.0.0.1',port),.2);s.close();return True
    except OSError:return False
def main():
    extracted=OUT/'extracted'
    p=run([DIST/f'Tinglan-{VERSION}-Portable.exe','--extract-only',extracted]);check(p.returncode==0,'Single EXE extracts its embedded runtime')
    manifest=json.loads((DIST/'runtime-manifest.json').read_text())
    check(all(hashlib.sha256((extracted/k).read_bytes()).hexdigest()==v for k,v in manifest.items()),'Extracted payload matches every release file hash')
    check(not any((extracted/x).exists() for x in ['data','server','tests','.tools']),'Distribution contains no private database, test data or SDK')
    p=run([extracted/'im_desktop.exe','--self-test',OUT/'gui-checks']);report=json.loads((OUT/'gui-checks/results.json').read_text(encoding='utf-8'))
    check(p.returncode==0 and report['status']=='passed',f"Deployed GUI, music and business regression: {report['count']} checks without SDK PATH")
    # Launch the actual portable entry point, with its normal cache extraction and bootstrap.
    home=Path(env['TINGLAN_HOME']);home.mkdir(parents=True);port=n.free_port()
    (home/'client.json').write_text(json.dumps(dict(host='127.0.0.1',port=port,tls=False,allowInsecureLoopback=True,autoStartLocalServer=True)))
    launcher=subprocess.Popen([DIST/f'Tinglan-{VERSION}-Portable.exe'],env=env,cwd=OUT)
    try:
        wait(lambda:(home/'server.json').exists() and open_port(port),60)
        check(launcher.poll() is None,'Normal portable launch starts GUI and local central server')
        a,ar=w.login(port,'发行测试甲','test-package-password-2026',register=True)
        b,br=w.login(port,'发行测试乙','test-package-password-2026',register=True)
        check(ar['data']['ok'] and br['data']['ok'],'Fresh packaged server supports two independent registered accounts')
        w.snapshot(a);w.snapshot(b)
        for raw,response in ((a,ar),(b,br)):
            uid=int(response['data']['userId'][1:]);raw.send(n.message('workspace_command',dict(op='account.open',contact=2,number=str(uid),password='test-package-password-2026',shared=True,service=0),sender=response['data']['userId']));check(w.receive(raw,'workspace_result')['data']['ok'],'Packaged account opens Weibo service')
        a.send(n.message('workspace_command',dict(op='feed.publish',channel='weibo',text='打包后的公开微博',service=0),sender=ar['data']['userId']))
        result=w.receive(a,'workspace_result')['data'];check(result['ok'],'Packaged server accepts registered user public post')
        state=w.snapshot(b)
        while not any(p['text']=='打包后的公开微博' for p in state['weibo']):state=w.snapshot(b)
        check(not state['feed'] and bool(state['weibo']),'Packaged server shares public posts while keeping moments private')
        a.close();b.close()
    finally:
        # Only this launcher's direct GUI child; never touch a user's existing program/server.
        script=f"Get-CimInstance Win32_Process -Filter 'ParentProcessId={launcher.pid}' | Where-Object {{$_.Name -eq 'im_desktop.exe'}} | ForEach-Object {{Stop-Process -Id $_.ProcessId -Force}}"
        run([Path(env['SYSTEMROOT'])/'System32/WindowsPowerShell/v1.0/powershell.exe','-NoProfile','-NonInteractive','-WindowStyle','Hidden','-Command',script])
        try:launcher.wait(10)
        except subprocess.TimeoutExpired:launcher.terminate();launcher.wait(10)
    if os.environ.get('TINGLAN_TEST_SKIP_INSTALL')!='1':
        installed=OUT/'installed'
        p=run([DIST/f'Tinglan-{VERSION}-Setup.exe','/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/NOICONS','/TASKS=',f'/DIR={installed}',f'/LOG={OUT / "install.log"}'])
        check(p.returncode==0 and (installed/'unins000.exe').exists(),'Installer successfully installs to an isolated per-user directory')
        check(all(hashlib.sha256((installed/k).read_bytes()).hexdigest()==v for k,v in manifest.items()),'Installed runtime matches release manifest')
        p=run([installed/'im_desktop.exe','--version']);check(p.returncode==0,'Installed application starts without development SDK')
        p=run([installed/'unins000.exe','/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART']);check(p.returncode==0 and not (installed/'im_desktop.exe').exists(),'Uninstaller removes installed program')
        check((home/'server/workspace.sqlite').exists(),'Uninstall preserves independent user data')
    # Repeated connection probes would themselves reset the server's idle timer.
    time.sleep(62)
    check(not open_port(port),'Packaged local server exits after idle timeout')
if __name__=='__main__':
    status,error='passed',None
    try:main()
    except Exception as e:status,error='failed',repr(e);print('FAIL',error,flush=True)
    finally:(OUT/'results.json').write_text(json.dumps(dict(status=status,checks=checks,count=len(checks),error=error,skipped=(["Installer install/uninstall: not run against current Windows user registry"] if os.environ.get("TINGLAN_TEST_SKIP_INSTALL")=="1" else [])),ensure_ascii=False,indent=2),encoding='utf-8');print('RESULT',OUT/'results.json',flush=True)
    raise SystemExit(status!='passed')
