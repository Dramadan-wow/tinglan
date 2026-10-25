#include "ChatClientHsx.h"
#include <QFile>
#include <QJsonDocument>
#include <QHostAddress>
#include <QSslSocket>
#include <QSslConfiguration>
#include <QSslCertificate>
#include <QSslError>
#include <QTcpSocket>
#include <algorithm>

namespace im::net {
namespace {
QString explain(const QString& code){
    static const QHash<QString,QString> messages{{"invalid_credentials","账号或密码不正确"},{"already_online","该远程账号已在其他连接登录"},
        {"recipient_offline","对方未在线"},{"recipient_unavailable","对方连接暂不可用"},{"invalid_chat","收件人或消息内容无效"},
        {"id_conflict","消息编号与已有内容冲突"},{"dedup_capacity","服务器消息窗口已满，请稍后发送"},
        {"server_busy","服务器繁忙，请稍后重试"},{"login_rate_limit","登录过于频繁，请一分钟后重试"},
        {"not_authenticated","远程登录已失效"},{"sender_mismatch","发送身份与当前登录不一致"},{"rate_limit","消息过于频繁"}};
    return messages.value(code,"服务端拒绝请求（"+code+"）");
}
}
ChatClientHsx::ChatClientHsx(QObject* parent):QObject(parent) {
    clock_.start();retry_.setSingleShot(true);timer_.setInterval(1000);
    connect(&retry_,&QTimer::timeout,this,&ChatClientHsx::open);
    connect(&timer_,&QTimer::timeout,this,&ChatClientHsx::tick);timer_.start();
}
ChatClientHsx::~ChatClientHsx(){stop();}
void ChatClientHsx::start(const ClientOptionsHsx& options) {
    stop();options_=options;
    if(options.host.trimmed().isEmpty()||!options.port||(options.workspace?options.username.trimmed().isEmpty():!validUser(options.username))||
       options.password.toUtf8().size()<(options.workspace?1:12)||options.password.toUtf8().size()>128) {
        emit problem(options.workspace?"请输入登录昵称及密码（最多 128 字节）":"请填写服务器、端口、远程账号及 12–128 字节密码");return;
    }
    if(!options.tls && (!options.allowInsecureLoopback || !QHostAddress(options.host).isLoopback())) {
        emit problem("明文测试只允许显式连接 127.0.0.1 或 ::1；其他地址必须使用 TLS");return;
    }
    active_=true;everAuthenticated_=false;retryMs_=1000;received_.clear();open();
}
void ChatClientHsx::stop(){active_=false;retry_.stop();lost("未连接",false);options_.password.clear();user_.clear();}
void ChatClientHsx::open() {
    if(!active_)return;
    decoder_.clear();started_=lastReceived_=clock_.elapsed();
    if(options_.tls){
        if(!QSslSocket::supportsSsl()){lost("此 Qt 环境缺少可用 TLS 后端",false);return;}
        auto* ssl=new QSslSocket(this);socket_=ssl;
        auto config=QSslConfiguration::defaultConfiguration();config.setProtocol(QSsl::TlsV1_2OrLater);
        if(!options_.caFile.isEmpty()){
            QFile f(options_.caFile);if(!f.open(QIODevice::ReadOnly)){lost("无法读取 CA 证书",false);return;}
            const auto certs=QSslCertificate::fromData(f.readAll());
            if(certs.isEmpty()){lost("CA 证书无效",false);return;}
            config.setCaCertificates(certs);
        }
        ssl->setSslConfiguration(config);ssl->setPeerVerifyMode(QSslSocket::VerifyPeer);
        connect(ssl,&QSslSocket::encrypted,this,&ChatClientHsx::login);
        connect(ssl,&QSslSocket::sslErrors,this,[this](const QList<QSslError>&){lost("TLS 证书校验失败，请检查域名、有效期和 CA",false);});
    }else{
        socket_=new QTcpSocket(this);connect(socket_,&QTcpSocket::connected,this,&ChatClientHsx::login);
    }
    auto* current=socket_;current->setReadBufferSize(2*MaxFrame+4);
    connect(current,&QTcpSocket::readyRead,this,&ChatClientHsx::read);
    connect(current,&QTcpSocket::disconnected,this,[this,current]{if(socket_==current)lost("连接已断开");});
    connect(current,&QTcpSocket::errorOccurred,this,[this,current](QAbstractSocket::SocketError){if(socket_==current)lost(current->errorString());});
    emit stateChanged("正在连接服务器…");
    if(auto* ssl=qobject_cast<QSslSocket*>(current))ssl->connectToHostEncrypted(options_.host,options_.port);
    else current->connectToHost(options_.host,options_.port);
}
void ChatClientHsx::login(){
    emit stateChanged("正在验证远程账号…");loginId_=newId();
    write(envelope("login",{{"username",options_.username},{"password",options_.password},{"workspace",options_.workspace},{"register",options_.registerAccount},{"service",options_.service},{"admin",options_.administrator},{"fileTransfer",1}}, {},{},loginId_));
}
bool ChatClientHsx::write(const QJsonObject& message){
    if(!socket_||socket_->state()!=QAbstractSocket::ConnectedState)return false;
    try{const auto frame=encode(message);const auto* ssl=qobject_cast<QSslSocket*>(socket_);if(socket_->bytesToWrite()+(ssl?ssl->encryptedBytesToWrite():0)+frame.size()>MaxQueue)return false;
        return socket_->write(frame)==frame.size();}catch(const std::exception&){return false;}
}
void ChatClientHsx::lost(const QString& reason,bool retry){
    if(socket_){auto* old=socket_;socket_=nullptr;old->disconnect(this);old->abort();old->deleteLater();}
    decoder_.clear();snapshotBytes_.clear();snapshotId_.clear();
    const auto commands=commands_.keys();commands_.clear();for(const auto& id:commands)emit workspaceResult(id,{{"ok",false},{"detail","连接中断，操作结果待重新同步；请勿重复提交"}});
    const bool was=authenticated_;authenticated_=false;if(was)emit authenticatedChanged(false);
    const auto ids=pending_.keys();pending_.clear();for(const auto& id:ids)emit messageResult(id,false,"连接中断，送达状态未知；不会自动重发");
    // Until this session has authenticated once, a dropped workspace connection is the login failure
    // itself: report it through problem() and stop. Retrying would hang the login dialog, or sign in
    // silently in the background after the dialog already reported a failure.
    const bool initial=options_.workspace&&!everAuthenticated_;
    if(active_&&retry&&!initial){emit stateChanged((options_.workspace?QString("连接暂不可用"):reason)+QString(" · %1 秒后自动重连").arg(retryMs_/1000));retry_.start(retryMs_);retryMs_=std::min(retryMs_*2,30000);}
    else{const bool notify=active_;active_=false;retry_.stop();options_.password.clear();emit stateChanged(reason);if(notify&&options_.workspace)emit problem(reason);}
}
void ChatClientHsx::read(){
    while(socket_&&socket_->bytesAvailable()){
        QList<QJsonObject> messages;QString error;
        if(!decoder_.feed(socket_->read(MaxFrame),messages,error)){lost("服务器协议错误："+error,false);return;}
        for(const auto& m:messages){if(!socket_)break;lastReceived_=clock_.elapsed();handle(m);}
    }
}
void ChatClientHsx::handle(const QJsonObject& message){
    const auto type=message.value("type").toString();const auto data=message.value("data").toObject();
    if(type=="login_result"){
        if(authenticated_||data.value("requestId").toString()!=loginId_){lost("登录响应不匹配",false);return;}
        if(!data.value("ok").toBool()){emit problem(data.value("detail").toString("登录失败："+explain(data.value("code").toString())));lost("登录失败，请检查远程账号",false);return;}
        user_=data.value("userId").toString();if(options_.workspace?!validUser(user_):user_!=options_.username){lost("登录身份不匹配",false);return;}
        if(options_.workspace&&(data["admin"].toBool()!=options_.administrator)){lost("管理员身份校验不匹配",false);return;}
        options_.registerAccount=false;authenticated_=everAuthenticated_=true;retryMs_=1000;lastHeartbeat_=clock_.elapsed();emit stateChanged(options_.administrator?"已连接 · 管理数据实时同步":options_.workspace?"已连接 · 消息实时同步":"已连接 · "+user_+(options_.tls?" · TLS 加密":" · 本机明文测试"));emit authenticatedChanged(true);
        emit fileSession(data["files"].toObject());
    }else if(type=="file_notice"&&authenticated_&&options_.workspace){
        if(message["recipientId"].toString()!=user_||!validId(data["fileId"].toString())){lost("文件通知身份无效",false);return;}emit fileNotice();
    }else if(type=="workspace_sync"&&authenticated_&&options_.workspace){
        const auto sid=data["snapshot"].toString();const int part=data["part"].toInt(-1),count=data["count"].toInt(-1);const auto encoded=data["payload"].toString().toLatin1();const auto chunk=QByteArray::fromBase64(encoded);
        if(message["recipientId"].toString()!=user_||!validId(sid)||count<1||count>700||part<0||part>=count||chunk.toBase64()!=encoded){lost("同步数据无效",false);return;}
        if(part==0){snapshotBytes_.clear();snapshotId_=sid;snapshotPart_=0;snapshotCount_=count;}
        if(sid!=snapshotId_||part!=snapshotPart_||count!=snapshotCount_||snapshotBytes_.size()+chunk.size()>16*1024*1024){lost("同步分段顺序无效",false);return;}
        snapshotBytes_+=chunk;++snapshotPart_;if(snapshotPart_==count){const auto doc=QJsonDocument::fromJson(snapshotBytes_);snapshotBytes_.clear();if(!doc.isObject()){lost("同步内容无效",false);return;}emit workspaceSnapshot(doc.object());}
    }else if(type=="workspace_result"&&authenticated_&&options_.workspace){
        const auto id=data["requestId"].toString();if(commands_.remove(id))emit workspaceResult(id,data);
    }else if(type=="chat"&&authenticated_){
        const auto id=message.value("id").toString(),sender=message.value("senderId").toString();
        const auto text=data.value("text").toString();
        if(!validUser(sender)||message.value("recipientId").toString()!=user_||!data.value("text").isString()||text.toUtf8().size()>MaxText){lost("聊天响应字段无效",false);return;}
        const auto key=sender+":"+id;if(received_.contains(key))return;
        if(received_.size()>=10000){lost("接收去重窗口已满，请稍后连接",false);return;}
        received_[key]=clock_.elapsed();emit chatReceived(sender,text,id);
    }else if(type=="message_ack"&&authenticated_){
        const auto id=data.value("messageId").toString();if(!pending_.remove(id))return;
        const bool ok=data.value("ok").toBool();
        emit messageResult(id,ok,ok?(data.value("duplicate").toBool()?"重复消息已确认，未再次转发":"服务器已转发至对方连接（非已读回执）"):"发送失败："+explain(data.value("code").toString()));
    }else if(type=="error"){
        if(data["code"]=="session_revoked"){lost("管理员已结束此会话，请重新登录",false);emit sessionRevoked();return;}
        const auto id=data.value("requestId").toString(),detail=explain(data.value("code").toString());
        if(commands_.remove(id))emit workspaceResult(id,{{"ok",false},{"detail",detail}});else if(pending_.remove(id))emit messageResult(id,false,detail);else emit problem(detail);
    }else if(type!="heartbeat"){lost("未知服务器消息类型",false);}
}
QString ChatClientHsx::sendChat(const QString& recipient,const QString& text,const QString& requestedId){
    if(!authenticated_){emit problem("请先连接服务器并登录");return {};}
    if(!validUser(recipient)||text.trimmed().isEmpty()||text.toUtf8().size()>MaxText){emit problem("对方账号无效，或消息为空／超过 16 KiB UTF-8");return {};}
    const auto id=requestedId.isEmpty()?newId():requestedId;
    if(!validId(id)||pending_.contains(id)||pending_.size()>=100){emit problem("消息编号无效或发送队列已满");return {};}
    if(!write(envelope("chat",{{"text",text}},user_,recipient,id))){emit problem("消息未进入发送队列");return {};}
    pending_[id]=clock_.elapsed();return id;
}
QString ChatClientHsx::workspaceCommand(const QJsonObject& data){
    if(!authenticated_||!options_.workspace||commands_.size()>=100){emit problem("尚未连接，操作未发送");return {};}
    const auto id=newId();if(!write(envelope("workspace_command",data,user_,{},id))){emit problem("操作未进入发送队列");return {};}commands_[id]=clock_.elapsed();return id;
}
void ChatClientHsx::tick(){
    const auto now=clock_.elapsed();
    for(auto it=received_.begin();it!=received_.end();)if(now-it.value()>600000)it=received_.erase(it);else++it;
    const auto keys=pending_.keys();for(const auto& id:keys)if(now-pending_[id]>15000){pending_.remove(id);emit messageResult(id,false,"确认超时，送达状态未知；不会自动重发");}
    const auto requests=commands_.keys();for(const auto& id:requests)if(now-commands_[id]>20000){commands_.remove(id);emit workspaceResult(id,{{"ok",false},{"detail","确认超时，操作结果待同步；请勿重复提交"}});}
    if(!socket_)return;
    if((!authenticated_&&now-started_>30000)||(authenticated_&&now-lastReceived_>45000)){lost("连接或心跳超时");return;}
    if(authenticated_&&now-lastHeartbeat_>=15000){lastHeartbeat_=now;if(!write(envelope("heartbeat",{},user_)))lost("心跳发送失败");}
}
}
