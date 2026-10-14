"""Install/uninstall only the new ordinary-client product in an isolated workspace directory."""
from pathlib import Path
import hashlib,json,os,subprocess,sys,time,winreg,shutil
import network_integration as n
sys.path.insert(0,str(n.ROOT/'scripts'))
import prepare_lan as setup
sys.stdout.reconfigure(encoding='utf-8')
VERSION=(n.ROOT/'VERSION').read_text().strip();DIST=n.ROOT/f'dist/v{VERSION}'
OUT=n.ROOT/'tests/client-installer-output'/str(time.time_ns());OUT.mkdir(parents=True)
checks=[];installed=OUT/'installed';product=r'Software\Microsoft\Windows\CurrentVersion\Uninstall\{9BA1EB25-7653-481C-9288-A59F5E3ECA46}_is1'
env={k.upper():v for k,v in os.environ.items() if not k.upper().startswith('QT_')};env['PATH']=os.pathsep.join([env['SYSTEMROOT'],str(Path(env['SYSTEMROOT'])/'System32')]);env['TINGLAN_HOME']=str(OUT/'home')
def check(ok,text):
    if not ok:raise AssertionError(text)
    checks.append(text);print('PASS',text,flush=True)
def run(args,timeout=180):return subprocess.run([str(x) for x in args],env=env,cwd=OUT,capture_output=True,timeout=timeout,creationflags=subprocess.CREATE_NO_WINDOW)
def product_exists():
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER,product):return True
    except FileNotFoundError:return False
owned=False;status='passed';error=None
try:
    if product_exists():raise RuntimeError('Ordinary client already installed; refusing to replace an existing registration')
    result=run([DIST/f'Tinglan-{VERSION}-Client-Setup.exe','/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/NOICONS','/TASKS=',f'/DIR={installed}',f'/LOG={OUT/"install.log"}']);owned=product_exists()
    check(result.returncode==0 and (installed/'im_client.exe').exists(),'Single EXE installer completes automatically with bundled dependencies')
    manifest=json.loads((DIST/'client-runtime-manifest.json').read_text())
    check(all(hashlib.sha256((installed/k).read_bytes()).hexdigest()==v for k,v in manifest.items()),'All installed runtime files match release manifest')
    check(not any((installed/x).exists() for x in ('im_desktop.exe','im_server.exe','data','server','管理员使用说明.md')),'Friend installer contains no admin executable, local server or private data')
    help_result=run([installed/'im_client.exe','--help']);check(help_result.returncode==0 and b'--connection-code' in help_result.stdout and b'--admin' not in help_result.stdout,'Installed ordinary EXE exposes connection code and no admin options')
    result=run([installed/'im_client.exe','--admin']);check(result.returncode!=0,'Admin command line rejected')
    app=subprocess.Popen([installed/'im_client.exe'],env=env,cwd=OUT,creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        time.sleep(3);check(app.poll() is None,'First launch opens connection setup without Qt SDK on PATH')
        check(not (OUT/'home/server.json').exists(),'First launch starts no local server')
    finally:app.terminate();app.wait(15)
    cert=OUT/'cert';cert.mkdir();setup.certificates('127.0.0.1',cert)
    # This helper is test-only and is never part of the installer manifest.
    shutil.copy2(n.ROOT/'build-gui/Release/connection_code_checks.exe',installed/'connection_code_checks.exe')
    env['TINGLAN_HOME']=str(OUT/'code-home')
    result=run([installed/'connection_code_checks.exe',OUT/'code-home',cert/'ca.pem']);(OUT/'code-checks.log').write_bytes(result.stdout+result.stderr)
    check(result.returncode==0,'Installed TLS and image plugins pass actual text import/UI security checks without SDK')
except Exception as e:status='failed';error=repr(e);print('FAIL',error,flush=True)
finally:
    if owned:
        installed.resolve().relative_to(OUT.resolve());result=run([installed/'unins000.exe','/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART'])
        if result.returncode or product_exists():status='failed';error='Test installer cleanup failed'
        else:check(True,'Isolated install uninstalled and registry registration removed')
    setup.write_json(OUT/'results.json',dict(status=status,count=len(checks),checks=checks,error=error));print('RESULT',OUT/'results.json',flush=True)
raise SystemExit(status!='passed')
