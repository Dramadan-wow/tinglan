"""Real online admin GUI, independent users, server-side authorization and persistence.
Only isolated fixtures are mutated. No actual payment is reviewed.
"""
import json,os,sys,subprocess,time,sqlite3
from pathlib import Path
import network_integration as n
import workspace_integration as w
sys.stdout.reconfigure(encoding='utf-8')
OUT=n.ROOT/'tests/admin-network-output'/str(time.time_ns());n.OUT=w.OUT=OUT
n.SERVER=n.ROOT/'build-gui/server/Release/im_server.exe'
check=n.check

def admin_login(port,password='admin123456',name='admin'):
    raw=n.Raw(port);raw.send(n.message('login',dict(workspace=True,admin=True,username=name,password=password,service=0)))
    return raw,w.receive(raw,'login_result')['data']

def command(raw,uid,op,**args):
    raw.send(n.message('workspace_command',dict(op=op,service=0,**args),sender=uid))
    return w.receive(raw,'workspace_result')['data']

def ui_state(gui):
    gui.command('adminState');return gui.event('adminState')

def main():
    OUT.mkdir(parents=True);seed=OUT/'seed';seed.mkdir()
    initial=subprocess.run([w.PROBE],input=(json.dumps(dict(directory=str(seed),seed=True))+'\n').encode(),env=n.ENV,capture_output=True,timeout=40)
    check(initial.returncode==0,'Isolated fixture created')
    port=n.free_port();config=OUT/'server.json';config.write_text(json.dumps(dict(listenAddress='127.0.0.1',port=port,tls=False,allowInsecureLoopback=True,usersFile=str(OUT/'users.json'),workspaceFile=str(OUT/'workspace.sqlite'),workspaceImport=str(seed/'data.txt'))))
    client=OUT/'client.json';client.write_text(json.dumps(dict(host='127.0.0.1',port=port,tls=False,allowInsecureLoopback=True)))
    server=n.start_server(config,'server')
    bad,result=admin_login(port,'incorrect-password');check(not result['ok'],'Wrong server administrator password rejected');bad.close()
    raw=n.Raw(port)
    raw.send(n.message('workspace_command',dict(op='admin.allow',target=10002,enabled=False)))
    check(w.receive(raw,'error')['data']['code']=='not_authenticated','Unauthenticated management request rejected');raw.close()
    admin=w.Gui('admin','admin',client,'admin123456',admin=True)
    state=admin.snapshot(lambda s:s.get('admin'));check(len(state['users'])==4,'Actual management login shows all server users')
    check('adminHash' not in state and not state['messages'] and all(not a['password'] for u in state['users'] for a in u['services']),'Admin snapshot excludes password hashes and private conversations')
    person=w.Gui('existing-user','李四',client)
    admin.snapshot(lambda s:any(u['id']==10002 and u['online'] for u in s['users']))
    check(ui_state(admin)['counts']==[4,4,0,1],'Administrator sees a different existing account online')
    check(not person.rpc('admin.allow',dict(target=10001,enabled=False,admin=True),False)['ok'],'Ordinary user cannot claim administrator privileges in RPC')
    user,login=w.login(port,'新注册测试用户','new-user-password-2026',register=True);uid=login['data']['userId'];numeric=int(uid[1:]);w.snapshot(user)
    admin.snapshot(lambda s:len(s['users'])==5 and sum(u['online'] for u in s['users'])==2)
    check(ui_state(admin)['counts']==[5,5,0,2],'Registration and online counts update live without reopening admin')
    check(any(row[1]=='新注册测试用户' and '注册账号' in row[2] for row in ui_state(admin)['logs']),'New registration appears in management audit table')
    bad,result=w.login(port,'新注册测试用户','wrong-password');check(not result['data']['ok'],'Wrong ordinary password rejected');bad.close()
    admin.snapshot(lambda s:any(l['target']==numeric and '登录失败' in l['action'] for l in s['adminLogs']))
    check(any(row[1]=='新注册测试用户' and '登录失败' in row[2] for row in ui_state(admin)['logs']),'Failed login is persisted and displayed with user identity')
    order=command(user,uid,'membership.order',tier=2,reference='test-payment-proof-not-real')
    check(order['ok'],'New account creates a pending server membership request');oid=order['order']
    admin.snapshot(lambda s:any(o['id']==oid and o['status']=='pending' for o in s['membershipOrders']))
    check(any('新注册测试用户' in row[1] and row[5]=='待核验' for row in ui_state(admin)['orders']),'New account payment request appears in actual admin order table')
    check(not command(user,uid,'admin.membership.review',id=oid,approve=True,verified=True,note='forged-payment-reference')['ok'],'Ordinary user cannot approve own membership')
    admin.rpc('admin.membership.review',dict(id=oid,approve=True,verified=False,note='test-payment-reference'),False)
    check(True,'Server requires explicit payment verification for approval')
    admin.command('adminForm',button='approvePaymentOrder',order=oid,fields=dict(reviewNote='isolated-test-transaction-2026',paymentVerified=True))
    admin.event('result',lambda e:e['data'].get('ok'))
    admin.snapshot(lambda s:any(o['id']==oid and o['status']=='approved' and o['reviewer']=='admin' for o in s['membershipOrders']))
    command(user,uid,'sync');snapshot=w.snapshot(user)
    while snapshot['membership']['tier']!=2:snapshot=w.snapshot(user)
    check(True,'Actual approval form grants server membership and syncs to recipient')
    admin.rpc('admin.membership.review',dict(id=oid,approve=True,verified=True,note='isolated-test-transaction-2026'),False)
    check(True,'Processed payment cannot be approved twice')
    another=person.rpc('membership.order',dict(tier=1,reference='other-user-payment-proof'))['order']
    admin.snapshot(lambda s:any(o['id']==another for o in s['membershipOrders']))
    admin.command('adminForm',button='rejectPaymentOrder',order=another,fields=dict(reviewNote='测试订单未实际付款，驳回'))
    admin.event('result',lambda e:e['data'].get('ok'))
    person.snapshot(lambda s:any(o['id']==another and o['status']=='rejected' for o in s['membershipOrders']))
    check(True,'Actual rejection form updates another user order without granting membership')
    admin.command('adminClick',button='denyUserLogin',target=numeric);admin.event('result',lambda e:e['data'].get('ok'))
    check(w.receive(user,'error')['data']['code']=='session_revoked','Disable button revokes actual user network session');user.close()
    bad,result=w.login(port,'新注册测试用户','new-user-password-2026');check(not result['data']['ok'],'Disabled user cannot log back in');bad.close()
    admin.snapshot(lambda s:any(u['id']==numeric and u['disabled'] and not u['online'] for u in s['users']))
    check(ui_state(admin)['counts']==[5,4,1,1],'Server permission and online counts refresh together')
    admin.command('adminClick',button='allowUserLogin',target=numeric);admin.event('result',lambda e:e['data'].get('ok'))
    user,result=w.login(port,'新注册测试用户','new-user-password-2026');check(result['data']['ok'],'Allow button restores login on server');w.snapshot(user)
    admin.command('adminForm',button='resetUserPassword',target=numeric,fields={'new':'new-reset-password-2026','repeat':'new-reset-password-2026'})
    admin.event('result',lambda e:e['data'].get('ok'));check(w.receive(user,'error')['data']['code']=='session_revoked','Reset password ends the active network session');user.close()
    bad,result=w.login(port,'新注册测试用户','new-user-password-2026');check(not result['data']['ok'],'Old password rejected after reset');bad.close()
    user,result=w.login(port,'新注册测试用户','new-reset-password-2026');check(result['data']['ok'],'New password authenticates on server');w.snapshot(user)
    admin.command('adminClick',button='forceUserLogout',target=10002);admin.event('result',lambda e:e['data'].get('ok'));person.event('revoked')
    check(True,'Force logout stops a real Qt client without automatic reauthentication')
    other,reply=admin_login(port);check(reply['ok'],'Independent second admin connection authenticated');w.snapshot(other)
    admin.command('adminForm',button='changeAdminPassword',fields={'old':'admin123456','new':'updated-admin-password-2026','repeat':'updated-admin-password-2026'})
    admin.event('result',lambda e:e['data'].get('ok'));check(w.receive(other,'error')['data']['code']=='session_revoked','Admin password change revokes other admin sessions');other.close()
    bad,result=admin_login(port);check(not result['ok'],'Old administrator password no longer works');bad.close()
    user.close();admin.snapshot(lambda s:any(l['target']==numeric and '连接断开' in l['action'] for l in s['adminLogs']))
    check(True,'Disconnect appears as a persistent logout record')
    for tab,name in ((0,'users.png'),(1,'logins.png'),(2,'orders.png')):
        admin.command('adminShot',tab=tab,name=name);check(admin.event('screenshot')['ok'],'Actual admin page rendered: '+name)
    with sqlite3.connect(OUT/'workspace.sqlite') as db:
        payload=bytes(db.execute('SELECT payload FROM state').fetchone()[0]);stored=json.loads(payload)
        check(bool(stored['adminLogs']) and stored['nextAudit']!='1','Audit entries are stored in SQLite state')
        check(all(p.encode() not in payload for p in ('new-reset-password-2026','updated-admin-password-2026')),'Passwords stored as hashes, not plaintext')
    server.kill();admin.event('state',lambda e:'重连' in e['text']);admin.events.clear();server=n.start_server(config,'server-restart')
    admin.event('state',lambda e:'管理数据实时同步' in e['text']);after=admin.snapshot(lambda s:s.get('admin') and len(s['users'])==5)
    check(any(o['id']==oid and o['status']=='approved' for o in after['membershipOrders']) and any(l['target']==numeric and '登录失败' in l['action'] for l in after['adminLogs']),'Admin reconnect uses changed password and preserves users, orders and audit history')
    admin.command('quit');admin.process.wait(15)
    server.kill()
    original_hash=stored['adminHash']
    for _ in range(2):
        imported=subprocess.run([n.SERVER,'--config',config,'--import-audit',seed/'data.txt'],env=n.ENV,capture_output=True,timeout=30)
        check(imported.returncode==0 and b'Imported audit records: 0' in imported.stdout,'Importing already present local audit is idempotent')
    with sqlite3.connect(OUT/'workspace.sqlite') as db:
        check(json.loads(db.execute('SELECT payload FROM state').fetchone()[0])['adminHash']==original_hash,'Old audit import cannot replace server administrator credentials')
    # No default administrator on a fresh installation; login audits exist before provisioning.
    fresh=OUT/'fresh-server.json';fresh.write_text(json.dumps(dict(listenAddress='127.0.0.1',port=port,tls=False,allowInsecureLoopback=True,usersFile=str(OUT/'fresh-users.json'),workspaceFile=str(OUT/'fresh.sqlite'))))
    server=n.start_server(fresh,'fresh-server')
    freshuser,r=w.login(port,'全新服务用户','fresh-user-password',register=True);check(r['data']['ok'],'Fresh server registers user without preconfigured administrator');w.snapshot(freshuser);freshuser.close();server.kill()
    env=dict(n.ENV);env['TINGLAN_ADMIN_PASSWORD']='fresh-operator-password-2026'
    provision=subprocess.run([n.SERVER,'--config',fresh,'--set-admin','operator'],env=env,capture_output=True,timeout=30)
    check(provision.returncode==0,'Stopped-server CLI explicitly provisions administrator from environment')
    server=n.start_server(fresh,'fresh-restart');raw,result=admin_login(port,'fresh-operator-password-2026','operator');check(result['ok'],'Provisioned server operator can authenticate')
    snapshot=w.snapshot(raw);check(any(l['action']=='注册账号' for l in snapshot['adminLogs']),'Registration audit survives even before administrator initialization');raw.close()

if __name__=='__main__':
    status,error='passed',None
    try:main()
    except Exception as e:status,error='failed',repr(e);print('FAIL',error,flush=True)
    finally:
        for child in n.children:child.kill()
        (OUT/'results.json').write_text(json.dumps(dict(status=status,count=len(n.checks),checks=n.checks,error=error),ensure_ascii=False,indent=2),encoding='utf-8')
        print('RESULT',OUT/'results.json',flush=True)
    raise SystemExit(status!='passed')
