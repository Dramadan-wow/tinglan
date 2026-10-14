#include "ChatServerHsx.h"
#include "WorkspaceBackendHsx.h"
#include "PasswordHsx.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QSslSocket>
#include <QSslKey>
#include <QSslError>
#include <QTcpSocket>
#include <stdexcept>
namespace im::net {
ChatServerHsx::ChatServerHsx(AuthStoreHsx& auth,const ServerOptionsHsx& options,QObject* parent)
    :QTcpServer(parent),auth_(auth),options_(options){
    clock_.start();workers_.setMaxThreadCount(4);timer_.setInterval(1000);connect(&timer_,&QTimer::timeout,this,&ChatServerHsx::tick);
}
ChatServerHsx::~ChatServerHsx(){shutdown();workers_.waitForDone();}
bool ChatServerHsx::start(QString& failure){
    if(options_.tls){
        if(!QSslSocket::supportsSsl()){failure="TLS backend unavailable";return false;}
        QFile certFile(options_.certificateFile),keyFile(options_.keyFile);
        if(!certFile.open(QIODevice::ReadOnly)||!keyFile.open(QIODevice::ReadOnly)){failure="TLS certificate/key cannot be read";return false;}
        const auto certs=QSslCertificate::fromData(certFile.readAll());const auto keyBytes=keyFile.readAll();
        QSslKey key(keyBytes,QSsl::Rsa);if(key.isNull())key=QSslKey(keyBytes,QSsl::Ec);
        if(certs.isEmpty()||key.isNull()){failure="TLS certificate/key invalid (PEM RSA or EC required)";return false;}
        ssl_=QSslConfiguration::defaultConfiguration();ssl_.setProtocol(QSsl::TlsV1_2OrLater);
        ssl_.setLocalCertificateChain(certs);ssl_.setPrivateKey(key);ssl_.setPeerVerifyMode(QSslSocket::VerifyNone);
    }else if(!options_.allowInsecureLoopback||!options_.address.isLoopback()){
        failure="Plaintext is allowed only on an explicitly enabled loopback listener";return false;
    }
    if(!listen(options_.address,options_.port)){failure=errorString();return false;}
    timer_.start();emit logLine(QString("listening address=%1 port=%2 tls=%3").arg(serverAddress().toString()).arg(serverPort()).arg(options_.tls));return true;
}
void ChatServerHsx::shutdown(){timer_.stop();close();const auto sockets=peers_.keys();for(auto* s:sockets)s->abort();}
void ChatServerHsx::incomingConnection(qintptr descriptor){
    QTcpSocket* socket=options_.tls?static_cast<QTcpSocket*>(new QSslSocket(this)):new QTcpSocket(this);
    if(!socket->setSocketDescriptor(descriptor)){socket->deleteLater();return;}
    if(peers_.size()>=options_.maxConnections||(!options_.tls&&!socket->peerAddress().isLoopback())){socket->abort();socket->deleteLater();return;}
    auto peer=std::make_shared<Peer>();peer->socket=socket;peer->created=peer->lastActive=peer->window=clock_.elapsed();peers_[socket]=peer;
    socket->setReadBufferSize(2*MaxFrame+4);socket->setSocketOption(QAbstractSocket::KeepAliveOption,1);
    connect(socket,&QTcpSocket::readyRead,this,[this,socket]{read(socket);});
    connect(socket,&QTcpSocket::disconnected,this,[this,socket,peer]{
        if(!peer->user.isEmpty()&&online_.value(peer->user)==socket)online_.remove(peer->user);
        try{if(workspace_&&peer->workspaceUser)workspace_->online(peer->workspaceUser,false,im::ServiceHsx(peer->service));
            if(workspace_&&peer->administrator)workspace_->loginAudit({},0,im::ServiceHsx::Count,"管理员登出 / 连接断开",true);
        }catch(const std::exception&){emit logLine("logout_audit_failed");}
        emit logLine("disconnected user="+peer->user);peers_.remove(socket);socket->deleteLater();
        if(peer->workspaceUser||peer->administrator)for(const auto& other:peers_.values())if(other->workspaceUser||other->administrator)synchronize(other);
    });
    connect(socket,&QTcpSocket::errorOccurred,this,[this,socket](QAbstractSocket::SocketError){emit logLine("socket_error "+socket->errorString());socket->abort();});
    if(auto* ssl=qobject_cast<QSslSocket*>(socket)){
        ssl->setSslConfiguration(ssl_);connect(ssl,&QSslSocket::sslErrors,this,[ssl](const QList<QSslError>&){ssl->abort();});ssl->startServerEncryption();
    }
    emit logLine("connected peer="+socket->peerAddress().toString());
}
bool ChatServerHsx::send(const std::shared_ptr<Peer>& peer,const QJsonObject& message){
    auto* socket=peer->socket.data();if(!socket||socket->state()!=QAbstractSocket::ConnectedState||peer->closing)return false;
    const auto frame=encode(message);const auto* ssl=qobject_cast<QSslSocket*>(socket);
    if(socket->bytesToWrite()+(ssl?ssl->encryptedBytesToWrite():0)+frame.size()>MaxQueue){socket->abort();return false;}
    if(socket->write(frame)!=frame.size()){socket->abort();return false;}return true;
}
void ChatServerHsx::error(const std::shared_ptr<Peer>& peer,const QString& code,const QString& request,bool closing){
    send(peer,envelope("error",{{"code",code},{"requestId",request}}, {},peer->user));
    if(closing&&peer->socket){peer->closing=true;peer->socket->disconnectFromHost();const QPointer<QTcpSocket> socket=peer->socket;QTimer::singleShot(1000,this,[socket]{if(socket)socket->abort();});}
}
void ChatServerHsx::read(QTcpSocket* socket){
    auto peer=peers_.value(socket);if(!peer||peer->closing)return;
    if(auto* ssl=qobject_cast<QSslSocket*>(socket);ssl&&!ssl->isEncrypted())return;
    try{
        int budget=4;
        while(peer->socket&&socket->bytesAvailable()&&budget--&&!peer->closing){
            QList<QJsonObject> messages;QString problem;
            if(!peer->decoder.feed(socket->read(MaxFrame),messages,problem)){error(peer,problem,{},true);return;}
            for(const auto& m:messages){if(!peer->socket||peer->closing||socket->state()!=QAbstractSocket::ConnectedState)return;
                const auto now=clock_.elapsed();if(now-peer->window>=1000){peer->window=now;peer->count=0;}
                if(++peer->count>100){error(peer,"rate_limit",{},true);return;}
                handle(peer,m);
            }
        }
        if(peer->socket&&socket->bytesAvailable()&&!peer->closing){const QPointer<QTcpSocket> guarded=socket;QTimer::singleShot(0,this,[this,guarded]{if(guarded)read(guarded);});}
    }catch(const std::exception&){emit logLine("request_exception");error(peer,"internal_error",{},true);}
}
void ChatServerHsx::handle(const std::shared_ptr<Peer>& peer,const QJsonObject& m){
    const auto type=m.value("type").toString(),id=m.value("id").toString();
    if(type=="login"){login(peer,m);return;}
    if(peer->user.isEmpty()){error(peer,"not_authenticated",id);return;}
    // A claimed sender can never override the authenticated connection identity.
    if(m.value("senderId").toString()!=peer->user){error(peer,"sender_mismatch",id);return;}
    if(type=="heartbeat"){peer->lastActive=clock_.elapsed();send(peer,envelope("heartbeat",{{"requestId",id}}, {},peer->user));}
    else if(type=="workspace_command"&&workspace_&&(peer->workspaceUser||peer->administrator)){
        peer->lastActive=clock_.elapsed();const auto data=m["data"].toObject();QJsonObject result;
        try{result=workspace_->command(peer->workspaceUser,peer->administrator?im::ServiceHsx::Count:im::serviceType(data.value("service").toInt(peer->service)),id,data,peer->administrator);}
        catch(const std::exception& e){result={{"ok",false},{"detail",QString::fromUtf8(e.what())}};}
        result["requestId"]=id;send(peer,envelope("workspace_result",result,{},peer->user));
        if(result["ok"].toBool()){
            if(peer->administrator){for(const auto& other:peers_.values())if((result["revokeUser"].toInt()>0&&other->workspaceUser==result["revokeUser"].toInt())||(result["revokeAdmins"].toBool()&&other->administrator&&other!=peer))error(other,"session_revoked",{},true);}
            if(data["op"]=="sync")synchronize(peer);else for(const auto& other:peers_.values())if(other->workspaceUser||other->administrator)synchronize(other);
        }
    }
    else if(type=="chat"&&!peer->workspaceUser&&!peer->administrator){peer->lastActive=clock_.elapsed();chat(peer,m);}
    else error(peer,"unknown_type",id);
}
void ChatServerHsx::login(const std::shared_ptr<Peer>& peer,const QJsonObject& message){
    if(message["data"].toObject()["workspace"].toBool()){workspaceLogin(peer,message);return;}
    const auto request=message.value("id").toString();
    if(!peer->user.isEmpty()||peer->authenticating){error(peer,"login_state",request);return;}
    const auto data=message.value("data").toObject();const auto username=data.value("username").toString(),password=data.value("password").toString();
    if(!validUser(username)||!data.value("password").isString()||password.toUtf8().size()<12||password.toUtf8().size()>128||
       !message.value("senderId").toString().isEmpty()||!message.value("recipientId").toString().isEmpty()) {error(peer,"invalid_login",request,true);return;}
    const auto ip=peer->socket->peerAddress().toString();const auto now=clock_.elapsed();
    if(!attempts_.contains(ip)&&attempts_.size()>=1024){error(peer,"server_busy",request,true);return;}
    auto& attempt=attempts_[ip];if(now-attempt.first>60000)attempt={now,0};
    if(++attempt.second>20){error(peer,"login_rate_limit",request,true);return;}
    if(authJobs_>=16){error(peer,"server_busy",request,true);return;}
    peer->authenticating=true;++authJobs_;
    const auto record=auth_.record(username);const bool exists=auth_.contains(username);
    // PBKDF2 is deliberately expensive: execute it outside the event loop, with bounded concurrency.
    workers_.start([this,peer,record,password,exists,username,request]{
        bool valid=false;try{valid=AuthStoreHsx::verify(record,password)&&exists;}catch(...){valid=false;}
        QMetaObject::invokeMethod(this,[this,peer,valid,username,request]{
            --authJobs_;if(!peer->socket||!peers_.contains(peer->socket.data())||peer->closing)return;
            peer->authenticating=false;const bool duplicate=online_.contains(username);const bool ok=valid&&!duplicate;
            if(ok){peer->user=username;peer->lastActive=clock_.elapsed();online_[username]=peer->socket.data();}
            send(peer,envelope("login_result",{{"requestId",request},{"ok",ok},{"userId",ok?username:QString{}},{"code",ok?"ok":valid?"already_online":"invalid_credentials"}}, {},ok?username:QString{}));
            emit logLine(ok?"authenticated user="+username:"authentication_failed");
            if(!ok&&peer->socket){peer->closing=true;peer->socket->disconnectFromHost();}
        },Qt::QueuedConnection);
    });
}
void ChatServerHsx::chat(const std::shared_ptr<Peer>& peer,const QJsonObject& m){
    const auto id=m.value("id").toString(),target=m.value("recipientId").toString();const auto data=m.value("data").toObject();const auto text=data.value("text").toString();
    auto ack=[&](bool ok,const QString& code,bool duplicate=false){send(peer,envelope("message_ack",{{"messageId",id},{"ok",ok},{"code",code},{"duplicate",duplicate}}, {},peer->user));};
    if(!validUser(target)||!data.value("text").isString()||text.trimmed().isEmpty()||text.toUtf8().size()>MaxText){ack(false,"invalid_chat");return;}
    const auto key=peer->user+":"+id;
    const auto digest=QCryptographicHash::hash(QJsonDocument(QJsonObject{{"to",target},{"text",text}}).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256);
    const auto previous=seen_.constFind(key);if(previous!=seen_.cend()){if(previous->digest!=digest)ack(false,"id_conflict");else ack(true,"forwarded",true);return;}
    if(seen_.size()>=10000){ack(false,"dedup_capacity");return;}
    const auto other=peers_.value(online_.value(target));if(!other||other->closing){ack(false,"recipient_offline");return;}
    const auto forwarded=envelope("chat",{{"text",text},{"serverTime",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}},peer->user,target,id);
    if(!send(other,forwarded)){ack(false,"recipient_unavailable");return;}
    seen_[key]={digest,clock_.elapsed()};ack(true,"forwarded");
    emit logLine("forwarded id="+id+" from="+peer->user+" to="+target);
}

void ChatServerHsx::workspaceLogin(const std::shared_ptr<Peer>& peer,const QJsonObject& message){
    const auto data=message["data"].toObject();const auto nickname=data["username"].toString().trimmed(),password=data["password"].toString(),request=message["id"].toString();const bool registration=data["register"].toBool(),administrator=data["admin"].toBool();const int service=data["service"].toInt(-1);
    if(!workspace_||!peer->user.isEmpty()||peer->authenticating||nickname.isEmpty()||nickname.toUtf8().size()>60||password.isEmpty()||password.toUtf8().size()>128||service<0||service>=im::ActiveServiceCountHsx||!message["senderId"].toString().isEmpty()||!message["recipientId"].toString().isEmpty()||(registration&&(administrator||password.toUtf8().size()<8))){error(peer,"invalid_login",request,true);return;}
    const auto ip=peer->socket->peerAddress().toString();const auto now=clock_.elapsed();
    if(!attempts_.contains(ip)&&attempts_.size()>=1024){error(peer,"server_busy",request,true);return;}
    auto& attempt=attempts_[ip];if(now-attempt.first>60000)attempt={now,0};
    if(++attempt.second>20||authJobs_>=16){error(peer,"login_rate_limit",request,true);return;}
    const int user=administrator?0:workspace_->resolve(nickname,im::serviceType(service));
    static const auto dummy=im::PasswordHsx::hash("unmatched-login-placeholder");
    const auto adminRecord=administrator?workspace_->adminCredential(nickname):std::string{};
    const auto record=administrator?(adminRecord.empty()?dummy:adminRecord):(user?workspace_->credential(user,im::serviceType(service)):dummy);
    peer->authenticating=true;++authJobs_;
    workers_.start([this,peer,nickname,password,request,registration,service,user,record,administrator,adminRecord]{
        bool valid=false;std::string hash;
        try{if(registration){hash=im::PasswordHsx::hash(password.toStdString());valid=true;}else valid=im::PasswordHsx::verify(password.toStdString(),record)&&(administrator?!adminRecord.empty():user!=0);}catch(...){}
        QMetaObject::invokeMethod(this,[this,peer,nickname,request,registration,service,user,record,valid,hash,administrator]{
            --authJobs_;if(!peer->socket||!peers_.contains(peer->socket.data())||peer->closing)return;peer->authenticating=false;
            int authenticated=user;QString reason="账号或密码不正确";
            try{if(!valid)throw std::runtime_error("账号或密码不正确");
                if(administrator){
                    if(workspace_->adminCredential(nickname)!=record)throw std::runtime_error("管理员凭据已改变，请重新登录");
                    workspace_->loginAudit(nickname,0,im::ServiceHsx::Count,"管理员登录成功",true);
                    peer->administrator=true;peer->user="workspace_admin";peer->lastActive=clock_.elapsed();
                    send(peer,envelope("login_result",{{"requestId",request},{"ok",true},{"userId",peer->user},{"workspace",true},{"admin",true}}, {},peer->user));
                    for(const auto& other:peers_.values())if(other->administrator)synchronize(other);return;
                }
                if(registration)authenticated=workspace_->registerUser(nickname,hash,im::serviceType(service));
                else if(workspace_->resolve(nickname,im::serviceType(service))!=user||workspace_->credential(user,im::serviceType(service))!=record)throw std::runtime_error("账号信息已变更，请重新登录");
                const auto stable="u"+QString::number(authenticated);if(online_.contains(stable))throw std::runtime_error("账号已在另一台设备登录");
                peer->workspaceUser=authenticated;peer->service=service;peer->user=stable;peer->lastActive=clock_.elapsed();online_[stable]=peer->socket.data();workspace_->online(authenticated,true,im::serviceType(service));
                send(peer,envelope("login_result",{{"requestId",request},{"ok",true},{"userId",stable},{"numericId",authenticated},{"workspace",true}}, {},stable));for(const auto& other:peers_.values())if(other->workspaceUser||other->administrator)synchronize(other);return;
            }catch(const std::exception& e){reason=QString::fromUtf8(e.what());}
            try{workspace_->loginAudit(nickname,user,im::ServiceHsx(service),registration?"注册失败":administrator?"管理员登录失败":"用户登录失败："+reason,administrator);}catch(const std::exception&){emit logLine("login_audit_failed");}
            for(const auto& other:peers_.values())if(other->administrator)synchronize(other);
            send(peer,envelope("login_result",{{"requestId",request},{"ok",false},{"code","invalid_credentials"},{"detail",reason}}));peer->closing=true;peer->socket->disconnectFromHost();
        },Qt::QueuedConnection);
    });
}
void ChatServerHsx::synchronize(const std::shared_ptr<Peer>& peer){
    if(!peer->socket||(!peer->workspaceUser&&!peer->administrator)||peer->closing)return;if(peer->syncing){peer->resync=true;return;}
    QByteArray payload;try{payload=QJsonDocument(peer->administrator?workspace_->adminSnapshot():workspace_->snapshot(peer->workspaceUser)).toJson(QJsonDocument::Compact);}
    catch(const std::exception&){emit logLine("snapshot_failed");error(peer,"internal_error",{},true);return;}
    if(payload.size()>16*1024*1024){error(peer,"snapshot_too_large",{},true);return;}
    peer->syncing=true;sendSnapshot(peer,payload,newId(),0);
}
void ChatServerHsx::sendSnapshot(const std::shared_ptr<Peer>& peer,const QByteArray& payload,const QString& id,int part){
    if(!peer->socket||peer->closing||peer->socket->state()!=QAbstractSocket::ConnectedState){peer->syncing=false;return;}
    constexpr int chunk=24000;const int count=(payload.size()+chunk-1)/chunk;
    if(part>=count){peer->syncing=false;if(peer->resync){peer->resync=false;synchronize(peer);}return;}
    if(peer->socket->bytesToWrite()<100000){if(!send(peer,envelope("workspace_sync",{{"snapshot",id},{"part",part},{"count",count},{"payload",QString::fromLatin1(payload.mid(part*chunk,chunk).toBase64())}}, {},peer->user))){peer->syncing=false;return;}++part;}
    QTimer::singleShot(5,this,[this,peer,payload,id,part]{sendSnapshot(peer,payload,id,part);});
}

void ChatServerHsx::tick(){
    const auto now=clock_.elapsed();const auto peers=peers_.values();
    for(const auto& p:peers)if(p->socket&&((p->user.isEmpty()&&now-p->created>options_.loginTimeoutMs)||(!p->user.isEmpty()&&now-p->lastActive>options_.idleTimeoutMs))){emit logLine("timeout user="+p->user);p->socket->abort();}
    for(auto it=seen_.begin();it!=seen_.end();)if(now-it->time>600000)it=seen_.erase(it);else++it;
    for(auto it=attempts_.begin();it!=attempts_.end();)if(now-it->first>60000)it=attempts_.erase(it);else++it;
}
}
