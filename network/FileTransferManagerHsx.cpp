#include "FileTransferManagerHsx.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QSaveFile>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QUrl>
#include <algorithm>
namespace im::net {
namespace {
constexpr qint64 Chunk=1024*1024;
qint64 number(const QJsonObject& o,const char* key){return qint64(o[key].toDouble());}
bool terminal(const QString& s){return s=="complete"||s=="cancelled"||s=="failed"||s=="paused";}
}
FileTransferManagerHsx::FileTransferManagerHsx(QObject* p):QObject(p){
    workers_.setMaxThreadCount(2);poll_.setInterval(15000);retry_.setInterval(5000);
    connect(&poll_,&QTimer::timeout,this,&FileTransferManagerHsx::refresh);
    connect(&retry_,&QTimer::timeout,this,[this]{if(!available())return;for(const auto& id:tasks_.keys())if(tasks_[id]["status"]=="waiting")run(id);});retry_.start();
}
FileTransferManagerHsx::~FileTransferManagerHsx(){disconnected();workers_.waitForDone();}
void FileTransferManagerHsx::configure(const ClientOptionsHsx& o,const QString& root){
    disconnected();tasks_.clear();files_={};user_=0;cache_.clear();options_=o;options_.password.clear();root_=root;
}
void FileTransferManagerHsx::stop(const QString& id){auto& r=running_[id];++r.generation;if(r.cancel)r.cancel->store(true);if(r.reply){r.reply->abort();r.reply.clear();}}
void FileTransferManagerHsx::disconnected(){
    token_.clear();poll_.stop();++epoch_;
    for(const auto& id:running_.keys())stop(id);
    for(auto it=tasks_.begin();it!=tasks_.end();++it)if(!terminal(it.value()["status"].toString()))it.value()["status"]="waiting";
    save();emit filesChanged();
}
void FileTransferManagerHsx::setSession(const QJsonObject& s){
    disconnected();if(!options_.tls||s["user"].toInt()<=0||s["port"].toInt()<=0||s["port"].toInt()>65535||s["token"].toString().size()!=64)return;
    QFile ca(options_.caFile);QByteArray trust;if(ca.open(QIODevice::ReadOnly))trust=ca.readAll();
    const auto scope=QCryptographicHash::hash(options_.host.toUtf8()+":"+QByteArray::number(options_.port)+trust,QCryptographicHash::Sha256).toHex();
    const auto cache=QDir(root_).filePath(QString::fromLatin1(scope)+"-"+QString::number(s["user"].toInt())+".json");
    user_=s["user"].toInt();port_=s["port"].toInt();token_=s["token"].toString();
    if(cache_!=cache){cache_=cache;tasks_.clear();files_={};load();}
    poll_.start();refresh();for(const auto& id:tasks_.keys())if(tasks_[id]["status"]=="waiting")run(id);emit filesChanged();
}
void FileTransferManagerHsx::save(){
    if(cache_.isEmpty())return;QDir().mkpath(root_);QJsonArray array;for(auto t:tasks_){t.remove("speed");t.remove("progress");array.append(t);}
    QSaveFile f(cache_);if(!f.open(QIODevice::WriteOnly)||f.write(QJsonDocument(QJsonObject{{"tasks",array},{"files",files_}}).toJson(QJsonDocument::Compact))<0||!f.commit())emit problem("无法保存文件传输进度，请检查客户端目录权限");
}
void FileTransferManagerHsx::load(){
    QFile f(cache_);if(!f.open(QIODevice::ReadOnly)||f.size()>8*1024*1024)return;const auto o=QJsonDocument::fromJson(f.readAll()).object();files_=o["files"].toArray();
    for(const auto v:o["tasks"].toArray()){auto t=v.toObject();const auto id=t["localId"].toString();if(!validId(id))continue;if(!terminal(t["status"].toString()))t["status"]="waiting";tasks_[id]=t;}
}
QJsonObject FileTransferManagerHsx::task(const QString& id)const{return tasks_.value(id);}
void FileTransferManagerHsx::state(const QString& id,const QString& s,const QString& error){if(!tasks_.contains(id))return;tasks_[id]["status"]=s;tasks_[id]["error"]=error;save();emit taskChanged(id);}
void FileTransferManagerHsx::request(const QString& id,const QByteArray& method,const QString& path,const QByteArray& body,const QMap<QByteArray,QByteArray>& headers,Callback callback){
    if(!available()){if(!id.isEmpty())state(id,"waiting","等待重新连接服务器");return;}
    auto config=QSslConfiguration::defaultConfiguration();config.setProtocol(QSsl::TlsV1_2OrLater);config.setPeerVerifyMode(QSslSocket::VerifyPeer);
    if(!options_.caFile.isEmpty()){QFile f(options_.caFile);if(!f.open(QIODevice::ReadOnly)){state(id,"failed","无法读取 CA 证书");return;}const auto certs=QSslCertificate::fromData(f.readAll());if(certs.isEmpty()){state(id,"failed","CA 证书无效");return;}config.setCaCertificates(certs);}
    QUrl url;url.setScheme("https");url.setHost(options_.host);url.setPort(port_);url.setPath(path);
    QNetworkRequest req(url);req.setSslConfiguration(config);req.setTransferTimeout(30000);req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);req.setRawHeader("Authorization","Bearer "+token_.toLatin1());req.setRawHeader("Content-Type","application/octet-stream");
    for(auto i=headers.begin();i!=headers.end();++i)req.setRawHeader(i.key(),i.value());
    auto* reply=network_.sendCustomRequest(req,method,body);reply->setReadBufferSize(2*Chunk);auto buffer=std::make_shared<QByteArray>();
    const auto epoch=epoch_,generation=running_[id].generation;running_[id].reply=reply;auto clock=std::make_shared<QElapsedTimer>();clock->start();
    connect(reply,&QNetworkReply::readyRead,this,[reply,buffer]{*buffer+=reply->readAll();if(buffer->size()>2*Chunk)reply->abort();});
    const auto progress=[this,id,epoch,generation,clock](qint64 n,qint64){if(epoch!=epoch_||generation!=running_[id].generation||!tasks_.contains(id))return;auto& t=tasks_[id];t["progress"]=double(qMin(number(t,"size"),number(t,"offset")+n));t["speed"]=double(n)*1000/qMax<qint64>(1,clock->elapsed());emit taskChanged(id);};
    if(!id.isEmpty()){if(method=="PUT")connect(reply,&QNetworkReply::uploadProgress,this,progress);else if(method=="GET"&&path.startsWith("/v1/files/"))connect(reply,&QNetworkReply::downloadProgress,this,progress);}
    connect(reply,&QNetworkReply::finished,this,[this,id,reply,buffer,callback,epoch,generation]{
        reply->deleteLater();if(epoch!=epoch_||generation!=running_[id].generation)return;running_[id].reply.clear();*buffer+=reply->readAll();
        int code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();auto json=QJsonDocument::fromJson(*buffer).object();
        if(buffer->size()>2*Chunk){json={{"error","文件响应超过限制"}};callback(400,json,{},reply);return;}
        if(code>=200&&code<300&&reply->error()!=QNetworkReply::NoError)code=0; // An interrupted 206 body is not a completed chunk.
        if(!code)json={{"error",reply->error()==QNetworkReply::SslHandshakeFailedError?"TLS 证书验证失败":"网络中断，等待恢复后继续"}};
        callback(!code&&reply->error()==QNetworkReply::SslHandshakeFailedError?400:code,json,*buffer,reply);
    });
}
bool FileTransferManagerHsx::response(const QString& id,int code,const QJsonObject& d){
    if(code>=200&&code<300){tasks_[id]["attempts"]=0;return true;}
    const int attempts=tasks_[id]["attempts"].toInt()+1;tasks_[id]["attempts"]=attempts;
    state(id,(code==0||code==429||code>=500)&&attempts<6?"waiting":"failed",d["error"].toString("文件服务拒绝请求（"+QString::number(code)+"）"));return false;
}
void FileTransferManagerHsx::refresh(){
    if(!available()||running_[""].reply)return;
    request({},"GET","/v1/files",{}, {},[this](int code,const QJsonObject& d,const QByteArray&,QNetworkReply*){if(code!=200)return;maximum_=number(d,"maxFileBytes");auto files=d["files"].toArray();
        // Resolve a completion response lost during logout or a network interruption.
        for(const auto v:files){const auto f=v.toObject();if(f["state"]!="ready")continue;for(const auto& id:tasks_.keys()){auto& t=tasks_[id];if(t["kind"]=="upload"&&t["clientId"]==f["clientId"]&&t["status"]!="complete"){stop(id);t["fileId"]=f["fileId"];t["offset"]=f["size"];t["state"]="ready";t["cancelPending"]=false;state(id,"complete");}}}
        if(files!=files_){files_=files;save();emit filesChanged();}});
}
QString FileTransferManagerHsx::upload(int service,int recipient,const QString& path){
    if(!available()){emit problem("请连接已启用 HTTPS 文件服务的中心服务器");return {};}
    int active=0;for(const auto& t:tasks_)if(!terminal(t["status"].toString()))++active;
    const QFileInfo f(path);if(active>=4||!f.isFile()||f.isSymLink()||f.size()>maximum_){emit problem("请选择有效文件，大小不能超过服务器上限；同时传输最多 4 个任务");return {};}
    const auto id=newId();tasks_[id]={{"localId",id},{"clientId",id},{"kind","upload"},{"path",f.absoluteFilePath()},{"name",f.fileName()},{"size",double(f.size())},{"sender",user_},{"recipient",recipient},{"source",service},{"created",double(QDateTime::currentSecsSinceEpoch())},{"started",double(QDateTime::currentMSecsSinceEpoch())},{"offset",0},{"status","waiting"}};save();emit filesChanged();run(id);return id;
}
QString FileTransferManagerHsx::partial(const QString& id)const{return tasks_.value(id)["path"].toString()+".tinglan-"+id+".part";}
QString FileTransferManagerHsx::download(const QJsonObject& file,const QString& path){
    if(!available()||file["state"]!="ready"||!validId(file["fileId"].toString()))return {};
    int active=0;for(const auto& t:tasks_)if(!terminal(t["status"].toString()))++active;if(active>=4){emit problem("同时传输最多 4 个任务");return {};}
    const auto dest=QFileInfo(path).absoluteFilePath();if(QFileInfo::exists(dest)){emit problem("为保护已有文件，请选择一个尚不存在的保存文件名");return {};}
    const auto id=newId();auto t=file;t["localId"]=id;t["kind"]="download";t["path"]=dest;t["status"]="waiting";t["offset"]=0;t["progress"]=0;t["started"]=double(QDateTime::currentMSecsSinceEpoch());tasks_[id]=t;
    QFile f(partial(id));if(!f.open(QIODevice::WriteOnly|QIODevice::NewOnly)){tasks_.remove(id);emit problem("无法在所选目录创建下载临时文件");return {};}f.close();save();emit filesChanged();run(id);return id;
}
void FileTransferManagerHsx::hash(const QString& id,const QString& path,std::function<void(const QString&)> callback){
    state(id,"hashing");const auto epoch=epoch_,generation=running_[id].generation;auto cancel=std::make_shared<std::atomic_bool>(false);running_[id].cancel=cancel;
    workers_.start([this,id,path,epoch,generation,cancel,callback]{QFile f(path);QCryptographicHash hash(QCryptographicHash::Sha256);bool ok=f.open(QIODevice::ReadOnly);
        while(ok&&!f.atEnd()&&!cancel->load()){auto b=f.read(Chunk);if(b.isEmpty()&&f.error()!=QFile::NoError){ok=false;break;}hash.addData(b);}const auto result=ok&&!cancel->load()?QString::fromLatin1(hash.result().toHex()):QString{};f.close();
        QMetaObject::invokeMethod(this,[this,id,epoch,generation,result,callback]{if(epoch!=epoch_||generation!=running_[id].generation)return;running_[id].cancel.reset();if(result.isEmpty()){state(id,"failed","无法读取文件进行 SHA-256 校验");return;}callback(result);},Qt::QueuedConnection);
    });
}
void FileTransferManagerHsx::run(const QString& id){
    if(!tasks_.contains(id)||!available())return;stop(id);auto& t=tasks_[id];
    int active=0;for(auto i=running_.begin();i!=running_.end();++i)if(!i.key().isEmpty()&&i.key()!=id&&(i->reply||(i->cancel&&!i->cancel->load())))++active;
    if(active>=4){state(id,"waiting","等待其他文件任务完成");return;}
    if(t["cancelPending"].toBool()){cancel(id);return;}
    if(t["kind"]=="download"){
        QFile f(partial(id));if(!f.open(QIODevice::ReadWrite)||f.isSequential()||f.size()>number(t,"size")){state(id,"failed","下载临时文件不可用，请重新下载");return;}
        if(t["corrupt"].toBool()){f.resize(0);t["corrupt"]=false;}t["offset"]=double(f.size());f.close();nextDownload(id);return;
    }
    hash(id,t["path"].toString(),[this,id](const QString& sha){auto& t=tasks_[id];if(QFileInfo(t["path"].toString()).size()!=number(t,"size")||(!t["sha256"].toString().isEmpty()&&t["sha256"]!=sha)){state(id,"failed","源文件已经改变，请取消后重新选择文件");return;}t["sha256"]=sha;
        auto accept=[this,id](int code,const QJsonObject& d,const QByteArray&,QNetworkReply*){if(!response(id,code,d))return;auto& t=tasks_[id];t["uploadId"]=d["uploadId"];t["fileId"]=d["fileId"];t["offset"]=d["offset"];t["destination"]=d["destination"];const auto s=d["state"].toString();
            if(s=="ready"){t["state"]="ready";state(id,"complete");refresh();emit filesChanged();return;}
            if(s=="failed"||s=="expired"||s=="cancelled"){state(id,"failed","服务器任务已失效，请取消后重新选择文件");return;}nextUpload(id);};
        if(t["uploadId"].toString().isEmpty()){auto data=t;data["service"]=t["source"];data.remove("path");request(id,"POST","/v1/uploads",QJsonDocument(data).toJson(QJsonDocument::Compact),{},accept);}
        else request(id,"GET","/v1/uploads/"+t["uploadId"].toString(),{},{},accept);
    });
}
void FileTransferManagerHsx::nextUpload(const QString& id){
    auto& t=tasks_[id];const auto offset=number(t,"offset"),size=number(t,"size");const auto url="/v1/uploads/"+t["uploadId"].toString();
    if(offset==size){state(id,"verifying");request(id,"POST",url+"/complete",{},{},[this,id](int code,const QJsonObject& d,const QByteArray&,QNetworkReply*){if(!response(id,code,d))return;
            if(d["state"]=="ready"){tasks_[id]["state"]="ready";state(id,"complete");refresh();emit filesChanged();return;}
            const auto epoch=epoch_,generation=running_[id].generation;QTimer::singleShot(500,this,[this,id,epoch,generation]{if(epoch==epoch_&&generation==running_[id].generation)nextUpload(id);});});return;}
    QFile f(t["path"].toString());if(offset<0||offset>size||offset%Chunk||!f.open(QIODevice::ReadOnly)||f.size()!=size||!f.seek(offset)){state(id,"failed","源文件或服务器断点无效");return;}
    const auto bytes=f.read(qMin(Chunk,size-offset));if(bytes.size()!=qMin(Chunk,size-offset)){state(id,"failed","读取源文件失败");return;}
    state(id,"uploading");request(id,"PUT",url,bytes,{{"Upload-Offset",QByteArray::number(offset)},{"X-Chunk-SHA256",QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex()}},[this,id,offset,count=bytes.size()](int code,const QJsonObject& d,const QByteArray&,QNetworkReply*){if(!response(id,code,d))return;if(number(d,"offset")!=offset+count){state(id,"failed","上传确认偏移不匹配");return;}tasks_[id]["offset"]=d["offset"];save();nextUpload(id);});
}
void FileTransferManagerHsx::nextDownload(const QString& id){
    auto& t=tasks_[id];const auto offset=number(t,"offset"),size=number(t,"size");
    if(offset==size){hash(id,partial(id),[this,id](const QString& sha){auto& t=tasks_[id];if(sha!=t["sha256"].toString()){t["corrupt"]=true;state(id,"failed","SHA-256 校验失败，重试将重新下载");return;}
            if(!QFile::rename(partial(id),t["path"].toString())){state(id,"failed","无法保存文件；目标可能已存在，请移走目标文件后重试");return;}state(id,"complete");emit filesChanged();});return;}
    const auto end=qMin(offset+Chunk,size)-1;state(id,"downloading");request(id,"GET","/v1/files/"+t["fileId"].toString(),{},{{"Range","bytes="+QByteArray::number(offset)+"-"+QByteArray::number(end)},{"If-Range",'"'+t["sha256"].toString().toLatin1()+'"'}},[this,id,offset,end,size](int code,const QJsonObject& d,const QByteArray& bytes,QNetworkReply* reply){
        if(!response(id,code,d))return;const auto expected="bytes "+QByteArray::number(offset)+"-"+QByteArray::number(end)+"/"+QByteArray::number(size);
        if(code!=206||reply->rawHeader("Content-Range")!=expected||bytes.size()!=end-offset+1){state(id,"failed","下载分段长度或 Range 响应无效");return;}
        QFile f(partial(id));if(!f.open(QIODevice::ReadWrite)||f.size()!=offset||!f.seek(offset)||f.write(bytes)!=bytes.size()||!f.flush()){if(f.isOpen())f.resize(offset);state(id,"failed","下载写入失败，请检查磁盘空间");return;}f.close();tasks_[id]["offset"]=double(end+1);save();nextDownload(id);
    });
}
void FileTransferManagerHsx::pause(const QString& id){if(!tasks_.contains(id)||tasks_[id]["status"]=="complete"||tasks_[id]["status"]=="cancelled")return;stop(id);state(id,"paused");}
void FileTransferManagerHsx::resume(const QString& id){if(!tasks_.contains(id)||tasks_[id]["status"]=="complete"||tasks_[id]["status"]=="cancelled")return;tasks_[id]["attempts"]=0;state(id,"waiting");run(id);}
void FileTransferManagerHsx::cancel(const QString& id){
    if(!tasks_.contains(id)||tasks_[id]["status"]=="complete")return;stop(id);auto& t=tasks_[id];
    if(t["kind"]=="download"){QFile::remove(partial(id));state(id,"cancelled");return;}
    // Creation may have reached the server even when its response was lost. Resolve by clientId first.
    t["cancelPending"]=true;
    if(!available()){state(id,"waiting","重新连接后取消服务器任务");return;}
    auto remove=[this,id](const QString& upload){request(id,"DELETE","/v1/uploads/"+upload,{},{},[this,id](int code,const QJsonObject& d,const QByteArray&,QNetworkReply*){if(!response(id,code,d))return;tasks_[id]["cancelPending"]=false;state(id,"cancelled");refresh();});};
    if(!t["uploadId"].toString().isEmpty()){remove(t["uploadId"].toString());return;}
    if(t["sha256"].toString().isEmpty()){t["cancelPending"]=false;state(id,"cancelled");return;}
    auto data=t;data["service"]=t["source"];data.remove("path");request(id,"POST","/v1/uploads",QJsonDocument(data).toJson(QJsonDocument::Compact),{},[this,id,remove](int code,const QJsonObject& d,const QByteArray&,QNetworkReply*){if(!response(id,code,d))return;tasks_[id]["uploadId"]=d["uploadId"];remove(d["uploadId"].toString());});
}
QJsonArray FileTransferManagerHsx::cards(int service,int peer)const{
    QHash<QString,QJsonObject> merged;
    auto relevant=[&](const QJsonObject& f){return (f["sender"].toInt()==user_&&f["source"].toInt()==service&&f["recipient"].toInt()==peer)||(f["recipient"].toInt()==user_&&f["destination"].toInt()==service&&f["sender"].toInt()==peer);};
    for(const auto v:files_){const auto f=v.toObject();if(relevant(f)&&f["state"]=="ready")merged[f["fileId"].toString()]=f;}
    auto keys=tasks_.keys();std::sort(keys.begin(),keys.end(),[this](const auto& a,const auto& b){return number(tasks_[a],"started")<number(tasks_[b],"started");});
    for(const auto& id:keys){const auto t=tasks_[id];if(!relevant(t))continue;const auto key=t["fileId"].toString().isEmpty()?id:t["fileId"].toString();
        if(merged.contains(key)&&merged[key]["kind"]=="download"&&t["kind"]=="upload")continue;merged[key]=t;}
    QList<QJsonObject> sorted=merged.values();std::sort(sorted.begin(),sorted.end(),[](const auto& a,const auto& b){return number(a,"created")<number(b,"created");});QJsonArray out;for(const auto& f:sorted)out.append(f);return out;
}
}
