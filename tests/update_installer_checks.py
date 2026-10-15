"""Actual 5.4 -> current ordinary-client installer upgrade, isolated registration/data."""
from pathlib import Path
import hashlib,json,os,subprocess,sys,time,winreg
import network_integration as n
sys.path.insert(0,str(n.ROOT/'.tools/python'))
import psutil
sys.stdout.reconfigure(encoding='utf-8')
OUT=n.ROOT/'tests/update-output'/('installer-'+str(time.time_ns()));OUT.mkdir(parents=True)
VERSION=(n.ROOT/'VERSION').read_text().strip();installed=OUT/'installed';home=OUT/'home';home.mkdir()
product=r'Software\Microsoft\Windows\CurrentVersion\Uninstall\{9BA1EB25-7653-481C-9288-A59F5E3ECA46}_is1'
env={k.upper():v for k,v in os.environ.items() if not k.upper().startswith('QT_')}
env['PATH']=os.pathsep.join([env['SYSTEMROOT'],str(Path(env['SYSTEMROOT'])/'System32')]);env['TINGLAN_HOME']=str(home)
checks=[];owned=False;waiter=None
def exists():
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER,product):return True
    except FileNotFoundError:return False
def check(value,name):
    if not value:raise AssertionError(name)
    checks.append(name);print('PASS',name,flush=True)
def run(args,**kw):return subprocess.run([str(a) for a in args],env=env,cwd=OUT,capture_output=True,timeout=180,creationflags=subprocess.CREATE_NO_WINDOW,**kw)
def clients():
    result=[]
    for process in psutil.process_iter(['exe']):
        try:
            if process.info['exe'] and Path(process.info['exe']).resolve()==(installed/'im_client.exe').resolve():result.append(process)
        except (psutil.NoSuchProcess,psutil.AccessDenied):pass
    return result
status='passed';error=None
try:
    if exists():raise RuntimeError('Existing ordinary-client installation detected; refusing to overwrite it')
    old=n.ROOT/'dist/v5.4.0/Tinglan-5.4.0-Client-Setup.exe'
    common=['/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/NOICONS','/TASKS=',f'/DIR={installed}']
    result=run([old,*common,f'/LOG={OUT/"old-install.log"}']);owned=exists()
    check(result.returncode==0 and owned,'Old 5.4 ordinary client installed in isolated directory')
    sentinel=b'isolated user data must survive';(home/'preserve.bin').write_bytes(sentinel)
    config=dict(host='127.0.0.1',port=9,tls=True,allowInsecureLoopback=False,autoStartLocalServer=False)
    (home/'client.json').write_text(json.dumps(config),encoding='utf-8');before=(home/'client.json').read_bytes()
    oldhash=hashlib.sha256((installed/'im_client.exe').read_bytes()).hexdigest()
    waiter=subprocess.Popen([sys.executable,'-c','import time; time.sleep(8)'],env=env,creationflags=subprocess.CREATE_NO_WINDOW)
    setup=n.ROOT/f'dist/v{VERSION}/Tinglan-{VERSION}-Client-Setup.exe'
    updater=subprocess.Popen([str(setup),*common,'/NOCLOSEAPPLICATIONS','/NORESTARTAPPLICATIONS','/TINGLANUPDATE=1',f'/TINGLANPID={waiter.pid}',f'/LOG={OUT/"upgrade.log"}'],env=env,cwd=OUT,creationflags=subprocess.CREATE_NO_WINDOW)
    time.sleep(3)
    check(waiter.poll() is None and updater.poll() is None and hashlib.sha256((installed/'im_client.exe').read_bytes()).hexdigest()==oldhash,'Installer waits for previous process; does not replace running application')
    check(updater.wait(180)==0,'Automatic upgrade installation completes successfully')
    waiter.wait(15)
    n.wait_for(lambda:clients(),timeout=20)
    check(bool(clients()),'Update installer automatically restarts ordinary client')
    for process in clients():process.terminate();process.wait(15)
    check((home/'preserve.bin').read_bytes()==sentinel and (home/'client.json').read_bytes()==before,'Upgrade preserves user data and connection configuration')
    manifest=json.loads((n.ROOT/f'dist/v{VERSION}/client-runtime-manifest.json').read_text())
    check(all(hashlib.sha256((installed/k).read_bytes()).hexdigest()==v for k,v in manifest.items()),'Upgraded files exactly match new release manifest')
    check(not (installed/'im_server.exe').exists() and not (installed/'im_desktop.exe').exists(),'Upgrade introduces no administrator or server executable')
    result=run([installed/'im_client.exe','--version']);check(result.returncode==0 and VERSION.encode() in result.stdout,'Restarted client reports new version')
except Exception as e:status='failed';error=repr(e);print('FAIL',error,flush=True)
finally:
    if waiter and waiter.poll() is None:waiter.terminate();waiter.wait(15)
    for process in clients():process.terminate();process.wait(15)
    if owned:
        result=run([installed/'unins000.exe','/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART'])
        if result.returncode or exists():status='failed';error='Isolated uninstall failed'
        else:check(True,'Isolated test installation uninstalled without changing real data')
    (OUT/'results.json').write_text(json.dumps(dict(status=status,count=len(checks),checks=checks,error=error),ensure_ascii=False,indent=2),encoding='utf-8')
    print('RESULT',OUT/'results.json',flush=True)
raise SystemExit(status!='passed')
