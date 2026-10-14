"""Two real ordinary-client windows over TLS; all accounts, images and DBs are isolated."""
from pathlib import Path
import base64,json,sqlite3,subprocess,sys,time
from PIL import Image,ImageDraw
import network_integration as n
import workspace_integration as w
sys.path.insert(0,str(n.ROOT/'scripts'))
import prepare_lan as setup
import ssl,socket
sys.stdout.reconfigure(encoding='utf-8')
OUT=n.ROOT/'tests/appearance-output'/str(time.time_ns());OUT.mkdir(parents=True)
n.OUT=w.OUT=OUT
n.SERVER=n.ROOT/"build-gui/server/Release/im_server.exe"
OWNER=n.ROOT/'build-gui/Release/workspace_gui_probe.exe'
CLIENT=n.ROOT/'build-gui/Release/client_gui_probe.exe'
check=n.check
class Client(w.Gui):
    def __init__(self,name,nickname,config,legacy=False):
        self.folder=OUT/name;self.folder.mkdir()
        if legacy:
            media=self.folder/'data.txt.media';media.mkdir();Image.open(OUT/'a.png').save(media/'old.png')
            (self.folder/'data.txt.studio.json').write_text(json.dumps(dict(version=1,orders=[],profiles={'10001':dict(avatar='old.png',bubble='peach',frame='rose')})))
        n.Process.__init__(self,[CLIENT],name,dict(directory=str(self.folder),nickname=nickname,password='123456',config=str(config)))
        check(self.event('login',lambda e:e['ok'])['roles']==1,'Ordinary client '+name+' has only one login role')
