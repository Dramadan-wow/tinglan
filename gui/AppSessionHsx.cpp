#include "AppSessionHsx.h"
#include "StorageHsx.h"
#include "WorkspaceCodecHsx.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QSettings>
namespace im {
AppSessionHsx::AppSessionHsx(std::filesystem::path file, QObject* parent)
    : QObject(parent), file_(std::move(file)), accounts_(data_), admins_(data_), friends_(data_), groups_(data_), messages_(data_), studio_(*this,QString::fromStdWString(file_.wstring())) {
    if (std::filesystem::exists(file_)) StorageHsx::load(data_, file_);

#ifndef IM_USER_CLIENT
    admins_.initialize();
#endif

    if (data_.dirty()) StorageHsx::save(data_, file_);
}
QString AppSessionHsx::q(const std::string& value) { return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())); }
std::string AppSessionHsx::s(const QString& value) { return value.toUtf8().toStdString(); }
std::filesystem::path AppSessionHsx::path(const QString& value) {
#ifdef _WIN32
    return std::filesystem::path(value.toStdWString());
#else
    return std::filesystem::u8path(s(value));
#endif
}
bool AppSessionHsx::login(int user, ServiceHsx type, const QString& password) {
    try {
        accounts_.login(user, type, s(password));
        lastNickname_=q(data_.user(user).name());
        if (user_ && user_ != user) accounts_.logout(user_);
        admins_.logout();
        user_ = user; service_ = type; save(); emit changed(); return true;
    } catch (const std::exception& error) { save(); emit failed(q(error.what())); return false; }
}
bool AppSessionHsx::loginAdmin(const QString& name, const QString& password) {
#ifdef IM_USER_CLIENT
    Q_UNUSED(name);Q_UNUSED(password);emit failed("普通客户端不支持管理登录");return false;
#else
    if(remoteConfigured_){loginRemoteAdmin(name,password);return false;}
    try {
        admins_.login(s(name), s(password));
        if (user_) accounts_.logout(user_);
        user_ = 0; service_ = ServiceHsx::Count; save(); emit changed(); return true;
    } catch (const std::exception& error) { save(); emit failed(q(error.what())); return false; }

#endif
}
bool AppSessionHsx::loginNickname(const QString& nickname,ServiceHsx type,const QString& password) {
    try{return login(accounts_.findByNickname(s(nickname.trimmed())),type,password);}
    catch(const std::exception& error){emit failed(q(error.what()));return false;}
}
void AppSessionHsx::logout() {
    if(remoteConfigured_){
        if(user_)lastNickname_=q(data_.user(user_).name());remoteClient_.stop();remoteAdmin_=false;user_=0;service_=ServiceHsx::Count;remoteLoginPending_=false;pendingCommands_.clear();readPending_.clear();social_={};feed_={};
        // A fresh remote-only client has no local archive; its server snapshot stays in memory.
        if(std::filesystem::exists(file_))StorageHsx::load(data_,file_,false);else data_=DataStoreHsx{};
#ifndef IM_USER_CLIENT
        admins_.initialize();
#endif
        emit changed();return;
    }
    if (user_) {lastNickname_=q(data_.user(user_).name());accounts_.logout(user_);}
    save();admins_.logout(); user_ = 0; service_ = ServiceHsx::Count; save(); emit changed();
}
bool AppSessionHsx::switchService(ServiceHsx type) {
    try { require(activeService(type), "此服务已下线"); data_.requireOnline(user_); data_.account(user_, type); service_ = type; emit changed(); return true; }
    catch (const std::exception& error) { emit failed(q(error.what())); return false; }
}
bool AppSessionHsx::mutate(const std::function<void()>& action, const QString& success) {
    if(remote()){emit failed("此本机数据操作不适用于服务器账号");return false;}
    try {
        action();
        const bool ended = user_ && !data_.user(user_).online();
        if (ended) { user_ = 0; service_ = ServiceHsx::Count; }
        if (user_ && !data_.user(user_).services().count(service_)) service_ = data_.user(user_).services().begin()->first;
        const bool saved = save(); emit changed();
        if (saved && !success.isEmpty()) emit succeeded(success);
        if (ended) emit sessionEnded();
        return true; // 保存失败时业务修改仍在内存中，不能重复提交同一操作。
    } catch (const std::exception& error) { emit failed(q(error.what())); return false; }
}
bool AppSessionHsx::personalize(const std::function<void()>& action,const QString& success){try{require(signedIn(),"请先登录");const auto before=pendingCommands_.size();action();if(pendingCommands_.size()==before){emit changed();if(!success.isEmpty())emit succeeded(success);}return true;}catch(const std::exception& e){emit failed(q(e.what()));return false;}}
bool AppSessionHsx::save() {
    if(remote())return true;
    try { if (data_.dirty()) StorageHsx::save(data_, file_); return true; }
    catch (const std::exception& error) { emit failed("保存失败：" + q(error.what()) + "。修改仍在内存中。"); return false; }
}
void AppSessionHsx::markDisplayed(const std::vector<long long>& messages) {
    if(remote()){if(!connected())return;QJsonArray ids;for(auto id:messages)if(!data_.message(id).read(user_)&&!readPending_.contains(id)){ids.append(QString::number(id));readPending_.insert(id);}
        if(!ids.isEmpty())command("chat.read",{{"messages",ids}});return;}
    try { for (auto id : messages) messages_.markRead(user_, service_, id); save(); emit countersChanged(); }
    catch (const std::exception& error) { emit failed(q(error.what())); }
}
int AppSessionHsx::unreadCount() const {
    return signedIn() ? static_cast<int>(messages_.inbox(user_, service_, true).size()) : 0;
}
int AppSessionHsx::requestCount() const {
    int count = 0; if (!signedIn()) return count;
    for (const auto& entry : data_.requests()) {
        const auto& r = entry.second;
        if (r.to() == user_ && r.destination() == service_ && r.status() == RequestStatusHsx::Pending) ++count;
    }
    for (const auto& entry : data_.groups()) {
        const auto& g = *entry.second;
        if (g.contains(user_, service_) && g.manages(user_) && g.policy().applications()) count += static_cast<int>(g.pending().size());
    }
    return count;
}

