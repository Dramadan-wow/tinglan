#include "ChatServerHsx.h"
#include "WorkspaceBackendHsx.h"
#include "PasswordHsx.h"
#include "UpdateStoreHsx.h"
#include "UpdateInfoHsx.h"
#include "FileStorageServiceHsx.h"
#include <QRandomGenerator>
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
        if(files_&&!peer->fileToken.isEmpty()){const auto token=peer->fileToken;QMetaObject::invokeMethod(files_,[service=files_,token]{service->revoke(token);},Qt::QueuedConnection);}
        if(!peer->user.isEmpty()&&online_.value(peer->user)==socket)online_.remove(peer->user);
        if(!peer->onlineKey.isEmpty()&&online_.value(peer->onlineKey)==socket)online_.remove(peer->onlineKey);
        if(!peer->qrCode.isEmpty())tickets_.remove(peer->qrCode);
        // The account stays online while its other device (or the connection that replaced this one) is connected.
        bool elsewhere=false;for(const auto& other:peers_.values())if(other!=peer&&peer->workspaceUser&&other->workspaceUser==peer->workspaceUser)elsewhere=true;
        try{if(workspace_&&peer->workspaceUser&&!elsewhere)workspace_->online(peer->workspaceUser,false,im::ServiceHsx(peer->service));
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
    if(type=="update_check"||type=="update_chunk"){update(peer,m);return;}
    if(peer->updater){error(peer,"update_connection_only",id,true);return;}
    if(type=="login"){login(peer,m);return;}
    if(type=="qr_start"){qrStart(peer,m);return;}
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
            refreshFileGrants();
            if(peer->administrator){for(const auto& other:peers_.values())if((result["revokeUser"].toInt()>0&&other->workspaceUser==result["revokeUser"].toInt())||(result["revokeAdmins"].toBool()&&other->administrator&&other!=peer))error(other,"session_revoked",{},true);}
            if(data["op"]=="sync")synchronize(peer);else for(const auto& other:peers_.values())if(other->workspaceUser||other->administrator)synchronize(other);
        }
    }
    else if(type=="chat"&&!peer->workspaceUser&&!peer->administrator){peer->lastActive=clock_.elapsed();chat(peer,m);}
    else if((type=="qr_scan"||type=="qr_confirm")&&peer->workspaceUser){peer->lastActive=clock_.elapsed();qrAction(peer,m);}
    else if(type=="logout"&&peer->workspaceUser&&workspace_){
        // Signing out on this device forgets its automatic sign-in; the connection closes after the reply.
        try{workspace_->forgetToken(peer->workspaceUser,m["data"].toObject()["token"].toString());}catch(const std::exception&){emit logLine("token_forget_failed");}
        send(peer,envelope("logout_result",{{"requestId",id},{"ok",true}}, {},peer->user));peer->closing=true;peer->socket->disconnectFromHost();
    }
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
    const auto data=message["data"].toObject();
    if(!peer->authenticating&&peer->user.isEmpty()){
        const auto device=data["device"].toString();peer->legacyClient=!data.contains("device");
        peer->device=device=="desktop"||device=="mobile"?device:data["fileTransfer"].toInt()==1?"desktop":"mobile";peer->wantsFiles=data["fileTransfer"].toInt()==1;
    }
    if(data.contains("token")){tokenLogin(peer,message);return;}
    const auto nickname=data["username"].toString().trimmed(),password=data["password"].toString(),request=message["id"].toString();const bool registration=data["register"].toBool(),administrator=data["admin"].toBool();const int service=data["service"].toInt(-1);
    const bool remember=data["remember"].toBool()&&peer->device=="mobile";
    if(!workspace_||!peer->user.isEmpty()||peer->authenticating||nickname.isEmpty()||nickname.toUtf8().size()>60||password.isEmpty()||password.toUtf8().size()>128||service<0||service>=im::ActiveServiceCountHsx||!message["senderId"].toString().isEmpty()||!message["recipientId"].toString().isEmpty()||(registration&&(administrator||password.toUtf8().size()<qsizetype(im::PasswordHsx::RemoteMinimum)))){error(peer,"invalid_login",request,true);return;}
    const auto ip=peer->socket->peerAddress().toString();const auto now=clock_.elapsed();
    if(!attempts_.contains(ip)&&attempts_.size()>=1024){error(peer,"server_busy",request,true);return;}
    auto& attempt=attempts_[ip];if(now-attempt.first>60000)attempt={now,0};
    if(++attempt.second>20||authJobs_>=16){error(peer,"login_rate_limit",request,true);return;}
    const int user=administrator?0:workspace_->resolve(nickname,im::serviceType(service));
    static const auto dummy=im::PasswordHsx::hash("unmatched-login-placeholder");
    const auto adminRecord=administrator?workspace_->adminCredential(nickname):std::string{};
    const auto record=administrator?(adminRecord.empty()?dummy:adminRecord):(user?workspace_->credential(user,im::serviceType(service)):dummy);
    peer->authenticating=true;++authJobs_;
    workers_.start([this,peer,nickname,password,request,registration,service,user,record,administrator,adminRecord,remember]{
        bool valid=false;std::string hash,upgraded;
        try{if(registration){hash=im::PasswordHsx::hash(password.toStdString());valid=true;}else valid=im::PasswordHsx::verify(password.toStdString(),record)&&(administrator?!adminRecord.empty():user!=0);
            // Rehash older 100000-round digests here, off the event loop, while the plain password is still known.
            if(valid&&!registration&&im::PasswordHsx::outdated(record))upgraded=im::PasswordHsx::hash(password.toStdString(),1);}catch(...){}
        QMetaObject::invokeMethod(this,[this,peer,nickname,request,registration,service,user,record,valid,hash,upgraded,administrator,remember]{
            --authJobs_;if(!peer->socket||!peers_.contains(peer->socket.data())||peer->closing)return;peer->authenticating=false;
            int authenticated=user;QString reason="账号或密码不正确";
            auto upgrade=[&](int owner){if(upgraded.empty())return;try{workspace_->upgradeCredential(owner,im::ServiceHsx(service),record,upgraded);emit logLine("credential_upgraded");}catch(const std::exception&){emit logLine("credential_upgrade_failed");}};
            try{if(!valid)throw std::runtime_error("账号或密码不正确");
                if(administrator){
                    if(workspace_->adminCredential(nickname)!=record)throw std::runtime_error("管理员凭据已改变，请重新登录");
                    upgrade(0);
                    workspace_->loginAudit(nickname,0,im::ServiceHsx::Count,"管理员登录成功",true);
                    peer->administrator=true;peer->user="workspace_admin";peer->lastActive=clock_.elapsed();
                    send(peer,envelope("login_result",{{"requestId",request},{"ok",true},{"userId",peer->user},{"workspace",true},{"admin",true}}, {},peer->user));
                    for(const auto& other:peers_.values())if(other->administrator)synchronize(other);return;
                }
                if(registration)authenticated=workspace_->registerUser(nickname,hash,im::serviceType(service));
                else if(workspace_->resolve(nickname,im::serviceType(service))!=user||workspace_->credential(user,im::serviceType(service))!=record)throw std::runtime_error("账号信息已变更，请重新登录");
                if(!registration)upgrade(authenticated);
                QJsonObject extra;if(remember)extra["token"]=workspace_->issueToken(authenticated,im::serviceType(service),"mobile",30LL*86400);
                admit(peer,authenticated,service,request,extra);return;
            }catch(const std::exception& e){reason=QString::fromUtf8(e.what());}
            try{workspace_->loginAudit(nickname,user,im::ServiceHsx(service),registration?"注册失败":administrator?"管理员登录失败":"用户登录失败："+reason,administrator);}catch(const std::exception&){emit logLine("login_audit_failed");}
            for(const auto& other:peers_.values())if(other->administrator)synchronize(other);
            send(peer,envelope("login_result",{{"requestId",request},{"ok",false},{"code","invalid_credentials"},{"detail",reason}}));peer->closing=true;peer->socket->disconnectFromHost();
        },Qt::QueuedConnection);
    });
}
bool ChatServerHsx::throttle(const std::shared_ptr<Peer>& peer,const QString& request){
    const auto ip=peer->socket->peerAddress().toString();const auto now=clock_.elapsed();
    if(!attempts_.contains(ip)&&attempts_.size()>=1024){error(peer,"server_busy",request,true);return false;}
    auto& attempt=attempts_[ip];if(now-attempt.first>60000)attempt={now,0};
    if(++attempt.second>20){error(peer,"login_rate_limit",request,true);return false;}return true;
}
void ChatServerHsx::tokenLogin(const std::shared_ptr<Peer>& peer,const QJsonObject& message){
    const auto data=message["data"].toObject();const auto request=message["id"].toString();
    if(!workspace_||!peer->user.isEmpty()||peer->authenticating||!peer->qrCode.isEmpty()||data["register"].toBool()||data["admin"].toBool()||!data["token"].isString()||
       !message["senderId"].toString().isEmpty()||!message["recipientId"].toString().isEmpty()){error(peer,"invalid_login",request,true);return;}
    if(!throttle(peer,request))return;
    int service=0;QString device;int user=0;
    try{user=workspace_->redeemToken(data["token"].toString(),service,device);}catch(const std::exception&){emit logLine("token_check_failed");}
    if(!user){
        send(peer,envelope("login_result",{{"requestId",request},{"ok",false},{"code","invalid_token"},{"detail","自动登录已失效，请重新输入密码登录"}}));
        peer->closing=true;peer->socket->disconnectFromHost();return;
    }
    peer->device=device;admit(peer,user,service,request,{});
}
void ChatServerHsx::admit(const std::shared_ptr<Peer>& peer,int user,int service,const QString& request,QJsonObject result){
    const auto stable="u"+QString::number(user);const auto key=stable+":"+peer->device;
    // A newer sign-in on the same kind of device replaces the older connection (also a stale one after a network drop).
    if(const auto old=peers_.value(online_.value(key));old&&old!=peer){
        online_.remove(key);old->onlineKey.clear();emit logLine("session_replaced user="+stable+" device="+peer->device);
        error(old,old->legacyClient?"session_revoked":"session_replaced",{},true);
    }
    peer->workspaceUser=user;peer->service=service;peer->user=stable;peer->onlineKey=key;peer->lastActive=clock_.elapsed();online_[key]=peer->socket.data();
    workspace_->online(user,true,im::serviceType(service));
    QJsonObject fileSession;
    if(files_&&options_.tls&&peer->wantsFiles){QByteArray secret(32,Qt::Uninitialized);for(auto& b:secret)b=char(QRandomGenerator::system()->generate());peer->fileToken=QString::fromLatin1(secret.toHex());refreshFileGrants();fileSession={{"port",filePort_},{"token",peer->fileToken},{"user",user}};}
    result["requestId"]=request;result["ok"]=true;result["userId"]=stable;result["numericId"]=user;result["workspace"]=true;result["files"]=fileSession;result["service"]=service;result["device"]=peer->device;
    send(peer,envelope("login_result",result,{},stable));emit logLine("authenticated user="+stable+" device="+peer->device);
    for(const auto& other:peers_.values())if(other->workspaceUser||other->administrator)synchronize(other);
}
void ChatServerHsx::qrStart(const std::shared_ptr<Peer>& peer,const QJsonObject& m){
    const auto id=m["id"].toString();const auto data=m["data"].toObject();const int service=data["service"].toInt(-1);
    if(!workspace_||!peer->user.isEmpty()||peer->authenticating||!peer->qrCode.isEmpty()||service<0||service>=im::ActiveServiceCountHsx||
       !m["senderId"].toString().isEmpty()||!m["recipientId"].toString().isEmpty()){error(peer,"invalid_qr_request",id,true);return;}
    if(!throttle(peer,id))return;
    if(tickets_.size()>=256){error(peer,"server_busy",id,true);return;}
    QByteArray secret(16,Qt::Uninitialized);for(auto& b:secret)b=char(QRandomGenerator::system()->generate());const auto code=QString::fromLatin1(secret.toHex());
    constexpr qint64 lifetime=120000;
    peer->device="desktop";peer->legacyClient=false;peer->wantsFiles=data["fileTransfer"].toInt()==1;peer->qrCode=code;peer->qrRequest=id;peer->qrUntil=clock_.elapsed()+lifetime;
    tickets_[code]={peer,service,peer->qrUntil,0};
    send(peer,envelope("qr_ticket",{{"requestId",id},{"code",code},{"expires",int(lifetime/1000)}}));
}
void ChatServerHsx::qrAction(const std::shared_ptr<Peer>& peer,const QJsonObject& m){
    const auto id=m["id"].toString();const auto data=m["data"].toObject();const auto code=data["code"].toString();
    auto reply=[&](bool ok,const QString& detail,QJsonObject extra){extra["requestId"]=id;extra["ok"]=ok;extra["detail"]=detail;send(peer,envelope("qr_result",extra,{},peer->user));};
    const auto ticket=tickets_.find(code);const auto desktop=ticket==tickets_.end()?nullptr:ticket->peer.lock();
    if(!desktop||!desktop->socket||desktop->closing||!desktop->user.isEmpty()||clock_.elapsed()>ticket->expires){reply(false,"二维码已失效，请在电脑上刷新后重新扫描",{});return;}
    if(!workspace_->canLogin(peer->workspaceUser,im::serviceType(ticket->service))){reply(false,"你的账号尚未开通电脑端所选的服务",{});return;}
    if(m["type"]=="qr_scan"){
        if(ticket->scannedBy&&ticket->scannedBy!=peer->workspaceUser){reply(false,"二维码已被其他账号扫描",{});return;}
        ticket->scannedBy=peer->workspaceUser;send(desktop,envelope("qr_status",{{"state","scanned"}}));
        // The phone shows where the computer connects from, so a code shown by someone else stands out.
        auto address=desktop->socket->peerAddress();if(const auto v4=address.toIPv4Address();v4)address=QHostAddress(v4);
        reply(true,{},{{"stage","scan"},{"service",ticket->service},{"address",address.toString()}});return;
    }
    if(ticket->scannedBy!=peer->workspaceUser){reply(false,"请先扫描电脑上的二维码",{});return;}
    const int service=ticket->service;tickets_.erase(ticket);desktop->qrCode.clear();
    if(!data["approve"].toBool()){
        send(desktop,envelope("qr_status",{{"state","cancelled"}}));desktop->closing=true;desktop->socket->disconnectFromHost();
        reply(true,{},{{"stage","cancel"}});return;
    }
    // The desktop keeps this short-lived token in memory only, to reconnect after a network drop.
    QJsonObject extra;try{extra["token"]=workspace_->issueToken(peer->workspaceUser,im::serviceType(service),"desktop",12LL*3600);}catch(const std::exception&){emit logLine("qr_token_failed");}
    admit(desktop,peer->workspaceUser,service,desktop->qrRequest,extra);
    reply(true,{},{{"stage","confirm"}});
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

void ChatServerHsx::setFileService(FileStorageServiceHsx* service,quint16 port){
    files_=service;filePort_=port;
    connect(service,&FileStorageServiceHsx::fileReady,this,[this](int sender,int recipient,const QString& id){
        for(const auto& p:peers_.values())if(!p->fileToken.isEmpty()&&(p->workspaceUser==sender||p->workspaceUser==recipient))send(p,envelope("file_notice",{{"fileId",id}}, {},p->user));
    });
}
void ChatServerHsx::refreshFileGrants(){
    if(!files_||!workspace_)return;
    for(const auto& p:peers_.values())if(!p->fileToken.isEmpty()){
        const auto permission=workspace_->filePermissions(p->workspaceUser);const auto token=p->fileToken;
        QMetaObject::invokeMethod(files_,[service=files_,token,permission]{service->grant(token,permission);},Qt::QueuedConnection);
    }
}
void ChatServerHsx::tick(){
    const auto now=clock_.elapsed();const auto peers=peers_.values();
    // A desktop showing a sign-in QR code may wait for the phone until its ticket expires.
    for(const auto& p:peers)if(p->socket&&(p->updater?(now-p->lastActive>30000||now-p->created>1800000):((p->user.isEmpty()&&now-p->created>options_.loginTimeoutMs&&now>p->qrUntil)||(!p->user.isEmpty()&&now-p->lastActive>options_.idleTimeoutMs)))){emit logLine("timeout user="+p->user);p->socket->abort();}
    for(auto it=tickets_.begin();it!=tickets_.end();)if(now>it->expires||it->peer.expired())it=tickets_.erase(it);else++it;
    for(auto it=seen_.begin();it!=seen_.end();)if(now-it->time>600000)it=seen_.erase(it);else++it;
    for(auto it=attempts_.begin();it!=attempts_.end();)if(now-it->first>60000)it=attempts_.erase(it);else++it;
}
void ChatServerHsx::update(const std::shared_ptr<Peer>& p,const QJsonObject& m){
    const auto id=m["id"].toString();const auto data=m["data"].toObject();
    const auto* ssl=qobject_cast<QSslSocket*>(p->socket.data());
    if(!ssl||!ssl->isEncrypted()||!p->user.isEmpty()||p->authenticating||
       !m["senderId"].toString().isEmpty()||!m["recipientId"].toString().isEmpty()){
        error(p,"update_requires_separate_tls",id,true);return;
    }
    if(m["type"]=="update_check"){
        if(p->updater||data["edition"]!="client"||data["platform"]!="windows-x64"||!validReleaseVersion(data["version"].toString())){error(p,"invalid_update_check",id,true);return;}
        p->updater=true;p->lastActive=clock_.elapsed();p->updateInfo=openPublishedUpdate(options_.updatesDirectory,p->updateFile);
        const bool available=!p->updateInfo.isEmpty()&&newerRelease(p->updateInfo["version"].toString(),data["version"].toString());
        send(p,envelope("update_info",{{"requestId",id},{"available",available},{"published",!p->updateInfo.isEmpty()},{"release",available?p->updateInfo:QJsonObject{}}}));
        if(!available)p->updateFile.close();return;
    }
    const double offset=data["offset"].toDouble(-1);
    if(!p->updater||!p->updateFile.isOpen()||offset!=p->updateOffset||data["sha256"]!=p->updateInfo["sha256"]||p->updateOffset>=p->updateFile.size()){
        error(p,"invalid_update_chunk",id,true);return;
    }
    const auto bytes=p->updateFile.read(UpdateChunkSize);if(bytes.isEmpty()){error(p,"update_read_failed",id,true);return;}
    p->lastActive=clock_.elapsed();p->updateOffset+=bytes.size();
    send(p,envelope("update_chunk",{{"requestId",id},{"offset",offset},{"payload",QString::fromLatin1(bytes.toBase64())}}));
    if(p->updateOffset==p->updateFile.size())p->updateFile.close();
}
}
