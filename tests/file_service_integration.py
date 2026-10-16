"""Authenticated real TLS chat + HTTPS file API; isolated databases, keys and fixtures."""
from pathlib import Path
import hashlib,http.client,json,os,socket,ssl,subprocess,sys,time,uuid,zipfile
from http.client import HTTPSConnection
import network_integration as n
import workspace_integration as w
sys.path.insert(0,str(n.ROOT/'scripts'))
import prepare_lan as setup
sys.stdout.reconfigure(encoding='utf-8')
CHUNK=1024*1024
OUT=n.ROOT/'tests/file-output'/('service-'+str(time.time_ns()));n.OUT=w.OUT=OUT
n.SERVER=n.ROOT/'build-gui/server/Release/im_server.exe'
checks=[];peers=[];last_beat=0
def check(value,label):
    if not value:raise AssertionError(label)
    checks.append(label);print('PASS',label,flush=True)
class Peer:
    def __init__(self,name):
        self.raw=n.Raw.__new__(n.Raw);self.raw.s=context.wrap_socket(socket.create_connection(('127.0.0.1',port)),server_hostname='127.0.0.1');self.raw.s.settimeout(20)
        self.raw.send(n.message('login',dict(workspace=True,username=name,password='123456',service=0,fileTransfer=1)))
        login=w.receive(self.raw,'login_result')['data'];assert login['ok'],login;self.token=login['files']['token'];self.user=login['userId'];self.alive=True;peers.append(self)
        w.snapshot(self.raw)
    def command(self,op,args):
        self.raw.send(n.message('workspace_command',dict(op=op,service=0,**args),sender=self.user));return w.receive(self.raw,'workspace_result')['data']
    def close(self):self.raw.close();self.alive=False
def heartbeat():
    global last_beat
    if time.monotonic()-last_beat<10:return
    last_beat=time.monotonic()
    for p in peers:
        if p.alive:p.raw.send(n.message('heartbeat',sender=p.user));w.receive(p.raw,'heartbeat')
def http(peer,method,path,body=b'',headers=None):
    heartbeat();conn=HTTPSConnection('127.0.0.1',file_port,context=context,timeout=30)
    h={'Authorization':'Bearer '+(peer.token if peer else 'invalid'),'Content-Length':str(len(body))};h.update(headers or {})
    try:
        conn.request(method,path,body,h);response=conn.getresponse();data=response.read();return response.status,dict(response.getheaders()),data
    finally:conn.close()
def api(peer,method,path,data=None,expect=200):
    status,_,body=http(peer,method,path,json.dumps(data,ensure_ascii=False).encode('utf-8') if data is not None else b'',{'Content-Type':'application/json'})
    result=json.loads(body);assert status==expect,(method,path,status,result);return result
def create(peer,path,**extra):
    with path.open('rb') as f:hash=hashlib.file_digest(f,'sha256').hexdigest()
    return api(peer,'POST','/v1/uploads',dict(clientId=str(uuid.uuid4()),name=path.name,size=path.stat().st_size,sha256=hash,recipient=10002,service=0,**extra),201)
def put(peer,record,body,offset):
    status,_,data=http(peer,'PUT','/v1/uploads/'+record['uploadId'],body,{'Upload-Offset':str(offset),'X-Chunk-SHA256':hashlib.sha256(body).hexdigest()})
    assert status==200,(status,data);return json.loads(data)
def complete(peer,r):
    status,_,data=http(peer,'POST','/v1/uploads/'+r['uploadId']+'/complete');assert status in (200,202),(status,data)
    return n.wait_for(lambda:(lambda x:x if x['state'] in ('ready','failed') else None)(api(peer,'GET','/v1/uploads/'+r['uploadId'])),timeout=30)
def transfer(peer,path,chat=False):
    r=create(peer,path);offset=0;delays=[]
    with path.open('rb') as f:
        while block:=f.read(CHUNK):
            r=put(peer,r,block,offset);offset+=len(block)
            if chat and offset%(8*CHUNK)==0:
                started=time.monotonic();reply=peer.command('chat.send',dict(kind=0,target=10002,text='文件上传期间聊天 '+str(offset)));assert reply['ok'];delays.append(time.monotonic()-started)
    r=complete(peer,r);assert r['state']=='ready',r
    if chat:check(delays and max(delays)<3,'100MB upload keeps chat acknowledgments below 3 seconds; max='+str(round(max(delays),3)))
    return r
