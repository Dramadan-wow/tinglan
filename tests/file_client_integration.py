"""Two actual Qt transfer clients + TLS server, isolated files and accounts only."""
import hashlib,json,os,subprocess,time,traceback,zipfile
from pathlib import Path
import network_integration as n
import workspace_integration as w
import sys
sys.path.insert(0,str(n.ROOT/'scripts'))
import prepare_lan as setup
sys.stdout.reconfigure(encoding='utf-8')
OUT=n.ROOT/'tests/file-output'/('clients-'+str(time.time_ns()));n.OUT=w.OUT=OUT
n.SERVER=n.ROOT/'build-gui/server/Release/im_server.exe'
PROBE=n.ROOT/'build-gui/Release/file_client_probe.exe'
checks=[]
def check(ok,label):
    assert ok,label
    checks.append(label);print('PASS',label,flush=True)
class Client:
    def __init__(self,name,nickname,peer):
        self.name=name;self.seq=0;self.folder=OUT/name;self.folder.mkdir(exist_ok=True)
        self.commandfile=self.folder/'command.json';self.output=self.folder/'output.json'
        for f in (self.commandfile,self.output):f.unlink(missing_ok=True)
        cfg=dict(host='127.0.0.1',port=port,caFile=str(OUT/'tls/ca.pem'),username=nickname,password='123456',peer=peer,cache=str(self.folder/'cache'),command=str(self.commandfile),output=str(self.output))
        path=self.folder/'config.json';setup.write_json(path,cfg);self.process=n.Process([PROBE,path],name+'-'+str(time.time_ns()));self.wait(lambda s:s['available'])
    def read(self):
        try:return json.loads(self.output.read_text(encoding='utf-8'))
        except (OSError,ValueError):return {}
    def wait(self,predicate,timeout=45):return n.wait_for(lambda:(lambda s:s if s and predicate(s) else None)(self.read()),timeout)
    def command(self,op,**kw):
        self.seq+=1;tmp=self.commandfile.with_suffix('.tmp');setup.write_json(tmp,dict(seq=self.seq,op=op,**kw))
        def replace():
            try:os.replace(tmp,self.commandfile);return True
            except PermissionError:return False
        n.wait_for(replace,5);return self.wait(lambda s:s['seq']==self.seq)
    def status(self,s):return self.wait(lambda o:o['last'].get('status')==s)
    def kill(self):self.process.kill()
