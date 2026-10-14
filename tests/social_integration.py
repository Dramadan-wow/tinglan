"""Version 5: real GUI posting/images/stranger DM/membership; isolated data only."""
import json,os,sys,subprocess,time,sqlite3
from pathlib import Path
sys.stdout.reconfigure(encoding='utf-8')
import network_integration as n
import workspace_integration as w
n.OUT=n.ROOT/'tests/social-output'/str(time.time_ns());w.OUT=n.OUT;OUT=n.OUT
n.SERVER=n.ROOT/'build-gui/server/Release/im_server.exe'
from PIL import Image,ImageDraw
check=n.check
def main():
    OUT.mkdir(parents=True);seed=OUT/'seed';seed.mkdir()
    r=subprocess.run([w.PROBE],input=(json.dumps(dict(directory=str(seed),seed=True))+'\n').encode(),env=n.ENV,capture_output=True,timeout=40)
    check(r.returncode==0,'Isolated seed created');port=n.free_port()
    config=OUT/'server.json';config.write_text(json.dumps(dict(listenAddress='127.0.0.1',port=port,tls=False,allowInsecureLoopback=True,usersFile=str(OUT/'users.json'),workspaceFile=str(OUT/'workspace.sqlite'),workspaceImport=str(seed/'data.txt'))))
    client=OUT/'client.json';client.write_text(json.dumps(dict(host='127.0.0.1',port=port,tls=False,allowInsecureLoopback=True)))
    server=n.start_server(config,'server')
    friend,fr=w.login(port,'张三');w.snapshot(friend)
    b=w.Gui('author','李四',client);c=w.Gui('stranger','赵六',client)
    check(True,'Two GUI users who are not friends authenticated')
    photo=OUT/'photo.png';im=Image.new('RGB',(640,420),'#75b8d2');d=ImageDraw.Draw(im);d.polygon([(0,420),(260,80),(500,420)],fill='#295b63');d.ellipse((430,35,540,145),fill='#fff2c1');im.save(photo)
    b.command('post',channel='moments',text='仅好友可见的照片',image=str(photo))
    own=b.snapshot(lambda s:any(p['text']=='仅好友可见的照片' for p in s['feed']));moment=next(p for p in own['feed'] if p['text']=='仅好友可见的照片')
    check(bool(moment['image']),'GUI photo selection, compression, upload and persisted image')
    friend.send(n.message('workspace_command',dict(op='sync',service=0),sender=fr['data']['userId']))
    w.receive(friend,"workspace_result");fs=w.snapshot(friend);check(any(p['id']==moment['id'] for p in fs['feed']),'Friend receives image moment')
    c.rpc('sync',{});snap=c.snapshot(lambda s:len(s['users'])==4)
    check(not any(p['id']==moment['id'] for p in snap['feed']),'Nonfriend cannot receive private moment or image')
    c.rpc('feed.comment',dict(post=moment['id'],text='越权评论'),False);check(True,'Nonfriend cannot guess ID to comment')
    b.command('workspaceState');locked=b.event('workspaceState');check(not locked['weiboAvailable'] and not locked['weiboVisible'] and not locked['inboxVisible'],'Unregistered Weibo service has no workspace or dropdown entry')
    b.rpc('feed.publish',dict(channel='weibo',text='未开通的发帖'),False)
    b.rpc('weibo.follow',dict(target=10004,enabled=True),False)
    b.rpc('weibo.dm.send',dict(target=10004,text='未开通的私信'),False)
    check(True,'Server rejects unregistered Weibo publishing, follow and private messages')
    b.command('openWeibo');b.event('result',lambda e:e['data'].get('ok'))
    b.snapshot(lambda s:any(u['id']==10002 and any(a['type']==2 for a in u['services']) for u in s['users']))
    check(True,'Real account opening form registers Weibo without changing user identity')
    b.command('workspaceState');check(not b.event('workspaceState')['weiboVisible'],'Opening service alone does not force entry from QQ')
    for gui in (b,c):
        gui.command('service',value=2);gui.event('service',lambda e:e['ok']);gui.command('workspaceState');state=gui.event('workspaceState');check(state['page']==8 and state['weiboVisible'] and state['inboxVisible'],'Bottom service switch opens independent Weibo workspace')
    b.command('post',channel='weibo',text='#城市散步# 今天的风景很漂亮，欢迎一起聊聊。',image=str(photo))
    cs=c.snapshot(lambda s:any('城市散步' in p['text'] for p in s['weibo']));post=next(p for p in cs['weibo'] if '城市散步' in p['text'])
    check(bool(post['image']),'Public Weibo image visible to nonfriend')
    c.rpc('weibo.follow',dict(target=10002,enabled=True));c.snapshot(lambda s:10002 in s['follows']);check(True,'Follow author without adding friend')
    c.command('dm',target=10002,fromPost=True,text='你好，我从你的微博照片过来，想问问拍摄地点。')
    b.snapshot(lambda s:any('拍摄地点' in m['text'] and m['sender']==10004 for m in s['directMessages']));check(True,'Actual post private-message button sends stranger DM')
    b.command('dm',target=10004,text='在城市公园拍的，很高兴认识你。')
    c.snapshot(lambda s:any('城市公园' in m['text'] for m in s['directMessages']));check(True,'Actual message inbox replies without friend relationship')
    friend.send(n.message('workspace_command',dict(op='sync',service=0),sender=fr['data']['userId']));fs=w.snapshot(friend)
    check(not fs['directMessages'],'Third user cannot read private messages')
    b.rpc('weibo.block',dict(target=10004,enabled=True));c.rpc('weibo.dm.send',dict(target=10002,post=post['id'],text='屏蔽后的消息'),False);check(True,'Server enforces DM block')
    b.rpc('weibo.block',dict(target=10004,enabled=False))
    b.command('style',button='theme_ocean');check(b.event('style')['theme']=='ocean','Actual online theme persisted');check(not any(e.get('event')=='failure' and '本机数据操作' in e.get('text','') for e in b.events),'Online appearance button bypasses obsolete local-business rejection')
    b.command('order',tier=2,reference='test-payment-reference-2026')
    orderstate=b.snapshot(lambda s:any(o['status']=='pending' for o in s['membershipOrders']));order=orderstate['membershipOrders'][0]
    check(orderstate['membership']['tier']==0 and order['price']==6,'GUI payment request saved on server without self-granting membership')
    c.rpc('membership.cancel',dict(id=order['id']),False);b.rpc('membership.review',dict(id=order['id'],approve=True),False);check(True,'Cannot cancel another user order or self-approve membership')
    b.command('service',value=0);b.event('service');b.command('workspaceState');check(not b.event('workspaceState')['weiboVisible'],'Switching back to QQ leaves Weibo workspace')
    b.command('shot',page=6,name='moments.png');b.event('screenshot')
    c.command('shot',page=8,tab=0,name='weibo.png');c.event('screenshot')
    friend.close();server.kill();b.event('state',lambda e:'重连' in e['text']);c.event('state',lambda e:'重连' in e['text']);b.events.clear();c.events.clear()
    reviewed=subprocess.run([n.SERVER,'--config',config,'--review-order',order['id'],'--approve','--note','test-verified-transaction-unique-2026'],env=n.ENV,capture_output=True,timeout=30)
    check(reviewed.returncode==0,'Authorized local server operator confirms payment')
    server=n.start_server(config,'server-restart');b.event('state',lambda e:'消息实时同步' in e['text']);c.event('state',lambda e:'消息实时同步' in e['text'])
    active=b.snapshot(lambda s:s['membership']['tier']==2)
    check(active['membership']['expires']>time.time()+29*86400,'Approved tier and 30-day validity synchronized after restart')
    b.command('style',button='bubble_candy');check(b.event('style')['bubble']=='candy','Member-only bubble unlocked and persisted after approval')
    c.snapshot(lambda s:any('城市公园' in m['text'] for m in s['directMessages']) and 10002 in s['follows']);check(True,'Images, private messages and follows survive restart')
    b.command('shot',page=7,name='membership.png');b.event('screenshot')
    b.rpc('account.close',dict(service=2));b.snapshot(lambda s:not s['weibo'] and not s['directMessages']);c.rpc('weibo.dm.send',dict(target=10002,post=post['id'],text='服务已关闭'),False)
    check(True,'Closing Weibo revokes social access without deleting posts or private records')
    b.rpc('account.open',dict(contact=2,number='10002',password='123456',shared=True));b.snapshot(lambda s:any('城市散步' in p['text'] for p in s['weibo']) and len(s['directMessages'])==2)
    check(True,'Reopening Weibo restores previous posts and private history')
    b.command('quit');c.command('quit');b.process.wait(timeout=20);c.process.wait(timeout=20);server.kill()
    duplicate=subprocess.run([n.SERVER,'--config',config,'--review-order',order['id'],'--approve','--note','test-verified-transaction-unique-2026'],env=n.ENV,capture_output=True,timeout=30)
    check(duplicate.returncode!=0,'Processed membership order cannot be approved twice')
    db=sqlite3.connect(OUT/'workspace.sqlite');check(db.execute('SELECT COUNT(*) FROM direct_messages').fetchone()[0]==2,'Only the two authorized DMs were persisted');db.close()
if __name__=='__main__':
    status,error='passed',None
    try:main()
    except Exception as e:status,error='failed',repr(e);print('FAIL',error,flush=True)
    finally:
        for child in n.children:child.kill()
        OUT.mkdir(parents=True,exist_ok=True);(OUT/'results.json').write_text(json.dumps(dict(status=status,count=len(n.checks),checks=n.checks,error=error),ensure_ascii=False,indent=2),encoding='utf-8');print('RESULT',OUT/'results.json',flush=True)
    raise SystemExit(status!='passed')