def download(peer,r,path,start=0):
    with path.open('ab' if start else 'wb') as f:
        offset=start
        while offset<r['size']:
            end=min(offset+CHUNK,r['size'])-1;status,h,body=http(peer,'GET','/v1/files/'+r['fileId'],headers={'Range':f'bytes={offset}-{end}','If-Range':'"'+r['sha256']+'"'})
            assert status==206 and h['Content-Range']==f'bytes {offset}-{end}/{r["size"]}' and len(body)==end-offset+1,(status,h);f.write(body);offset+=len(body)
    with path.open('rb') as f:check(hashlib.file_digest(f,'sha256').hexdigest()==r['sha256'],'Downloaded SHA-256 matches: '+r['name'])
def main():
    global context,port,file_port,last_beat
    OUT.mkdir(parents=True);seed=OUT/'seed';seed.mkdir();result=subprocess.run([w.PROBE],input=(json.dumps(dict(directory=str(seed),seed=True))+'\n').encode(),env=n.ENV,capture_output=True,timeout=30);assert result.returncode==0
    tls=OUT/'tls';tls.mkdir();setup.certificates('127.0.0.1',tls);context=ssl.create_default_context(cafile=str(tls/'ca.pem'));port=n.free_port();file_port=n.free_port()
    config=OUT/'server.json';settings=dict(listenAddress='127.0.0.1',port=port,tls=True,certificateFile=str(tls/'server.pem'),keyFile=str(tls/'server.key'),usersFile=str(OUT/'users.json'),workspaceFile=str(OUT/'workspace.sqlite'),workspaceImport=str(seed/'data.txt'),fileServiceEnabled=True,filePort=file_port,fileStorageDirectory=str(OUT/'storage'),maxFileMiB=512,fileUserQuotaMiB=512,fileQuotaMiB=1024)
    setup.write_json(config,settings);server=n.start_server(config,'server');a=Peer('张三');b=Peer('李四');stranger=Peer('赵六')
    small=OUT/'中文 文件说明.txt';small.write_bytes(('听澜文件传输\n'.encode()*100)[:1024]);r=transfer(a,small);check(r['size']==1024 and r['name']==small.name,'1KB Chinese filename upload and durable completion')
    download(b,r,OUT/'small-received.txt')
    status,_,_=http(stranger,'GET','/v1/files/'+r['fileId']);check(status==403,'Unrelated authenticated account cannot download guessed file ID')
    status,_,_=http(None,'GET','/v1/files/'+r['fileId']);check(status==401,'Anonymous download rejected')
    data=dict(clientId=str(uuid.uuid4()),name='../private.key',size=1,sha256='0'*64,recipient=10002,service=0)
    api(a,'POST','/v1/uploads',data,400);check(True,'Path traversal filename rejected')
    a.close();time.sleep(.15);download(b,r,OUT/'sender-offline.txt');check(True,'Recipient downloads after uploader disconnects')
    status,_,_=http(a,'GET','/v1/files');check(status==401,'Logout revokes file bearer token')
    if '--basic' in sys.argv:return
    a=Peer('张三');medium=OUT/'十兆课程文档.txt'
    with medium.open('wb') as f:
        for _ in range(10):f.write(b'content '*131072)
    m=transfer(a,medium);check(m['size']==10*CHUNK,'10MB document upload');download(b,m,OUT/'medium-download.txt')
    large=OUT/'一百兆压缩包.zip'
    with zipfile.ZipFile(large,'w',compression=zipfile.ZIP_STORED) as archive:
        with archive.open('中文数据.bin','w') as f:
            for _ in range(100):f.write(os.urandom(CHUNK))
    big=transfer(a,large,True);check(big['size']>=100*CHUNK,'100MB ZIP uploaded in bounded chunks')
    partial=OUT/'large.part';status,h,first=http(b,'GET','/v1/files/'+big['fileId'],headers={'Range':'bytes=0-1048575'});assert status==206;partial.write_bytes(first)
    # Interrupt a second HTTP response without committing it to the local partial file.
    conn=HTTPSConnection('127.0.0.1',file_port,context=context);conn.request('GET','/v1/files/'+big['fileId'],headers={'Authorization':'Bearer '+b.token,'Range':'bytes=1048576-2097151'});resp=conn.getresponse();resp.read(200);conn.close()
    download(b,big,partial,CHUNK);check(True,'Interrupted download resumes with Range and verifies full hash')
    upload=create(a,medium);upload=put(a,upload,medium.read_bytes()[:CHUNK],0);again=put(a,upload,medium.read_bytes()[:CHUNK],0);check(again['offset']==CHUNK,'Lost upload acknowledgment can be retried idempotently')
    api(a,'DELETE','/v1/uploads/'+upload['uploadId']);check(not (OUT/'storage/temp'/ (upload['uploadId']+'.part')).exists(),'Cancel removes only owned temporary upload')
    broken=create(a,small);status,_,_=http(a,'PUT','/v1/uploads/'+broken['uploadId'],small.read_bytes(),{'Upload-Offset':'0','X-Chunk-SHA256':'0'*64});check(status==400,'Bad chunk hash rejected before acknowledgment')
    put(a,broken,b'X'*1024,0);bad=complete(a,broken);check(bad['state']=='failed','Whole-file SHA mismatch never marked completed');api(a,'DELETE','/v1/uploads/'+broken['uploadId'])
    pending=create(a,medium);put(a,pending,medium.read_bytes()[:CHUNK],0);oldtoken=a.token
    for peer in peers:
        if peer.alive:peer.close()
    server.kill();server=n.start_server(config,'restarted');last_beat=0;a=Peer('张三');b=Peer('李四');status,_,_=http(type('P',(),{'token':oldtoken})(),'GET','/v1/files');check(status==401,'Server restart invalidates old credentials')
    resumed=api(a,'GET','/v1/uploads/'+pending['uploadId']);check(resumed['offset']==CHUNK,'Server restart preserves confirmed upload progress')
    with medium.open('rb') as f:
        f.seek(CHUNK);offset=CHUNK
        while block:=f.read(CHUNK):put(a,pending,block,offset);offset+=len(block)
    ready=complete(a,pending);check(ready['state']=='ready','Upload resumes missing bytes after server restart')
    download(b,big,OUT/'restart-download.zip');check(any(x['fileId']==r['fileId'] for x in api(b,'GET','/v1/files')['files']),'Offline file messages survive reconnect/restart')
    status,_,_=http(b,'DELETE','/v1/uploads/'+pending['uploadId']);check(status==403,'Receiver cannot cancel uploader task')
    api(a,'POST','/v1/uploads',dict(clientId=str(uuid.uuid4()),name='oversized.bin',size=513*CHUNK,sha256='0'*64,recipient=10002,service=0),400);check(True,'Configured maximum file size enforced')
    api(a,'POST','/v1/uploads',dict(clientId=str(uuid.uuid4()),name='quota.bin',size=500*CHUNK,sha256='0'*64,recipient=10002,service=0),507);check(True,'User reserved storage quota enforced')
    api(b,'POST','/v1/uploads',dict(clientId=str(uuid.uuid4()),name='stranger.txt',size=1,sha256='0'*64,recipient=10004,service=0),403);check(True,'Non-friend upload target rejected')
    empty=OUT/'空文件.txt';empty.touch();zero=transfer(a,empty);download(b,zero,OUT/'empty.txt');check(zero['state']=='ready','Empty files complete with the SHA-256 of empty content')
    status,_,_=http(b,'GET','/v1/files/'+big['fileId'],headers={'Range':'bytes=9999999999-'});check(status==416,'Out-of-bounds Range rejected')
    status,_,_=http(b,'GET','/v1/files/'+big['fileId'],headers={'Range':'bytes=0-1','If-Range':'"wrong"'});check(status==412,'Changed file version rejected')
    half=create(a,medium);sock=context.wrap_socket(socket.create_connection(('127.0.0.1',file_port)),server_hostname='127.0.0.1')
    header=f'PUT /v1/uploads/{half["uploadId"]} HTTP/1.1\r\nHost: localhost\r\nAuthorization: Bearer {a.token}\r\nContent-Length: {CHUNK}\r\nUpload-Offset: 0\r\nX-Chunk-SHA256: '+('0'*64)+'\r\n\r\n'
    sock.sendall(header.encode()+b'X'*100);sock.close();check(api(a,'GET','/v1/uploads/'+half['uploadId'])['offset']==0,'Interrupted unconfirmed chunk never advances durable offset');api(a,'DELETE','/v1/uploads/'+half['uploadId'])
    sock=context.wrap_socket(socket.create_connection(('127.0.0.1',file_port)),server_hostname='127.0.0.1');sock.sendall(b'GET /v1/files HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\nContent-Length: 1\r\n\r\n');reply=sock.recv(4096);sock.close();check(b' 400 ' in reply,'Ambiguous duplicate Content-Length rejected')
if __name__=='__main__':
    status='passed';error=None
    try:main()
    except Exception as e:status='failed';error=repr(e);print('FAIL',error,flush=True)
    finally:
        for peer in peers:
            if peer.alive:peer.close()
        for child in n.children:child.kill()
        setup.write_json(OUT/'results.json',dict(status=status,count=len(checks),checks=checks,error=error));print('RESULT',OUT/'results.json',flush=True)
    raise SystemExit(status!='passed')
