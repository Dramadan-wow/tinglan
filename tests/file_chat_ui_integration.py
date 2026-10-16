"""Exercise file integration through real AppSession/MainWindow/ChatPage instances."""
import hashlib,json,subprocess,sys,time
import network_integration as n
import workspace_integration as w
sys.path.insert(0,str(n.ROOT/'scripts'))
import prepare_lan as setup
sys.stdout.reconfigure(encoding='utf-8')
OUT=n.ROOT/'tests/file-output'/('chat-ui-'+str(time.time_ns()));n.OUT=w.OUT=OUT
n.SERVER=n.ROOT/'build-gui/server/Release/im_server.exe'
checks=[]
def check(ok,label):
    assert ok,label
    checks.append(label);print('PASS',label,flush=True)
def main():
    OUT.mkdir(parents=True);seed=OUT/'seed';seed.mkdir();r=subprocess.run([w.PROBE],input=(json.dumps(dict(directory=str(seed),seed=True))+'\n').encode(),env=n.ENV,capture_output=True,timeout=30);assert r.returncode==0
    tls=OUT/'tls';tls.mkdir();setup.certificates('127.0.0.1',tls);port=n.free_port();fileport=n.free_port();server=OUT/'server.json';client=OUT/'client.json'
    setup.write_json(server,dict(listenAddress='127.0.0.1',port=port,tls=True,certificateFile=str(tls/'server.pem'),keyFile=str(tls/'server.key'),usersFile=str(OUT/'users.json'),workspaceFile=str(OUT/'workspace.sqlite'),workspaceImport=str(seed/'data.txt'),fileServiceEnabled=True,filePort=fileport,fileStorageDirectory=str(OUT/'storage')))
    setup.write_json(client,dict(host='127.0.0.1',port=port,tls=True,caFile=str(tls/'ca.pem')));n.start_server(server,'server')
    a=w.Gui('a','张三',client);b=w.Gui('b','李四',client);source=OUT/'实际聊天窗口文件.txt';source.write_text('你好，文件传输！\n'*100,encoding='utf-8')
    a.command('fileUpload',target=10002,path=str(source));check(a.event('fileButton')['enabled'],'Actual private chat file button is enabled after TLS login')
    ready=a.event('fileTask',lambda e:e['task'].get('status')=='complete')['task'];check(True,'Actual AppSession uploads file successfully')
    b.command('fileState',target=10001);state=b.event('fileState',lambda e:e['cards']>0);check(state['screenshot'] and any(f.get('fileId')==ready['fileId'] for f in state['files']),'Recipient real chat window renders received file card')
    destination=OUT/'界面接收.txt';b.command('fileDownload',target=10001,fileId=ready['fileId'],path=str(destination));b.event('fileTask',lambda e:e['task'].get('status')=='complete')
    check(hashlib.sha256(source.read_bytes()).digest()==hashlib.sha256(destination.read_bytes()).digest(),'Actual AppSession download verifies identical content')
    b.command('send',kind=0,target=10001,text='文件已收到，普通聊天继续正常');a.has('文件已收到，普通聊天继续正常');check(True,'Original text composer still sends and receives messages')
    a.command('fileState',target=10002);a.event('fileState',lambda e:e['cards']>0)
if __name__=='__main__':
    error=None
    try:main()
    except Exception as e:error=repr(e);print('FAIL',error,flush=True)
    finally:
        for p in n.children:p.kill()
        setup.write_json(OUT/'results.json',dict(status='failed' if error else 'passed',count=len(checks),checks=checks,error=error));print('RESULT',OUT/'results.json',flush=True)
    raise SystemExit(bool(error))
