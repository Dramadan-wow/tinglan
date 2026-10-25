"""Unified backend integration using two actual Windows Qt UI processes.
All data is isolated. No production account or database is modified.
"""
import base64
import json
import sqlite3
import subprocess
import time
import network_integration as n

n.OUT = n.ROOT / 'tests/workspace-output' / str(time.time_ns())
OUT = n.OUT
PROBE = n.ROOT / 'build-gui/Release/workspace_gui_probe.exe'
check = n.check

class Gui(n.Process):
    def __init__(self, name, nickname, config, password='123456', admin=False):
        self.folder = OUT / name
        self.folder.mkdir()
        super().__init__([PROBE], name, dict(directory=str(self.folder),
            nickname=nickname, password=password, config=str(config), admin=admin))
        self.event('login', lambda e: e['ok'])

    def command(self, action, **args):
        with (self.folder / 'commands.jsonl').open('a', encoding='utf-8') as f:
            f.write(json.dumps(dict(action=action, **args), ensure_ascii=False)+'\n')

    def snapshot(self, predicate):
        return self.event('snapshot', lambda e: predicate(e['data']))['data']

    def has(self, text):
        return self.snapshot(lambda s: any(m['text']==text for m in s['messages']))

    def rpc(self, op, args, ok=True):
        self.events[:] = [e for e in self.events if e.get('event')!='result']
        self.command('rpc', op=op, args=args)
        result = self.event('result', lambda e: e['data'].get('ok')==ok)['data']
        return result

def receive(raw, kind):
    for _ in range(300):
        m=raw.recv()
        if m['type']==kind: return m
    raise AssertionError('Expected '+kind)

def snapshot(raw):
    parts=[]
    while True:
        m=receive(raw,'workspace_sync')['data']
        if m['part']==0: parts=[]
        parts.append(base64.b64decode(m['payload']))
        if m['part']+1==m['count']: return json.loads(b''.join(parts))

def login(port, name, password='123456', register=False, fragmented=False):
    raw=n.Raw(port)
    packet=n.frame(n.message('login',dict(workspace=True,username=name,password=password,service=0,register=register)))
    if fragmented:
        for cut in (packet[:2],packet[2:7],packet[7:]): raw.s.sendall(cut);time.sleep(.02)
    else: raw.s.sendall(packet)
    response=receive(raw,'login_result')
    return raw,response