void AppSessionHsx::configureRemote(const QString& filename){
    QFile file(filename);require(file.open(QIODevice::ReadOnly),"无法读取后台连接配置 client.json");
    const auto doc=QJsonDocument::fromJson(file.readAll());require(doc.isObject(),"后台连接配置格式无效");const auto config=doc.object();
#ifdef IM_USER_CLIENT
    require(config["tls"].toBool()&&!config["autoStartLocalServer"].toBool()&&!config["allowInsecureLoopback"].toBool(),"普通客户端必须严格验证 TLS 证书");
#endif
    require(config["host"].isString()&&!config["host"].toString().trimmed().isEmpty()&&config["port"].toInt()>0&&config["port"].toInt()<=65535,"后台连接配置不完整");
    remoteOptions_.host=config["host"].toString();remoteOptions_.port=quint16(config["port"].toInt());remoteOptions_.tls=config.value("tls").toBool(true);remoteOptions_.allowInsecureLoopback=config["allowInsecureLoopback"].toBool();
    if(!config["caFile"].toString().isEmpty())remoteOptions_.caFile=QDir(QFileInfo(filename).absolutePath()).absoluteFilePath(config["caFile"].toString());
    remoteOptions_.workspace=true;remoteConfigured_=true;lastNickname_=QSettings().value("lastRemoteNickname").toString();
    files_.configure(remoteOptions_,QFileInfo(QString::fromStdWString(file_.wstring())).absolutePath()+"/file-transfers");
    connect(&remoteClient_,&net::ChatClientHsx::fileSession,&files_,&net::FileTransferManagerHsx::setSession);
    connect(&remoteClient_,&net::ChatClientHsx::fileNotice,&files_,&net::FileTransferManagerHsx::refresh);
    connect(&remoteClient_,&net::ChatClientHsx::authenticatedChanged,this,[this](bool online){if(!online)files_.disconnected();});
    connect(&files_,&net::FileTransferManagerHsx::problem,this,&AppSessionHsx::failed);
    connect(&remoteClient_,&net::ChatClientHsx::stateChanged,this,[this](const QString& text){connectionStatus_=text;emit connectionChanged(text);});
    connect(&remoteClient_,&net::ChatClientHsx::problem,this,[this](const QString& text){if(remoteLoginPending_){remoteLoginPending_=false;emit failed(text);emit remoteLoginFinished(false);}else{connectionStatus_=text;emit connectionChanged(text);}});
    connect(&remoteClient_,&net::ChatClientHsx::sessionRevoked,this,[this]{remoteAdmin_=false;user_=0;endReason_=connectionStatus_;emit sessionEnded();});
    connect(&remoteClient_,&net::ChatClientHsx::qrTicket,this,&AppSessionHsx::qrTicket);
    connect(&remoteClient_,&net::ChatClientHsx::qrStatus,this,&AppSessionHsx::qrStatus);
    connect(&remoteClient_,&net::ChatClientHsx::workspaceSnapshot,this,[this](const QJsonObject& snapshot){
        try{if(remoteOptions_.administrator){
                require(snapshot["admin"].toBool()&&remoteClient_.user()=="workspace_admin","管理同步身份不匹配");
                data_=WorkspaceCodecHsx::decode(snapshot);remoteAdmin_=true;user_=0;social_=snapshot;emit changed();
                if(remoteLoginPending_){remoteLoginPending_=false;emit remoteLoginFinished(true);}return;
            }
            const int viewer=snapshot["viewer"].toInt();require(!snapshot["admin"].toBool()&&remoteClient_.user()=="u"+QString::number(viewer),"同步身份不匹配");
            auto next=WorkspaceCodecHsx::decode(snapshot);next.user(viewer);data_=std::move(next);user_=viewer;
            if(!data_.user(user_).services().count(service_))service_=data_.user(user_).services().begin()->first;
            if(!data_.user(user_).services().count(serviceType(remoteClient_.loginService()))){remoteClient_.stop();emit failed("登录服务已关闭，请重新选择服务登录");emit sessionEnded();return;}
            lastNickname_=q(data_.user(user_).name());remoteClient_.updateNickname(lastNickname_);QSettings().setValue("lastRemoteNickname",lastNickname_);
            social_=snapshot;feed_=snapshot["feed"].toArray();readPending_.clear();emit changed();
            if(remoteLoginPending_){remoteLoginPending_=false;emit remoteLoginFinished(true);}
            if(!legacyAppearanceAttempted_){legacyAppearanceAttempted_=true;studio_.syncLegacyAppearance();}
        }catch(const std::exception& e){remoteClient_.stop();emit failed(q(e.what()));if(remoteLoginPending_){remoteLoginPending_=false;emit remoteLoginFinished(false);}}
    });
    connect(&remoteClient_,&net::ChatClientHsx::workspaceResult,this,[this](const QString& id,const QJsonObject& result){
        const auto request=pendingCommands_.take(id);const auto success=pendingSuccess_.take(id);
        if(result["ok"].toBool()){if((request["op"]=="admin.password"||(request["op"]=="account.edit"&&request["service"].toInt()==remoteClient_.loginService()))&&!request["password"].toString().isEmpty())remoteClient_.updatePassword(request["password"].toString());if(!success.isEmpty())emit succeeded(success);}
        else{readPending_.clear();emit operationRejected(request);emit failed(result["detail"].toString("服务器未完成操作"));}
    });
}
void AppSessionHsx::loginRemote(const QString& nickname,ServiceHsx service,const QString& password,bool registration){
    legacyAppearanceAttempted_=false;
    admins_.logout();remoteAdmin_=false;remoteOptions_.administrator=false;remoteLoginPending_=true;service_=service;remoteOptions_.username=nickname.trimmed();remoteOptions_.password=password;remoteOptions_.service=int(service);remoteOptions_.registerAccount=registration;remoteClient_.start(remoteOptions_);remoteOptions_.password.clear();
}
void AppSessionHsx::loginRemoteQr(ServiceHsx service){
    legacyAppearanceAttempted_=false;
    admins_.logout();remoteAdmin_=false;remoteOptions_.administrator=false;remoteLoginPending_=true;service_=service;
    remoteOptions_.username.clear();remoteOptions_.password.clear();remoteOptions_.service=int(service);remoteOptions_.registerAccount=false;remoteOptions_.qrLogin=true;
    remoteClient_.start(remoteOptions_);remoteOptions_.qrLogin=false;
}
void AppSessionHsx::loginRemoteAdmin(const QString& name,const QString& password){
#ifdef IM_USER_CLIENT
    Q_UNUSED(name);Q_UNUSED(password);emit failed("普通客户端不支持管理登录");emit remoteLoginFinished(false);return;
#else
    admins_.logout();remoteAdmin_=false;user_=0;service_=ServiceHsx::Count;remoteOptions_.administrator=true;remoteLoginPending_=true;
    remoteOptions_.username=name.trimmed();remoteOptions_.password=password;remoteOptions_.service=0;remoteOptions_.registerAccount=false;
    remoteClient_.start(remoteOptions_);remoteOptions_.password.clear();
#endif
}
bool AppSessionHsx::command(const QString& op,QJsonObject arguments,const std::function<void()>& local,const QString& success){
#ifdef IM_USER_CLIENT
    if(op.startsWith("admin.")){emit failed("普通客户端不支持管理操作");return false;}
#endif
    if(!remote())return mutate(local?local:[]{},success);
    if(!connected()){emit failed("尚未连接服务器，此操作未发送");return false;}
    arguments["op"]=op;if(!arguments.contains("service"))arguments["service"]=int(service_);const auto id=remoteClient_.workspaceCommand(arguments);if(id.isEmpty())return false;
    pendingCommands_[id]=arguments;pendingSuccess_[id]=success.isEmpty()?(op=="chat.send"?QString("消息已由服务器保存并同步"):QString{}):success;
    if(op=="chat.send")emit succeeded("正在发送…");return true;
}
}

