"""Replay real UI business suites over strictly verified TLS, isolated loopback only.

No current Wi-Fi address is read/used. No real user database is touched.
"""
import importlib
import json
import socket
import ssl
import struct
import subprocess
import sys
import time
from pathlib import Path
sys.stdout.reconfigure(encoding='utf-8')
import network_integration as n
import workspace_integration as w
sys.path.insert(0, str(n.ROOT/'scripts'))
import prepare_lan as setup

phase=sys.argv[1]
module=importlib.import_module({'workspace':'workspace_integration','social':'social_integration','admin':'admin_network_integration'}[phase])
OUT=n.ROOT/'tests/lan-output'/str(time.time_ns())/phase
OUT.parent.mkdir(parents=True)
n.OUT=w.OUT=module.OUT=OUT
n.SERVER=n.ROOT/('build-package/v'+(n.ROOT/'VERSION').read_text().strip()+'/runtime/im_server.exe')
# Use deployed Schannel and SQL plugins, not SDK plugins.
n.ENV['QT_PLUGIN_PATH']=str(n.SERVER.parent)
certdir=OUT.parent/'tls';certdir.mkdir()
setup.certificates('127.0.0.1',certdir)
context=ssl.create_default_context(cafile=str(certdir/'ca.pem'))
original_start=n.start_server

class TLSRaw(n.Raw):
    def __init__(self, port, context=context):
        sock=socket.create_connection(('127.0.0.1',port),timeout=10)
        self.s=context.wrap_socket(sock,server_hostname='127.0.0.1');self.s.settimeout(20)
n.Raw=TLSRaw

def start(config,name):
    c=json.loads(config.read_text(encoding='utf-8'))
    c.update(tls=True,allowInsecureLoopback=False,idleExitMs=0,certificateFile=str(certdir/'server.pem'),keyFile=str(certdir/'server.key'))
    setup.write_json(config,c)
    client=OUT/'client.json'
    if client.exists():
        s=setup.read_json(client);s.update(tls=True,allowInsecureLoopback=False,autoStartLocalServer=False,caFile=str(certdir/'ca.pem'));setup.write_json(client,s)
    return original_start(config,name)
n.start_server=start

try:
    module.main()
    n.check(True,'Entire '+phase+' UI suite used verified TLS, no plaintext fallback')
    if phase=='workspace':
        config=OUT/'server.json';port=setup.read_json(config)['port']
        for body, code in ((b'not-json','invalid_json'),(b'\xff','invalid_utf8')):
            r=TLSRaw(port);r.s.sendall(struct.pack('!I',len(body))+body)
            n.check(r.recv()['data']['code']==code,'TLS malformed data rejected: '+code);r.close()
        wrong=OUT/'wrong-ca';wrong.mkdir();setup.certificates('localhost',wrong)
        for label,ctx,hostname in [('wrong CA',ssl.create_default_context(cafile=str(wrong/'ca.pem')),'127.0.0.1'),('wrong SAN',context,'localhost')]:
            rejected=False
            try:
                with socket.create_connection(('127.0.0.1',port),timeout=5) as sock:
                    with ctx.wrap_socket(sock,server_hostname=hostname):pass
            except ssl.SSLCertVerificationError:rejected=True
            n.check(rejected,'TLS handshake rejects '+label)
        gui=n.Gui('wrongca-lan',port,'isolated-test-password',tls=True,ca=wrong/'ca.pem')
        gui.event('state',lambda e:'证书' in e['text']);n.check(True,'Actual Qt client rejects untrusted CA');gui.command('quit');gui.process.wait(15)
        # Restart with localhost-only certificate, connecting by IP through Qt must fail.
        for child in n.children:
            if child.process.poll() is None:child.kill()
        c=setup.read_json(config);c.update(certificateFile=str(wrong/'server.pem'),keyFile=str(wrong/'server.key'));bad=OUT/'bad-san.json';setup.write_json(bad,c)
        server=original_start(bad,'wrong-san-server')
        gui=n.Gui('wrongsan-lan',port,'isolated-test-password',tls=True,ca=wrong/'ca.pem')
        gui.event('state',lambda e:'证书' in e['text']);n.check(True,'Actual Qt client rejects mismatched certificate SAN');gui.command('quit');gui.process.wait(15);server.kill()
        unsafe={**c,'listenAddress':'0.0.0.0','tls':False,'allowInsecureLoopback':True};setup.write_json(OUT/'unsafe.json',unsafe)
        r=subprocess.run([n.SERVER,'--config',OUT/'unsafe.json'],env=n.ENV,capture_output=True,timeout=15)
        n.check(r.returncode!=0,'Server refuses plaintext nonloopback listener')
        server=start(config,'persistent-server')
        print('Waiting 66 seconds without clients to verify independent lifetime...',flush=True)
        time.sleep(33);print('Independent server still running; completing idle check...',flush=True);time.sleep(33)
        n.check(server.process.poll() is None,'Independent server survives 66 seconds with all GUI clients closed')
    status='passed';error=None
except Exception as e:
    status='failed';error=repr(e);print('FAIL',error,flush=True)
finally:
    for child in n.children:child.kill()
    setup.write_json(OUT/'results.json',dict(status=status,count=len(n.checks),checks=n.checks,error=error,
        scope='One Windows machine, real Qt UI clients, deployed server, verified TLS on isolated loopback; two physical PCs unverified'))
    print('RESULT',OUT/'results.json',flush=True)
raise SystemExit(status!='passed')
