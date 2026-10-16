#include "FileStorageServiceHsx.h"
#include "ProtocolHsx.h"
#include <QTcpServer>
#include <QSslSocket>
#include <QSslKey>
#include <QSslError>
#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QPointer>
#include <QUrl>
#include <QUrlQuery>
#include <QStorageInfo>
#include <functional>
#include <stdexcept>
namespace im::net {
namespace {
constexpr qint64 Chunk=1024*1024;
struct HttpFailure {int code;QString message;};
void require(bool ok,int code,const QString& message){if(!ok)throw HttpFailure{code,message};}
QSqlQuery query(QSqlDatabase db,const QString& text,const QVariantList& values={}){
    QSqlQuery q(db);q.prepare(text);for(const auto& v:values)q.addBindValue(v);
    if(!q.exec())throw std::runtime_error("file database operation failed");return q;
}
bool sha(const QString& s){static QRegularExpression re("^[0-9a-f]{64}$");return re.match(s).hasMatch();}
bool nameValid(const QString& s){
    if(s.isEmpty()||s.toUtf8().size()>240||s=="."||s==".."||s.endsWith('.')||s.endsWith(' '))return false;
    for(const auto c:s)if(c.unicode()<32||c.unicode()==127||QString("/\\:*?\"<>|").contains(c))return false;
    static QRegularExpression reserved("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\\.|$)",QRegularExpression::CaseInsensitiveOption);
    return !reserved.match(s).hasMatch();
}
class Listener:public QTcpServer {
public:std::function<void(qintptr)> accept;
protected:void incomingConnection(qintptr s)override{accept(s);}
};
}
// Restricted HTTP/1.1: one bounded request per TLS connection; no chunked request encoding.
class FileHttpConnectionHsx final:public QObject {
public:
    FileHttpConnectionHsx(qintptr descriptor,const QSslConfiguration& config,FileStorageServiceHsx* service):QObject(service),service_(service){
        socket_=new QSslSocket(this);timer_=new QTimer(this);timer_->setSingleShot(true);timer_->setInterval(30000);
        connect(timer_,&QTimer::timeout,socket_,&QSslSocket::abort);timer_->start();
        if(!socket_->setSocketDescriptor(descriptor)){deleteLater();return;}
        socket_->setReadBufferSize(Chunk+16384);socket_->setSslConfiguration(config);
        connect(socket_,&QSslSocket::sslErrors,socket_,[this](const QList<QSslError>&){socket_->abort();});
        connect(socket_,&QSslSocket::disconnected,this,&QObject::deleteLater);
        connect(socket_,&QSslSocket::errorOccurred,this,[this](QAbstractSocket::SocketError){socket_->abort();deleteLater();});
        connect(socket_,&QSslSocket::readyRead,this,[this]{read();});
        // TLS has separate plaintext and ciphertext queues. Resume after either drains,
        // queued to avoid re-entering pump() from inside QSslSocket::write().
        connect(socket_,&QSslSocket::bytesWritten,this,[this]{pump();},Qt::QueuedConnection);
        connect(socket_,&QSslSocket::encryptedBytesWritten,this,[this]{pump();},Qt::QueuedConnection);socket_->startServerEncryption();
    }
    void json(int code,const QJsonObject& value){reply(code,QJsonDocument(value).toJson(QJsonDocument::Compact),"application/json; charset=utf-8");}
    void reply(int code,const QByteArray& body,const QByteArray& type,const QByteArray& extra={}){
        if(replied_)return;replied_=true;timer_->start();
        socket_->write("HTTP/1.1 "+QByteArray::number(code)+" Response\r\nContent-Type: "+type+"\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n"+extra+"\r\n"+body);socket_->disconnectFromHost();
    }
    void file(const QString& path,qint64 start,qint64 end,qint64 total,const QString& hash,bool range){
        output_=std::make_unique<QFile>(path);require(output_->open(QIODevice::ReadOnly)&&output_->size()==total&&output_->seek(start),410,"文件不再可用");
        replied_=true;remaining_=total==0?0:end-start+1;timer_->start();
        QByteArray header="HTTP/1.1 "+QByteArray(range?"206 Partial Content":"200 OK")+"\r\nContent-Type: application/octet-stream\r\nContent-Disposition: attachment\r\nAccept-Ranges: bytes\r\nETag: \""+hash.toLatin1()+"\"\r\nContent-Length: "+QByteArray::number(remaining_)+"\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n";
        if(range)header+="Content-Range: bytes "+QByteArray::number(start)+"-"+QByteArray::number(end)+"/"+QByteArray::number(total)+"\r\n";
        socket_->write(header+"\r\n");pump();
    }
private:
    void pump(){
        if(!output_)return;
        while(remaining_>0&&socket_->bytesToWrite()+socket_->encryptedBytesToWrite()<128*1024){
            auto b=output_->read(qMin<qint64>(65536,remaining_));if(b.isEmpty()){socket_->abort();return;}socket_->write(b);remaining_-=b.size();timer_->start();
        }
        if(!remaining_){output_.reset();socket_->disconnectFromHost();}
    }
    void read(){
        if(handled_||!socket_->isEncrypted())return;
        buffer_+=socket_->readAll();if(buffer_.size()>Chunk+16384){json(413,{{"error","request_too_large"}});handled_=true;return;}
        if(!parsed_){
            const auto pos=buffer_.indexOf("\r\n\r\n");if(pos<0){if(buffer_.size()>8192){handled_=true;json(431,{{"error","headers_too_large"}});}return;}
            if(pos>8192){handled_=true;json(431,{{"error","headers_too_large"}});return;}
            const auto lines=buffer_.left(pos).split('\n');const auto first=lines.value(0).trimmed().split(' ');
            bool valid=first.size()==3&&first[2]=="HTTP/1.1"&&first[1].startsWith("/v1/")&&!first[1].contains('%');
            if(valid){method_=first[0];path_=QString::fromLatin1(first[1]);}
            for(int i=1;i<lines.size();++i){const auto line=lines[i].trimmed();const auto colon=line.indexOf(':');const auto key=line.left(colon).toLower();
                if(colon<=0||lines[i].startsWith(' ')||lines[i].startsWith('\t')||headers_.contains(key)){valid=false;break;}headers_[key]=line.mid(colon+1).trimmed();}
            bool number=true;length_=headers_.contains("content-length")?headers_["content-length"].toLongLong(&number):0;
            valid=valid&&number&&length_>=0&&length_<=Chunk&&!headers_.contains("transfer-encoding")&&!headers_.contains("expect")&&headers_.contains("host");
            if((method_=="POST"||method_=="PUT")&&!headers_.contains("content-length"))valid=false;
            if(!valid){handled_=true;json(400,{{"error","invalid_http_request"}});return;}
            buffer_.remove(0,pos+4);parsed_=true;
        }
        if(buffer_.size()<length_)return;handled_=true;
        if(buffer_.size()!=length_){json(400,{{"error","pipelining_not_supported"}});return;}
        service_->handle(this,method_,path_,headers_,buffer_);buffer_.clear();
    }
    FileStorageServiceHsx* service_;QSslSocket* socket_;QTimer* timer_;QByteArray buffer_,method_;
    QString path_;QMap<QByteArray,QByteArray> headers_;qint64 length_=0,remaining_=0;
    bool parsed_=false,handled_=false,replied_=false;std::unique_ptr<QFile> output_;
};
FileStorageServiceHsx::FileStorageServiceHsx(FileStorageOptionsHsx o):options_(std::move(o)){hashes_.setMaxThreadCount(2);}
FileStorageServiceHsx::~FileStorageServiceHsx(){stop();}
bool FileStorageServiceHsx::start(QString& error){
    try{
        QDir root(options_.directory);require(root.mkpath("temp")&&root.mkpath("objects"),500,"无法创建文件存储目录");
        lock_=std::make_unique<QLockFile>(root.filePath("files.lock"));require(lock_->tryLock(),500,"文件存储目录已由其他进程占用");
        QFile cert(options_.certificate),key(options_.key);require(cert.open(QIODevice::ReadOnly)&&key.open(QIODevice::ReadOnly),500,"无法读取文件服务 TLS 证书");
        auto certs=QSslCertificate::fromData(cert.readAll());auto bytes=key.readAll();QSslKey privateKey(bytes,QSsl::Rsa);if(privateKey.isNull())privateKey=QSslKey(bytes,QSsl::Ec);
        require(!certs.isEmpty()&&!privateKey.isNull(),500,"文件服务 TLS 证书无效");ssl_=QSslConfiguration::defaultConfiguration();ssl_.setProtocol(QSsl::TlsV1_2OrLater);ssl_.setLocalCertificateChain(certs);ssl_.setPrivateKey(privateKey);ssl_.setPeerVerifyMode(QSslSocket::VerifyNone);
        dbName_=newId();db_=QSqlDatabase::addDatabase("QSQLITE",dbName_);db_.setDatabaseName(root.filePath("files.sqlite"));require(db_.open(),500,"无法打开文件索引");
        query(db_,"PRAGMA journal_mode=WAL");query(db_,"PRAGMA synchronous=FULL");
        query(db_,"CREATE TABLE IF NOT EXISTS files(upload TEXT PRIMARY KEY,id TEXT UNIQUE,client TEXT,owner INTEGER,recipient INTEGER,source INTEGER,destination INTEGER,name TEXT,size INTEGER,sha TEXT,offset INTEGER,state TEXT,created INTEGER,updated INTEGER,UNIQUE(owner,client))");
        query(db_,"CREATE TABLE IF NOT EXISTS chunks(upload TEXT,offset INTEGER,size INTEGER,sha TEXT,PRIMARY KEY(upload,offset))");
        query(db_,"UPDATE files SET state='uploading' WHERE state='verifying'");
        // A final rename can precede the SQLite commit. Recover this one-way transition.
        auto pending=query(db_,"SELECT upload,id,offset,size,state FROM files WHERE state IN ('uploading','committing')");
        while(pending.next()){
            const auto id=pending.value(0).toString(),object=root.filePath("objects/"+pending.value(1).toString()+".bin");
            if(pending.value(4)=="committing"&&QFileInfo(object).size()==pending.value(3).toLongLong()&&QFile::exists(object)){query(db_,"UPDATE files SET state='ready' WHERE upload=?",{id});continue;}
            QFile part(root.filePath("temp/"+id+".part"));const qint64 offset=pending.value(2).toLongLong();
            if(part.open(QIODevice::ReadWrite)&&part.size()>=offset){part.resize(offset);query(db_,"UPDATE files SET state='uploading' WHERE upload=?",{id});}
            else query(db_,"UPDATE files SET state='failed' WHERE upload=?",{id});
        }
        auto* server=new Listener;listener_=server;server->setParent(this);server->accept=[this](qintptr fd){
            if(findChildren<QSslSocket*>().size()>=options_.maxConnections){QTcpSocket socket;socket.setSocketDescriptor(fd);socket.abort();return;}
            new FileHttpConnectionHsx(fd,ssl_,this);
        };
        require(server->listen(options_.address,options_.port),500,server->errorString());
        cleanup_=new QTimer(this);connect(cleanup_,&QTimer::timeout,this,&FileStorageServiceHsx::cleanup);cleanup_->start(60000);cleanup();return true;
    }catch(const HttpFailure& e){error=e.message;}catch(...){error="文件服务初始化失败";}return false;
}
void FileStorageServiceHsx::stop(){
    if(stopping_)return;stopping_=true;if(cleanup_)cleanup_->stop();if(listener_)listener_->close();
    for(auto* socket:findChildren<QSslSocket*>())socket->abort();hashes_.waitForDone();grants_.clear();
    db_.close();db_=QSqlDatabase();if(!dbName_.isEmpty())QSqlDatabase::removeDatabase(dbName_);lock_.reset();
}
void FileStorageServiceHsx::grant(const QString& token,const QJsonObject& permissions){if(!stopping_)grants_[token]=permissions;}
void FileStorageServiceHsx::revoke(const QString& token){grants_.remove(token);}
QJsonObject FileStorageServiceHsx::record(const QString& id,bool byFile){
    require(validId(id),404,"文件不存在");auto q=query(db_,"SELECT upload,id,client,owner,recipient,source,destination,name,size,sha,offset,state,created,updated FROM files WHERE "+QString(byFile?"id":"upload")+"=?",{id});
    require(q.next(),404,"文件不存在");QJsonObject o;const QStringList keys{"uploadId","fileId","clientId","sender","recipient","source","destination","name","size","sha256","offset","state","created","updated"};
    for(int i=0;i<keys.size();++i)o[keys[i]]=QJsonValue::fromVariant(q.value(i));return o;
}
QJsonObject FileStorageServiceHsx::publicRecord(const QJsonObject& row)const{auto out=row;out.remove("updated");return out;}
void FileStorageServiceHsx::cleanup(){
    try{auto q=query(db_,"SELECT upload FROM files WHERE state IN ('uploading','failed') AND updated<?",{QDateTime::currentSecsSinceEpoch()-options_.tempLifetimeSeconds});
        while(q.next()){const auto id=q.value(0).toString();QFile::remove(QDir(options_.directory).filePath("temp/"+id+".part"));query(db_,"DELETE FROM chunks WHERE upload=?",{id});query(db_,"UPDATE files SET state='expired' WHERE upload=?",{id});}
    }catch(...){/* Retry on next cleanup tick; never stop the chat service. */}
}
void FileStorageServiceHsx::handle(FileHttpConnectionHsx* c,const QByteArray& method,const QString& requestPath,const QMap<QByteArray,QByteArray>& headers,const QByteArray& body){
    try{
        const auto authorization=headers.value("authorization");require(authorization.startsWith("Bearer "),401,"请先登录");const auto token=QString::fromLatin1(authorization.mid(7));
        require(grants_.contains(token),401,"文件会话已失效，请重新连接");const auto grant=grants_[token];const int actor=grant["user"].toInt();require(actor>0,401,"身份无效");
        const QUrl url(requestPath);const auto path=url.path();const QUrlQuery params(url);const auto now=QDateTime::currentSecsSinceEpoch();const QDir root(options_.directory);
        auto own=[&](const QJsonObject& r){require(r["sender"].toInt()==actor,403,"无权管理此上传任务");};
        auto visible=[&](const QJsonObject& r){return (r["sender"].toInt()==actor&&grant["services"].toArray().contains(r["source"]))||(r["recipient"].toInt()==actor&&grant["services"].toArray().contains(r["destination"]));};
        if(path=="/v1/files"&&method=="GET"){
            QJsonArray list;auto q=query(db_,"SELECT upload FROM files WHERE (owner=? OR (recipient=? AND state='ready')) ORDER BY created DESC,upload DESC LIMIT 1000",{actor,actor});
            while(q.next()){const auto r=record(q.value(0).toString());if(visible(r))list.append(publicRecord(r));}c->json(200,{{"files",list},{"maxFileBytes",double(options_.maxFileBytes)},{"chunkSize",double(Chunk)}});return;
        }
        if(path=="/v1/uploads"&&method=="POST"){
            require(body.size()<=8192,413,"元数据过大");QJsonParseError parse;const auto doc=QJsonDocument::fromJson(body,&parse);require(parse.error==QJsonParseError::NoError&&doc.isObject(),400,"元数据无效");const auto d=doc.object();
            const auto client=d["clientId"].toString(),name=d["name"].toString(),hash=d["sha256"].toString();const double size=d["size"].toDouble(-1);const int service=d["service"].toInt(-1),target=d["recipient"].toInt();
            require(validId(client)&&nameValid(name)&&sha(hash)&&size>=0&&size<=options_.maxFileBytes&&size==qint64(size),400,"文件名、大小或 SHA-256 无效");
            const auto route=QString::number(service)+":"+QString::number(target);require(grant["routes"].toObject().contains(route),403,"只能向未屏蔽的当前服务好友发送文件");const int destination=grant["routes"].toObject()[route].toInt();
            auto old=query(db_,"SELECT upload FROM files WHERE owner=? AND client=?",{actor,client});
            if(old.next()){const auto r=record(old.value(0).toString());require(r["name"]==name&&r["size"].toDouble()==size&&r["sha256"]==hash&&r["recipient"].toInt()==target&&r["source"].toInt()==service,409,"任务编号内容冲突");c->json(200,publicRecord(r));return;}
            auto usage=query(db_,"SELECT COALESCE(SUM(size),0),COALESCE(SUM(CASE WHEN owner=? THEN size ELSE 0 END),0),SUM(CASE WHEN owner=? AND state IN ('uploading','verifying','committing') THEN 1 ELSE 0 END) FROM files WHERE state NOT IN ('cancelled','expired')",{actor,actor});usage.next();
            require(usage.value(0).toLongLong()+qint64(size)<=options_.quotaBytes&&usage.value(1).toLongLong()+qint64(size)<=options_.userQuotaBytes,507,"文件存储配额不足");require(usage.value(2).toInt()<4,429,"同时上传任务最多 4 个");
            const auto free=QStorageInfo(options_.directory).bytesAvailable();require(free<0||free>qint64(size)+64*1024*1024,507,"服务器磁盘空间不足");
            const auto id=newId(),fileId=newId();QFile f(root.filePath("temp/"+id+".part"));require(f.open(QIODevice::WriteOnly|QIODevice::NewOnly),500,"无法创建上传临时文件");f.close();
            try{query(db_,"INSERT INTO files VALUES(?,?,?,?,?,?,?,?,?,?,0,'uploading',?,?)",{id,fileId,client,actor,target,service,destination,name,qint64(size),hash,now,now});}catch(...){f.remove();throw;}
            c->json(201,publicRecord(record(id)));return;
        }
        if(path.startsWith("/v1/uploads/")){
            const auto parts=path.mid(12).split('/');const auto id=parts.value(0);auto r=record(id);own(r);const auto state=r["state"].toString();const auto partPath=root.filePath("temp/"+id+".part");
            if(method=="GET"&&parts.size()==1){c->json(200,publicRecord(r));return;}
            if(method=="DELETE"&&parts.size()==1){require(state!="ready"&&state!="committing"&&state!="verifying",409,"文件已发送或正在校验");require(!QFile::exists(partPath)||QFile::remove(partPath),500,"无法删除临时文件");query(db_,"DELETE FROM chunks WHERE upload=?",{id});query(db_,"UPDATE files SET state='cancelled',updated=? WHERE upload=?",{now,id});c->json(200,{{"state","cancelled"}});return;}
            require(grant["routes"].toObject().contains(QString::number(r["source"].toInt())+":"+QString::number(r["recipient"].toInt())),403,"好友关系已改变，不能继续发送");
            if(method=="PUT"&&parts.size()==1){
                require(state=="uploading",409,"任务当前不可上传");bool ok=false;const auto offset=headers.value("upload-offset").toLongLong(&ok);const auto hash=QString::fromLatin1(headers.value("x-chunk-sha256"));const auto confirmed=qint64(r["offset"].toDouble()),size=qint64(r["size"].toDouble());
                require(ok&&offset>=0&&offset%Chunk==0&&!body.isEmpty()&&body.size()==qMin<qint64>(Chunk,size-offset)&&sha(hash)&&QCryptographicHash::hash(body,QCryptographicHash::Sha256).toHex()==hash.toLatin1(),400,"数据块长度或校验无效");
                if(offset<confirmed){auto q=query(db_,"SELECT sha,size FROM chunks WHERE upload=? AND offset=?",{id,offset});require(q.next()&&q.value(0)==hash&&q.value(1).toLongLong()==body.size(),409,"数据块冲突");c->json(200,publicRecord(r));return;}
                require(offset==confirmed,409,"请从服务器确认的偏移继续");QFile f(partPath);require(f.open(QIODevice::ReadWrite)&&f.seek(offset)&&f.write(body)==body.size()&&f.flush(),507,"服务器写入失败");f.close();
                require(db_.transaction(),500,"无法保存上传状态");try{query(db_,"INSERT INTO chunks VALUES(?,?,?,?)",{id,offset,body.size(),hash});query(db_,"UPDATE files SET offset=?,updated=? WHERE upload=?",{offset+body.size(),now,id});require(db_.commit(),500,"上传状态提交失败");}catch(...){db_.rollback();throw;}
                c->json(200,publicRecord(record(id)));return;
            }
            if(method=="POST"&&parts.size()==2&&parts[1]=="complete"){
                if(state=="ready"||state=="verifying"){c->json(200,publicRecord(r));return;}
                require(state=="uploading"&&r["offset"]==r["size"],409,"文件尚未完整上传");require(hashes_.activeThreadCount()<2,429,"校验繁忙，请稍后重试");
                query(db_,"UPDATE files SET state='verifying',updated=? WHERE upload=?",{now,id});const auto expected=r["sha256"].toString();const auto size=qint64(r["size"].toDouble());
                hashes_.start([this,id,partPath,expected,size]{QFile f(partPath);QCryptographicHash hash(QCryptographicHash::Sha256);bool ok=f.open(QIODevice::ReadOnly)&&f.size()==size;
                    while(ok&&!f.atEnd()){const auto block=f.read(1024*1024);if(block.isEmpty()&&f.error()!=QFile::NoError){ok=false;break;}hash.addData(block);}f.close();ok=ok&&hash.result().toHex()==expected.toLatin1();
                    QMetaObject::invokeMethod(this,[this,id,partPath,ok]{if(stopping_)return;try{auto r=record(id);
                        if(!ok){query(db_,"UPDATE files SET state='failed' WHERE upload=?",{id});return;}
                        query(db_,"UPDATE files SET state='committing' WHERE upload=?",{id});const auto dest=QDir(options_.directory).filePath("objects/"+r["fileId"].toString()+".bin");
                        if(!QFile::rename(partPath,dest)){query(db_,"UPDATE files SET state='failed' WHERE upload=?",{id});return;}
                        query(db_,"UPDATE files SET state='ready',updated=? WHERE upload=?",{QDateTime::currentSecsSinceEpoch(),id});emit fileReady(r["sender"].toInt(),r["recipient"].toInt(),r["fileId"].toString());
                    }catch(...){/* Durable state remains recoverable at restart. */}},Qt::QueuedConnection);
                });c->json(202,publicRecord(record(id)));return;
            }
        }
        if(path.startsWith("/v1/files/")&&method=="GET"){
            const auto r=record(path.mid(10),true);require(r["state"]=="ready"&&visible(r),403,"无权下载此文件");const auto size=qint64(r["size"].toDouble());qint64 start=0,end=size-1;const auto range=headers.value("range");
            require(!headers.contains("if-range")||headers["if-range"]==('"'+r["sha256"].toString().toLatin1()+'"'),412,"文件版本不匹配");
            if(!range.isEmpty()){
                static QRegularExpression re("^bytes=([0-9]+)-([0-9]*)$");const auto match=re.match(QString::fromLatin1(range));bool a=false,b=true;start=match.captured(1).toLongLong(&a);end=match.captured(2).isEmpty()?size-1:match.captured(2).toLongLong(&b);
                if(!match.hasMatch()||!a||!b||start<0||start>=size||end<start){c->reply(416,{},"application/octet-stream","Content-Range: bytes */"+QByteArray::number(size)+"\r\n");return;}end=qMin(end,size-1);
            }
            c->file(root.filePath("objects/"+r["fileId"].toString()+".bin"),start,end,size,r["sha256"].toString(),!range.isEmpty());return;
        }
        throw HttpFailure{404,"接口不存在"};
    }catch(const HttpFailure& e){c->json(e.code,{{"error",e.message}});}catch(...){c->json(500,{{"error","文件存储操作失败，请重试"}});}
}
}