def main():
    OUT.mkdir(parents=True)
    seed=OUT/'seed'; seed.mkdir()
    initial=json.dumps(dict(directory=str(seed),seed=True))+'\n'
    result=subprocess.run([PROBE],input=initial.encode(),env=n.ENV,capture_output=True,timeout=30)
    check(result.returncode==0,'Create isolated coursework fixture')
    port=n.free_port()
    settings=dict(listenAddress='127.0.0.1',port=port,tls=False,allowInsecureLoopback=True,
        usersFile=str(OUT/'users.json'),workspaceFile=str(OUT/'workspace.sqlite'),workspaceImport=str(seed/'data.txt'))
    config=OUT/'server.json';config.write_text(json.dumps(settings),encoding='utf-8')
    client=OUT/'client.json';client.write_text(json.dumps(dict(host='127.0.0.1',port=port,tls=False,allowInsecureLoopback=True)),encoding='utf-8')
    server=n.start_server(config,'server')
    for service in range(3,7):
        raw=n.Raw(port)
        raw.send(n.message('login',dict(workspace=True,username='下线服务测试',password='test-retired-password',service=service,register=True)))
        check(receive(raw,'error')['data']['code']=='invalid_login',f'Retired service {service} cannot register through protocol')
        raw.close()
    a=Gui('client-a','张三',client);b=Gui('client-b','李四',client)
    for service in range(3,7):
        check(not a.rpc('account.open',dict(contact=service,number='90000',password='test-retired-password'),ok=False)['ok'],f'Retired service {service} cannot be opened by RPC')
    check(True,'Two actual login dialogs and normal Windows chat windows authenticated')
    a.command('pulse');a.event('pulse');check(True,'GUI event loop responsive')
    text='你好，听澜！🙂 <>& "引号" \\'
    a.command('send',target=10002,text=text)
    state=b.has(text)
    check(any(m['text']==text and m['sender']==10001 for m in state['messages']),'Ordinary private composer A -> B through server')
    b.command('send',target=10001,text='收到，服务器回复！')
    a.has('收到，服务器回复！');check(True,'Ordinary private composer B -> A')
    a.command('send',kind=1,target=1001,text='群里见，大家好')
    b.has('群里见，大家好');check(True,'Ordinary group composer delivers to another group member')
    a.command('send',target=10002,text='长消息'*400)
    b.has('长消息'*400);check(True,'Long UTF-8 message delivered through normal composer')
    a.command('post',text='第一条公开动态\n山水有相逢 🙂')
    s=b.snapshot(lambda s:any(p['text'].startswith('第一条公开动态') for p in s['feed']))
    post=next(p['id'] for p in s['feed'] if p['text'].startswith('第一条公开动态'))
    b.command('post',text='另一个人的生活分享')
    a.snapshot(lambda s:any(p['author']==10002 for p in s['feed']))
    check(True,'Shared feed contains posts by different users, including multiline text')
    b.rpc('feed.like',dict(post=post,enabled=True))
    a.snapshot(lambda s:any(p['id']==post and p['likes']==1 for p in s['feed']))
    b.rpc('feed.comment',dict(post=post,text='一起看风景'))
    a.snapshot(lambda s:any(p['id']==post and p['comments'] for p in s['feed']))
    check(True,'Cross-client likes and comments synchronized')
    b.rpc('feed.delete',dict(post=post),False)
    check(True,'Cannot delete another user post')
    a.command('rename',nick='听澜测试新昵称')
    s=b.snapshot(lambda s:any(u['id']==10001 and u['name']=='听澜测试新昵称' for u in s['users']))
    check(all(x['nick']=='听澜测试新昵称' for u in s['users'] if u['id']==10001 for x in u['services']),'Login nickname and every service nickname synchronized')
    a.rpc('account.edit',dict(nick='李四',old='',password=''),False)
    a.rpc('sync',{})
    a.snapshot(lambda s:any(u['id']==10001 and u['name']=='听澜测试新昵称' for u in s['users']))
    check(True,'Duplicate nickname rejected without losing prior nickname')
    check('adminHash' not in s and all(not x['password'] for u in s['users'] for x in u['services']),'Snapshots contain no user password hashes or administrator credential')
    a.command('send',target=10002,text='改名已保存，我们继续聊天。');b.has('改名已保存，我们继续聊天。')
    a.command('shot',page=0,name='chat.png');a.event('screenshot',lambda e:e['ok'])
    b.command('shot',page=6,name='feed.png');b.event('screenshot',lambda e:e['ok'])
    check(True,'Captured ordinary chat and feed UI screenshots')
    a.command('quit');a.process.wait(timeout=20)
    a2=Gui('client-a-renamed','听澜测试新昵称',client)
    check(True,'New client logs in using saved new nickname')
    a2.rpc('account.edit',dict(nick='不应保存的昵称',old='wrong-password',password='test-new-password-2026'),False)
    a2.rpc('account.edit',dict(nick='听澜测试新昵称',old='123456',password='test-new-password-2026'))
    check(True,'Wrong old password rejected; successful password update acknowledged')
    raw,response=login(port,'张三');check(not response['data']['ok'],'Old login nickname no longer authenticates');raw.close()
    b.command('quit');b.process.wait(timeout=20)
    a2.command('send',target=10002,text='离线后仍能看到这条')
    offline=a2.has('离线后仍能看到这条')
    check(any(u['id']==10002 and not u['online'] for u in offline['users']),'Disconnected user presence updated')
    b2=Gui('client-b-return','李四',client);b2.has('离线后仍能看到这条')
    check(True,'Offline private message persists and is loaded at next login')
    # Third user uses raw protocol to test authoritative identity, replay and permissions.
    raw,response=login(port,'赵六',fragmented=True)
    check(response['data']['ok'],'Fragmented login frame accepted')
    snap=snapshot(raw)
    check(not any(m['text']==text for m in snap['messages']),'Unrelated user cannot read another private conversation')
    sender=response['data']['userId']
    forged=n.message('workspace_command',dict(op='feed.publish',text='伪造',service=0),sender='u10001')
    raw.send(forged);check(receive(raw,'error')['data']['code']=='sender_mismatch','Forged sender rejected')
    invalid=n.message('workspace_command',dict(op='chat.send',kind=1,target=1001,text='非法入群',service=0),sender=sender)
    raw.send(invalid);check(not receive(raw,'workspace_result')['data']['ok'],'Nonmember group send rejected')
    replay=n.message('workspace_command',dict(op='feed.publish',text='只发布一次',channel='weibo',service=0),sender=sender)
    raw.s.sendall(n.frame(replay)+n.frame(replay))
    first=receive(raw,'workspace_result')['data'];second=receive(raw,'workspace_result')['data']
    check(first['ok'] and second['ok'] and first['post']==second['post'],'Coalesced frames and durable operation deduplication')
    replay['data']['text']='冲突内容';raw.send(replay)
    check(not receive(raw,'workspace_result')['data']['ok'],'Reused request ID with changed content rejected')
    raw.close()
    b2.rpc('account.open',dict(contact=2,number='10002',password='test-weibo-password-2026',shared=True))
    a2.rpc('account.edit',dict(service=1,nick='听澜测试新昵称',old='123456',password='test-wechat-password-2026'))
    a2.command('service',value=1);a2.event('service',lambda e:e['ok'])
    # Reconnect active GUI processes after server restart; all data must survive.
    server.kill()
    a2.event('state',lambda e:'重连' in e['text']);b2.event('state',lambda e:'重连' in e['text'])
    a2.events.clear();b2.events.clear()
    a2.command('pulse');a2.event('pulse')
    server=n.start_server(config,'server-restart')
    a2.event('state',lambda e:'消息实时同步' in e['text']);b2.event('state',lambda e:'消息实时同步' in e['text'])
    a2.snapshot(lambda s:any(m['text']=='离线后仍能看到这条' for m in s['messages']))
    b2.snapshot(lambda s:any(p['text']=='只发布一次' for p in s['weibo']))
    check(True,'Automatic reconnect after server restart restores messages, feed and renamed account')
    a2.command('service',value=0);a2.event('service',lambda e:e['ok'])
    check(True,'Switching services and changing another service password preserve reconnect credentials')
    a2.command('send',target=10002,text='重连之后继续聊天');b2.has('重连之后继续聊天')
    check(True,'Ordinary conversation continues after automatic reconnect')
    raw,response=login(port,'赵六');snapshot(raw);replay['data']['text']='只发布一次';raw.send(replay)
    check(receive(raw,'workspace_result')['data']['post']==first['post'],'Replay deduplication survives server restart');raw.close()
    db=sqlite3.connect(OUT/'workspace.sqlite')
    saved=json.loads(db.execute('SELECT payload FROM state').fetchone()[0])
    check(all(x['password'].startswith('pbkdf2-sha256:') for u in saved['users'] for x in u['services']),'SQLite stores only salted password hashes')
    check(db.execute("SELECT COUNT(*) FROM posts WHERE text='只发布一次'").fetchone()[0]==1,'Duplicate request produces exactly one persisted post')
    db.close()
    # Registration uses the same directory and login identity as ordinary clients.
    raw,response=login(port,'新注册测试账号','test-only-password-2026',register=True)
    check(response['data']['ok'],'Server registration creates a usable platform account');snapshot(raw);raw.close()
    time.sleep(.2)
    raw,response=login(port,'新注册测试账号','test-only-password-2026')
    check(response['data']['ok'],'Registered account authenticates on a new connection');raw.close()
    for gui in (a2,b2):gui.command('quit');gui.process.wait(timeout=20)
    raw,response=login(port,'听澜测试新昵称','test-new-password-2026')
    check(response['data']['ok'],'Changed nickname and password survive server restart');raw.close()

if __name__=='__main__':
    status,error='passed',None
    try:main()
    except Exception as e:
        status,error='failed',repr(e);print('FAIL',error,flush=True)
    finally:
        for child in n.children:child.kill()
        OUT.mkdir(parents=True,exist_ok=True)
        (OUT/'results.json').write_text(json.dumps(dict(status=status,count=len(n.checks),checks=n.checks,error=error,
            environment='Windows local server plus independent Windows GUI clients; Linux/LAN/cloud unverified'),ensure_ascii=False,indent=2),encoding='utf-8')
        print('RESULT',OUT/'results.json',flush=True)
    raise SystemExit(status!='passed')