def main():
    tls=OUT/'tls';tls.mkdir();setup.certificates('127.0.0.1',tls)
    context=ssl.create_default_context(cafile=str(tls/'ca.pem'))
    class TLSRaw(n.Raw):
        def __init__(self,port):self.s=context.wrap_socket(socket.create_connection(('127.0.0.1',port),timeout=10),server_hostname='127.0.0.1');self.s.settimeout(20)
    n.Raw=TLSRaw
    for name,color in [('a.png','#f394ab'),('b.png','#4b9fdc')]:
        image=Image.new('RGB',(240,240),color);draw=ImageDraw.Draw(image);draw.ellipse((60,50,180,170),fill='#fff9d0');image.save(OUT/name)
    seed=OUT/'seed';seed.mkdir();r=subprocess.run([OWNER],input=(json.dumps(dict(directory=str(seed),seed=True))+'\n').encode(),env=n.ENV,capture_output=True,timeout=30);check(r.returncode==0,'Isolated existing-account fixture created')
    port=n.free_port();config=OUT/'server.json';setup.write_json(config,dict(listenAddress='127.0.0.1',port=port,tls=True,allowInsecureLoopback=False,idleExitMs=0,certificateFile=str(tls/'server.pem'),keyFile=str(tls/'server.key'),usersFile=str(OUT/'users.json'),workspaceFile=str(OUT/'workspace.sqlite'),workspaceImport=str(seed/'data.txt')))
    client=OUT/'client.json';setup.write_json(client,dict(host='127.0.0.1',port=port,tls=True,allowInsecureLoopback=False,autoStartLocalServer=False,caFile=str(tls/'ca.pem')))
    old=n.ROOT/'build-package/v5.3.1/runtime/im_server.exe'
    if old.exists():
        current=n.SERVER;n.SERVER=old;legacy=n.start_server(config,'legacy-server');legacy.kill();n.SERVER=current
    server=n.start_server(config,'server')
    if old.exists():
        backup=OUT/'workspace.sqlite.before-appearance-v5.4.sqlite'
        with sqlite3.connect(backup) as db:check(db.execute('PRAGMA integrity_check').fetchone()[0]=='ok' and db.execute('SELECT COUNT(*) FROM state').fetchone()[0]==1,'Upgrade automatically creates intact pre-appearance database backup')
    a=Client('a','张三',client,True);b=Client('b','李四',client)
    state=b.snapshot(lambda s:s.get('appearances',{}).get('10001',{}).get('bubble')=='peach')
    check(bool(state['appearances']['10001']['avatar']) and state['appearances']['10001']['frame']=='none','Legacy local avatar/free bubble migrated without granting unpaid frame')
    check((a.folder/'data.txt.media/old.png').exists(),'Original local avatar retained')
    b.command('avatar',file=str(OUT/'b.png'));a.snapshot(lambda s:bool(s.get('appearances',{}).get('10002',{}).get('avatar')))
    b.command('style',bubble='sun');a.snapshot(lambda s:s.get('appearances',{}).get('10002',{}).get('bubble')=='sun')
    a.command('send',target=10002,text='头像气泡同步：你好，🙂 <>&');b.has('头像气泡同步：你好，🙂 <>&')
    b.command('send',target=10001,text='收到你的头像和气泡');a.has('收到你的头像和气泡')
    a.command('render',target=10002);ra=a.event('render');b.command('render',target=10001);rb=b.event('render')
    check(ra['ok'] and rb['ok'],'Both real ordinary chat windows rendered')
    for sender,style in [(10001,'peach'),(10002,'sun')]:
        check(any(x['sender']==sender and x['style']==style for x in ra['bubbles']) and any(x['sender']==sender and x['style']==style for x in rb['bubbles']),'Same sender bubble shown on both sides: '+style)
        av=[x for x in ra['avatars'] if x['user']==sender and x['custom']];bv=[x for x in rb['avatars'] if x['user']==sender and x['custom']]
        check(any(x['size']==y['size'] and x['pixels']==y['pixels'] for x in av for y in bv),'Actual painted avatar pixels match for user '+str(sender))
    b.rpc('appearance.update',dict(profile={'bubble':'gold'}),False);check(True,'Unpaid premium bubble rejected by server')
    b.rpc('appearance.update',dict(profile={'avatar':'not-base64'}),False);b.rpc('appearance.update',dict(profile={'avatar':'a'*32769}),False);check(True,'Malformed and oversized avatars rejected')
    b.rpc('appearance.update',dict(profile={'tier':4}),False);check(True,'Client cannot grant itself membership through appearance')
    a.rpc('membership.order',dict(tier=2,reference='isolated-appearance-payment'))
    state=a.snapshot(lambda s:bool(s['membershipOrders']));order=state['membershipOrders'][0]['id']
    w.PROBE=OWNER;admin=w.Gui('admin','admin',client,'admin123456',True)
    admin.rpc('admin.membership.review',dict(id=order,approve=True,verified=True,note='isolated-payment-unique-appearance'))
    a.snapshot(lambda s:s['membership']['tier']==2);a.command('style',bubble='candy');premium=b.snapshot(lambda s:s.get('appearances',{}).get('10001',{}).get('bubble')=='candy')
    b.command('render',target=10001);rb=b.event('render');check(any(x['sender']==10001 and x['style']=='candy' for x in rb['bubbles']),'Receiver actually paints approved premium bubble')
    check('membershipOrders' not in premium['appearances']['10001'],'Public appearance exposes no payment records')
    b.rpc('appearance.update',dict(target=10001,senderId=10001,profile={'bubble':'lilac'}));s=a.snapshot(lambda s:s.get('appearances',{}).get('10002',{}).get('bubble')=='lilac');check(s.get('appearances',{}).get('10001',{}).get('bubble')=='candy','Spoofed target cannot change another user appearance')
    b.command('adminAttempt');check(not b.event('adminAttempt')['ok'],'Ordinary build rejects administrator authentication')
    a.rpc('appearance.update',dict(createOnly=True,profile={'bubble':'peach'}));s=b.snapshot(lambda s:s.get('appearances',{}).get('10001',{}).get('bubble')=='candy');check(True,'Legacy import cannot overwrite existing server appearance')
    for gui in [a,b]:gui.events[:]=[e for e in gui.events if e.get('event') not in ('state','snapshot')]
    server.kill();server=n.start_server(config,'restart');a.event('state',lambda e:'消息实时同步' in e['text']);b.event('state',lambda e:'消息实时同步' in e['text'])
    b.snapshot(lambda s:s.get('appearances',{}).get('10001',{}).get('bubble')=='candy' and bool(s.get('appearances',{}).get('10002',{}).get('avatar')));check(True,'Avatar and paid bubble survive server restart and reconnect')
    a.rpc('appearance.update',dict(profile={'avatar':''}));b.snapshot(lambda s:'avatar' not in s['appearances']['10001']);check(True,'Restoring default avatar synchronizes to other client')
    for gui in [a,b,admin]:gui.command('quit');gui.process.wait(20)
    server.kill();db=sqlite3.connect(OUT/'workspace.sqlite');check(db.execute('SELECT COUNT(*) FROM appearances').fetchone()[0]==2,'Public appearance persisted in central SQLite only');db.close()
    r=subprocess.run([n.ROOT/'build-gui/Release/connection_code_checks.exe',OUT/'code-checks',tls/'ca.pem'],env={**n.ENV,'TINGLAN_HOME':str(OUT/'code-checks')},capture_output=True,timeout=30)
    (OUT/'code-checks.log').write_bytes(r.stdout+r.stderr);check(r.returncode==0,'18 connection-code, strict TLS and ordinary-client UI checks passed')
if __name__=='__main__':
    status='passed';error=None
    try:main()
    except Exception as e:status='failed';error=repr(e);print('FAIL',error,flush=True)
    finally:
        for child in n.children:child.kill()
        setup.write_json(OUT/'results.json',dict(status=status,count=len(n.checks),checks=n.checks,error=error,scope='One Windows machine; two real ordinary Qt clients over verified TLS. Two physical PCs unverified.'));print('RESULT',OUT/'results.json',flush=True)
    raise SystemExit(status!='passed')