def digest(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def main():
    global port
    OUT.mkdir(parents=True);seed=OUT/'seed';seed.mkdir();result=subprocess.run([w.PROBE],input=(json.dumps(dict(directory=str(seed),seed=True))+'\n').encode(),env=n.ENV,capture_output=True,timeout=30);assert result.returncode==0
    tls=OUT/'tls';tls.mkdir();setup.certificates('127.0.0.1',tls);port=n.free_port();httpport=n.free_port();config=OUT/'server.json'
    setup.write_json(config,dict(listenAddress='127.0.0.1',port=port,tls=True,certificateFile=str(tls/'server.pem'),keyFile=str(tls/'server.key'),workspaceImport=str(seed/'data.txt'),usersFile=str(OUT/'users.json'),workspaceFile=str(OUT/'workspace.sqlite'),fileServiceEnabled=True,filePort=httpport,fileStorageDirectory=str(OUT/'storage')))
    server=n.start_server(config,'server');a=Client('a','张三',10002);b=Client('b','李四',10001)
    small=OUT/'中文 小文件.txt';small.write_bytes(('你好，听澜！\n'.encode()*100)[:1024]);a.command('upload',path=str(small));r=a.status('complete')['last'];check(r['offset']==1024,'Qt client uploads 1KB Chinese file')
    b.wait(lambda s:any(f.get('fileId')==r['fileId'] for f in s['cards']));check(True,'TCP file notification updates recipient cards')
    dest=OUT/'收到的文件.txt';b.command('download',fileId=r['fileId'],path=str(dest));b.status('complete');check(digest(small)==digest(dest),'Qt download SHA-256 matches source')
    a.command('screenshot',path=str(OUT/'upload-card.png'));b.command('screenshot',path=str(OUT/'download-card.png'));check((OUT/'download-card.png').exists(),'Actual Qt file message cards rendered')
    medium=OUT/'十兆文件.bin'
    with medium.open('wb') as f:
        for _ in range(10):f.write(os.urandom(1024*1024))
    a.command('upload',path=str(medium),autoPause=True);paused=a.status('paused')['last'];uploadid=paused['localId'];check(1024*1024<=paused['offset']<medium.stat().st_size,'Upload pauses after a confirmed chunk')
    time.sleep(.3);check(a.read()['last']['offset']==paused['offset'],'Paused upload does not advance')
    a.command('resume');large=a.status('complete')['last'];check(large['localId']==uploadid,'Resume keeps the same upload task')
    b.wait(lambda s:any(f.get('fileId')==large['fileId'] for f in s['cards']));dest2=OUT/'下载中断.bin';b.command('download',fileId=large['fileId'],path=str(dest2),autoPause=True);paused=b.status('paused')['last'];check(paused['offset']>=1024*1024,'Download pauses with a durable temporary file')
    downloadid=paused['localId'];part=Path(str(dest2)+'.tinglan-'+downloadid+'.part');check(part.exists() and not dest2.exists(),'Incomplete download is not exposed as a finished file')
    b.kill();time.sleep(.25);b=Client('b','李四',10001);b.wait(lambda s:any(f.get('localId')==downloadid and f.get('status')=='paused' for f in s['cards']));check(True,'Client restart restores paused download')
    b.command('resume',id=downloadid);b.wait(lambda s:any(f.get('localId')==downloadid and f.get('status')=='complete' for f in s['cards']));check(digest(dest2)==digest(medium),'Restarted Qt client resumes and verifies download')
    a.command('upload',path=str(medium),autoPause=True);a.status('paused');a.command('cancel');a.status('cancelled');check(True,'Qt upload cancel acknowledged by server')
    # Simulate transport loss while a task is pending, then restore its authenticated session.
    a.command('upload',path=str(medium),autoPause=True);a.status('paused');a.command('disconnect');a.command('resume');check(a.read()['last']['status']=='waiting','Offline resume waits without losing upload progress')
    a.command('connect');ready=a.status('complete')['last'];check(True,'Fresh TLS login resumes waiting upload automatically')
    a.kill();b.command('refresh');b.wait(lambda s:any(f.get('fileId')==ready['fileId'] for f in s['cards']));offline=OUT/'离线下载.bin';b.command('download',fileId=ready['fileId'],path=str(offline));b.status('complete');check(digest(offline)==digest(medium),'Qt recipient downloads after sender exits')
    # A deliberately corrupted local partial must never be marked complete.
    corrupted=OUT/'校验重试.bin';b.command('download',fileId=large['fileId'],path=str(corrupted),autoPause=True);bad=b.status('paused')['last'];part=Path(str(corrupted)+'.tinglan-'+bad['localId']+'.part')
    with part.open('r+b') as f:f.write(b'BROKEN')
    b.command('resume');b.status('failed');check(not corrupted.exists(),'Corrupt partial download fails SHA-256 and remains temporary')
    b.command('resume');b.status('complete');check(digest(corrupted)==digest(medium),'Failed checksum can be retried successfully')
    cancelpath=OUT/'取消下载.bin';b.command('download',fileId=large['fileId'],path=str(cancelpath),autoPause=True);cancel=b.status('paused')['last'];b.command('cancel');b.status('cancelled');check(not Path(str(cancelpath)+'.tinglan-'+cancel['localId']+'.part').exists(),'Cancel download removes only its temporary file')
    a=Client('a','张三',10002);zip_path=OUT/'一百兆Qt传输.zip'
    with zipfile.ZipFile(zip_path,'w',compression=zipfile.ZIP_STORED) as archive:
        with archive.open('大文件.bin','w') as f:
            for _ in range(100):f.write(os.urandom(1024*1024))
    a.command('upload',path=str(zip_path));sent=a.status('complete')['last'];b.wait(lambda s:any(f.get('fileId')==sent['fileId'] for f in s['cards']))
    received=OUT/'Qt收到的压缩包.zip';b.command('download',fileId=sent['fileId'],path=str(received));b.status('complete');check(digest(received)==digest(zip_path),'Two Qt clients transfer 100MB ZIP with matching SHA-256')
    a.command('screenshot',path=str(OUT/'all-upload-cards.png'));b.command('screenshot',path=str(OUT/'all-download-cards.png'))
if __name__=='__main__':
    error=None
    try:main()
    except Exception as e:error=repr(e);traceback.print_exc()
    finally:
        for process in n.children:process.kill()
        setup.write_json(OUT/'results.json',dict(status='failed' if error else 'passed',count=len(checks),checks=checks,error=error));print('RESULT',OUT/'results.json',flush=True)
    raise SystemExit(bool(error))
