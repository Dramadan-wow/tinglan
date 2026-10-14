"""Exercise profile generator, real settings window, and packaged bootstrap without real data."""
from pathlib import Path
import hashlib,json,os,shutil,sqlite3,subprocess,sys,time,zipfile
sys.stdout.reconfigure(encoding='utf-8')
import network_integration as n
sys.path.insert(0,str(n.ROOT/'scripts'))
import prepare_lan as setup
OUT=n.ROOT/'tests/lan-output'/str(time.time_ns())/'deployment';OUT.mkdir(parents=True)
version=(n.ROOT/'VERSION').read_text().strip();runtime=n.ROOT/f'build-package/v{version}/runtime';friend_runtime=runtime.parent/'client-runtime'
checks=[]
def check(ok,text):
    if not ok:raise AssertionError(text)
    checks.append(text);print('PASS',text,flush=True)
env={k.upper():v for k,v in os.environ.items() if not k.upper().startswith('QT_')}
env['PATH']=os.pathsep.join([env['SYSTEMROOT'],str(Path(env['SYSTEMROOT'])/'System32')]);env['TINGLAN_HOME']=str(OUT/'home')
def run(args,timeout=30):return subprocess.run([str(x) for x in args],env=env,cwd=OUT,capture_output=True,timeout=timeout,creationflags=subprocess.CREATE_NO_WINDOW)

try:
    source=OUT/'source';source.mkdir();db=source/'workspace.sqlite'
    with sqlite3.connect(db) as conn:
        conn.execute('PRAGMA journal_mode=WAL');conn.execute('CREATE TABLE preserved(value TEXT)');conn.execute("INSERT INTO preserved VALUES ('测试保留数据🙂')");conn.commit()
        config=source/'server.json';setup.write_json(config,dict(workspaceFile='workspace.sqlite',usersFile='users.json',workspaceImport='should-not-be-imported.txt',tls=False,allowInsecureLoopback=True,idleExitMs=60000,listenAddress='127.0.0.1',port=56110))
        original=config.read_bytes();profile=OUT/'profile'
        setup.prepare(config,'127.0.0.1','127.0.0.1',profile,runtime)
        check(config.read_bytes()==original,'Generator leaves original config unchanged')
        check(conn.execute('SELECT value FROM preserved').fetchone()[0]=='测试保留数据🙂','Original WAL database unchanged')
        with sqlite3.connect(profile/'backup/workspace.sqlite') as copied:
            check(copied.execute('SELECT value FROM preserved').fetchone()[0]=='测试保留数据🙂','Backup contains committed WAL data')
            check(copied.execute('PRAGMA integrity_check').fetchone()[0]=='ok','Backup passes SQLite integrity check')
    manifest=setup.read_json(profile/'backup/manifest.json')
    check(all((profile/'backup'/name).is_file() and hashlib.sha256((profile/'backup'/name).read_bytes()).hexdigest()==digest for name,digest in manifest['sha256'].items()),'Backup manifest hashes remain valid after all connections close')
    server=setup.read_json(profile/'server/server.json');client=setup.read_json(profile/'client/client.json')
    check(Path(server['workspaceFile'])==db and 'workspaceImport' not in server,'LAN profile points to original central database without import')
    check(server['tls'] and not server['allowInsecureLoopback'] and server['idleExitMs']==0 and server['port']==56110,'Independent server profile requires TLS and disables idle shutdown')
    check(client['tls'] and not client['allowInsecureLoopback'] and not client['autoStartLocalServer'] and client['port']==56110,'Client profile requires TLS and disables local server')
    from cryptography import x509
    cert=x509.load_pem_x509_certificate((profile/'server/server.pem').read_bytes())
    check(str(cert.extensions.get_extension_for_class(x509.SubjectAlternativeName).value[0].value)=='127.0.0.1','Generated SAN matches supplied test address exactly')
    check(not (profile/'Tinglan-LAN-Client.zip').exists(),'Deployment no longer requires a per-server ZIP')
    check(not any(p.suffix in ('.key','.sqlite','.db','.exe') for p in (profile/'client').rglob('*')),'Shared configuration contains no executable, private key or database')
    check((profile/'connection-code.txt').read_text().startswith('TINGLAN1.'),'Text connection code created automatically')
    try:setup.prepare(config,'127.0.0.1','127.0.0.1',profile,runtime);refused=False
    except FileExistsError:refused=True
    check(refused,'Generator refuses to overwrite an existing profile')
    # Isolated runtime for UI tests. Never ship the test executable.
    ui=OUT/'ui-runtime';shutil.copytree(runtime,ui);shutil.copy2(n.ROOT/'build-gui/Release/connection_gui_checks.exe',ui)
    env['TINGLAN_HOME']=str(OUT/'gui')
    r=run([ui/'connection_gui_checks.exe',OUT/'gui']);(OUT/'gui.log').write_bytes(r.stdout+r.stderr)
    check(r.returncode==0,'Real Qt connection settings validation, save/cancel and local-mode restoration')
    print(r.stdout.decode('utf-8',errors='replace'),flush=True)
    env['TINGLAN_HOME']=str(OUT/'home');home=Path(env['TINGLAN_HOME']);home.mkdir(exist_ok=True)
    local=dict(host='127.0.0.1',port=n.free_port(),tls=False,allowInsecureLoopback=True,autoStartLocalServer=True)
    setup.write_json(home/'client.json',local);original=(home/'client.json').read_bytes()
    # Main program, not just a session probe: explicit TLS profile suppresses bootstrap.
    app=subprocess.Popen([str(friend_runtime/'im_client.exe'),'--client-config',str(profile/'client/client.json')],env=env,cwd=OUT,creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        time.sleep(4);check(app.poll() is None,'Deployed friend client opens its actual login window')
        check(not (home/'server.json').exists() and not (home/'server').exists(),'Friend client does not create or start local database/server')
        check((home/'client.json').read_bytes()==original,'Explicit profile does not overwrite preexisting local mode')
    finally:app.terminate();app.wait(15)
    help_result=run([runtime/'im_desktop.exe','--help'])
    check(help_result.returncode==0 and b'--connection-settings' in help_result.stdout and b'--client-config' in help_result.stdout,'Release exposes both new entry points')
    status='passed';error=None
except Exception as e:
    status='failed';error=repr(e);print('FAIL',error,flush=True)
finally:
    setup.write_json(OUT/'results.json',dict(status=status,count=len(checks),checks=checks,error=error));print('RESULT',OUT/'results.json',flush=True)
raise SystemExit(status!='passed')
