"""Real central server + Qt updater processes, isolated TLS/data; no installer execution."""
from pathlib import Path
import hashlib,json,ssl,socket,subprocess,sys,time
import network_integration as n
sys.path.insert(0,str(n.ROOT/'scripts'))
import prepare_lan as setup
import publish_client_update as publisher
sys.stdout.reconfigure(encoding='utf-8')
OUT=n.ROOT/'tests/update-output'/str(time.time_ns());OUT.mkdir(parents=True)
n.OUT=OUT;n.SERVER=n.ROOT/'build-gui/server/Release/im_server.exe'
PROBE=n.ROOT/'build-gui/Release/update_client_probe.exe'
checks=[]
def check(value,name):
    if not value:raise AssertionError(name)
    checks.append(name);print('PASS',name,flush=True)
def run(config,name,current='5.4.0',cancel=False):
    output=OUT/name
    result=subprocess.run([str(PROBE),str(config),current,'60000',str(output)]+(['cancel'] if cancel else []),
        env=n.ENV,cwd=n.ROOT,capture_output=True,timeout=70,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
    (OUT/(name+'.log')).write_bytes(result.stdout+result.stderr)
    check(result.returncode==0,name+' updater process completes')
    return [json.loads(line) for line in result.stdout.decode('utf-8').splitlines() if line.startswith('{')]
def main():
    cert=OUT/'tls';cert.mkdir();setup.certificates('127.0.0.1',cert)
    port=n.free_port();server=OUT/'server.json';client=OUT/'client.json'
    setup.write_json(server,dict(listenAddress='127.0.0.1',port=port,tls=True,certificateFile=str(cert/'server.pem'),keyFile=str(cert/'server.key'),usersFile=str(OUT/'users.json')))
    setup.write_json(client,dict(host='127.0.0.1',port=port,tls=True,caFile=str(cert/'ca.pem')))
    original=server.read_bytes();n.start_server(server,'updates-server')
    events=run(client,'unpublished');check(events[-1]==dict(event='current',published=False),'No published update distinguished from latest')
    release=OUT/'release';release.mkdir();body=b'MZ'+bytes(range(256))*4096
    manifest=dict(schema=1,edition='client',platform='windows-x64',version='5.5.0',size=len(body),sha256=hashlib.sha256(body).hexdigest(),notes='中文更新说明 🎉')
    installer=release/'Tinglan-5.5.0-Client-Setup.exe';installer.write_bytes(body);setup.write_json(release/'client-update.json',manifest)
    feed=publisher.publish(release,server);check(publisher.publish(release,server)==feed,'Publishing same immutable package is idempotent')
    check(server.read_bytes()==original and not (OUT/'workspace.sqlite').exists(),'Publishing leaves server configuration and databases untouched')
    events=run(client,'download');ready=events[-1];check(ready['event']=='ready' and Path(ready['file']).read_bytes()==body,'Qt updater downloads all TLS chunks and verifies exact bytes')
    gui=OUT/'gui';env={**n.ENV,'TINGLAN_HOME':str(gui)}
    result=subprocess.run([str(n.ROOT/'build-gui/Release/update_gui_checks.exe'),str(client),str(gui)],env=env,capture_output=True,timeout=70,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
    (OUT/'gui.log').write_bytes(result.stdout+result.stderr)
    check(result.returncode==0,'Actual GUI automatic notification, download, verified state and manual update entry')
    for v in ['5.5.0','5.10.0']:
        events=run(client,'version-'+v,v);check(events[-1]==dict(event='current',published=True),'No equal-version reinstall or downgrade: '+v)
    events=run(client,'cancel',cancel=True);check(events[-1]['event']=='cancelled' and not list((OUT/'cancel').rglob('*.exe')),'Cancel removes partial installer')
    target=feed.parent/installer.name;target.write_bytes(body[:-1]+b'X')
    events=run(client,'tampered');check(events[-1]['event']=='failed' and 'SHA-256' in events[-1]['detail'] and not list((OUT/'tampered').rglob('*.exe')),'Tampered installer rejected and never finalized')
    target.write_bytes(body)
    wrong=OUT/'wrong-ca';wrong.mkdir();setup.certificates('127.0.0.1',wrong)
    for name,changes in [('wrong-ca',dict(caFile=str(wrong/'ca.pem'))),('wrong-san',dict(host='localhost')),('plaintext',dict(tls=False))]:
        config=OUT/(name+'.json');setup.write_json(config,{**json.loads(client.read_text(encoding="utf-8")),**changes});events=run(config,name)
        check(events[-1]['event']=='failed' and not list((OUT/name).rglob('*.exe')),'Reject '+name+' without plaintext fallback')
    context=ssl.create_default_context(cafile=str(cert/'ca.pem'))
    def raw():
        r=n.Raw.__new__(n.Raw);r.s=context.wrap_socket(socket.create_connection(('127.0.0.1',port)),server_hostname='127.0.0.1');r.s.settimeout(10);return r
    r=raw();request=n.message('update_check',dict(edition='desktop',platform='windows-x64',version='5.4.0'));r.send(request)
    check(r.recv()['data']['code']=='invalid_update_check','Admin installer cannot be requested from update channel');r.close()
    r=raw();request=n.message('update_check',dict(edition='client',platform='windows-x64',version='5.4.0'));frame=n.frame(request)
    r.s.sendall(frame[:2]);time.sleep(.03);r.s.sendall(frame[2:]);check(r.recv()['data']['available'],'Update request supports TCP half frames')
    r.send(n.message('update_chunk',dict(sha256=manifest['sha256'],offset=1)));check(r.recv()['data']['code']=='invalid_update_chunk','Out-of-order chunk rejected');r.close()
    for label,change in [('wrong-edition',{'edition':'desktop'}),('oversized',{'size':2**30}),('path',{'version':'../secret'})]:
        setup.write_json(feed,{**manifest,**change});events=run(client,'manifest-'+label);check(events[-1]==dict(event='current',published=False),'Server rejects '+label+' manifest')
    setup.write_json(feed,manifest)
    setup.write_json(release/'client-update.json',{**manifest,'sha256':'0'*64})
    try:publisher.publish(release,server);raise AssertionError('bad hash published')
    except ValueError:check(True,'Publisher rejects mismatched installer hash')
    check(json.loads(feed.read_text(encoding="utf-8"))==manifest,'Failed publication preserves current feed')
    setup.write_json(release/'client-update.json',manifest)
    setup.write_json(feed,{**manifest,'version':'5.6.0'})
    try:publisher.publish(release,server);raise AssertionError('downgrade published')
    except ValueError:check(True,'Publisher refuses release downgrade')
    setup.write_json(feed,manifest)
    check(not (feed.parent/'.publish.lock').exists(),'Publisher releases lock after failed publication')
    # Android feed: its own manifest and APK, served on the same TLS update channel.
    apk=b'PK'+bytes(range(256))*300
    android=dict(schema=1,edition='client',platform='android',version='1.3.3',size=len(apk),sha256=hashlib.sha256(apk).hexdigest(),notes='安卓更新')
    (release/'Tinglan-Android-1.3.3.apk').write_bytes(apk);setup.write_json(release/'android-update.json',android)
    try:publisher.publish(release,server,'android-update.json')
    except ValueError:raise AssertionError('Android package not published')
    check(json.loads((feed.parent/'android-stable.json').read_text(encoding='utf-8'))==android and json.loads(feed.read_text(encoding='utf-8'))==manifest,'Android feed published beside the unchanged Windows feed')
    r=raw();r.send(n.message('update_check',dict(edition='client',platform='android',version='1.3.2')));info=r.recv()['data']
    check(info['available'] and info['release']==android,'Android client is offered the APK, not the Windows installer')
    received=b''
    while len(received)<len(apk):
        r.send(n.message('update_chunk',dict(sha256=android['sha256'],offset=len(received))));received+=__import__('base64').b64decode(r.recv()['data']['payload'])
    r.close();check(received==apk,'Android APK downloads in verified TLS chunks')
    r=raw();r.send(n.message('update_check',dict(edition='client',platform='android',version='1.3.3')));check(not r.recv()['data']['available'],'Same Android version is not offered again');r.close()
    r=raw();r.send(n.message('update_check',dict(edition='client',platform='ios',version='1.0.0')));check(r.recv()['data']['code']=='invalid_update_check','Unknown platform rejected');r.close()
    setup.write_json(release/'android-update.json',{**android,'platform':'windows-x64'})
    try:publisher.publish(release,server,'android-update.json');raise AssertionError('APK published as Windows installer')
    except (ValueError,FileNotFoundError):check(json.loads(feed.read_text(encoding='utf-8'))==manifest,'APK cannot be published into the Windows feed')
status='passed';error=None
try:main()
except Exception as e:status='failed';error=repr(e);print('FAIL',error,flush=True)
finally:
    for process in n.children:process.kill()
    setup.write_json(OUT/'results.json',dict(status=status,count=len(checks),checks=checks,error=error));print('RESULT',OUT/'results.json',flush=True)
raise SystemExit(status!='passed')
