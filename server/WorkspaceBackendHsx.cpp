#include "WorkspaceBackendHsx.h"
#include "AccountManagerHsx.h"
#include "AdminManagerHsx.h"
#include "FriendManagerHsx.h"
#include "GroupManagerHsx.h"
#include "MessageManagerHsx.h"
#include "StorageHsx.h"
#include "PasswordHsx.h"
#include "SocialStoreHsx.h"
#include <QCryptographicHash>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>
#include <limits>
namespace im {
namespace {
std::string s(const QJsonValue& value){return value.toString().toStdString();}
void sql(QSqlQuery& query){if(!query.exec())throw std::runtime_error("数据库操作失败");}
std::filesystem::path path(const QString& value){
#ifdef _WIN32
return std::filesystem::path(value.toStdWString());
#else
return std::filesystem::u8path(value.toStdString());
#endif
}
}
WorkspaceBackendHsx::WorkspaceBackendHsx(const QString& file,const QString& importFile){
    QDir().mkpath(QFileInfo(file).absolutePath());connection_=QUuid::createUuid().toString();db_=QSqlDatabase::addDatabase("QSQLITE",connection_);db_.setDatabaseName(file);require(db_.open(),"无法打开平台数据库");
    QSqlQuery q(db_);for(const auto* text:{"PRAGMA journal_mode=WAL","PRAGMA synchronous=FULL","CREATE TABLE IF NOT EXISTS state(id INTEGER PRIMARY KEY CHECK(id=1),payload BLOB NOT NULL)","CREATE TABLE IF NOT EXISTS operations(actor INTEGER NOT NULL,id TEXT NOT NULL,digest BLOB NOT NULL,result BLOB NOT NULL,PRIMARY KEY(actor,id))","CREATE TABLE IF NOT EXISTS posts(id INTEGER PRIMARY KEY AUTOINCREMENT,author INTEGER NOT NULL,text TEXT NOT NULL,created INTEGER NOT NULL)","CREATE TABLE IF NOT EXISTS likes(post INTEGER NOT NULL,actor INTEGER NOT NULL,PRIMARY KEY(post,actor))","CREATE TABLE IF NOT EXISTS comments(id INTEGER PRIMARY KEY AUTOINCREMENT,post INTEGER NOT NULL,author INTEGER NOT NULL,text TEXT NOT NULL,created INTEGER NOT NULL)"}){q.prepare(text);sql(q);}
    SocialStoreHsx(db_,data_).migrate();
    q.prepare("SELECT payload FROM state WHERE id=1");sql(q);
    if(q.next()){const auto doc=QJsonDocument::fromJson(q.value(0).toByteArray());require(doc.isObject(),"平台数据损坏");data_=WorkspaceCodecHsx::decode(doc.object());}
    else if(!importFile.isEmpty()&&QFileInfo::exists(importFile)){StorageHsx::load(data_,path(importFile),false);persist();}
    for(auto& [id,u]:data_.users_)u->online_=false;
}
WorkspaceBackendHsx::~WorkspaceBackendHsx(){db_.close();db_=QSqlDatabase();QSqlDatabase::removeDatabase(connection_);}
void WorkspaceBackendHsx::persist(){const auto bytes=QJsonDocument(WorkspaceCodecHsx::encode(data_)).toJson(QJsonDocument::Compact);require(bytes.size()<=8*1024*1024,"课程版数据达到容量上限，请管理员归档");QSqlQuery q(db_);q.prepare("INSERT OR REPLACE INTO state(id,payload) VALUES(1,?)");q.addBindValue(bytes);sql(q);}
int WorkspaceBackendHsx::resolve(const QString& nickname,ServiceHsx service)const{
    for(const auto& [id,u]:data_.users())if(QString::fromStdString(u->name()).trimmed()==nickname.trimmed()&&u->services().count(service)&&u->loginAllowed())return id;return 0;
}
std::string WorkspaceBackendHsx::credential(int user,ServiceHsx service)const{return data_.account(user,service).password_;}
void WorkspaceBackendHsx::online(int user,bool enabled,ServiceHsx service){
    data_.editUser(user).online_=enabled;
    loginAudit(QString::fromStdString(data_.user(user).name()),user,service,enabled?"用户登录成功":"用户登出 / 连接断开");
}
std::string WorkspaceBackendHsx::adminCredential(const QString& name)const{return name.toStdString()==data_.adminName_?data_.adminHash_:std::string{};}
void WorkspaceBackendHsx::upgradeCredential(int user,ServiceHsx service,const std::string& previous,const std::string& upgraded){
    require(PasswordHsx::valid(upgraded),"密码摘要无效");auto& current=user?data_.editAccount(user,service).password_:data_.adminHash_;
    if(current!=previous)return;const auto before=WorkspaceCodecHsx::encode(data_);
    try{current=upgraded;persist();}catch(...){data_=WorkspaceCodecHsx::decode(before);throw;}
}
void WorkspaceBackendHsx::setAdmin(const QString& name,const QString& password){
    require(!name.trimmed().isEmpty()&&name.toUtf8().size()<=60,"管理员名称无效");
    const auto hash=PasswordHsx::hash(password.toStdString(),PasswordHsx::RemoteMinimum);const auto before=WorkspaceCodecHsx::encode(data_);
    try{data_.adminName_=name.trimmed().toStdString();data_.adminHash_=hash;data_.audit(data_.adminName_,"服务器维护者设置管理员凭据");persist();}
    catch(...){data_=WorkspaceCodecHsx::decode(before);throw;}
}
int WorkspaceBackendHsx::importAudit(const QString& file){
    DataStoreHsx source;StorageHsx::load(source,path(file),false);const auto before=WorkspaceCodecHsx::encode(data_);int count=0;
    try{for(const auto& [id,log]:source.adminLogs()){
        if(log.target()&&(!data_.users().count(log.target())||!source.users().count(log.target())||source.user(log.target()).name()!=data_.user(log.target()).name()))continue;
        bool duplicate=false;for(const auto& [existingId,other]:data_.adminLogs_)if(other.created()==log.created()&&other.actor()==log.actor()&&other.action()==log.action()&&other.target()==log.target()&&other.service()==log.service()){duplicate=true;break;}
        if(duplicate)continue;
        const auto next=data_.nextAudit_++;data_.adminLogs_.emplace(next,AdminLogHsx(next,log.created(),log.actor(),log.action(),log.target(),log.service()));++count;
    }while(data_.adminLogs_.size()>2000)data_.adminLogs_.erase(data_.adminLogs_.begin());persist();return count;}
    catch(...){data_=WorkspaceCodecHsx::decode(before);throw;}
}
void WorkspaceBackendHsx::recordAudit(const std::string& actor,const std::string& action,int user,ServiceHsx service){
    // SQLite can record events before an operator provisions an administrator. The legacy text format cannot.
    require(data_.nextAudit_<std::numeric_limits<long long>::max(),"日志编号已用尽");const auto id=data_.nextAudit_++;
    data_.adminLogs_.emplace(id,AdminLogHsx(id,QDateTime::currentSecsSinceEpoch(),actor,action,user,service));
    while(data_.adminLogs_.size()>2000)data_.adminLogs_.erase(data_.adminLogs_.begin());
}
void WorkspaceBackendHsx::loginAudit(const QString& nickname,int user,ServiceHsx service,const QString& action,bool admin){
    if(!user&&!admin)for(const auto& [id,u]:data_.users())if(QString::fromStdString(u->name())==nickname){user=id;break;}
    const auto before=WorkspaceCodecHsx::encode(data_);
    try{recordAudit(admin?data_.adminName_:"用户:"+nickname.toStdString(),action.toStdString(),user,service);persist();}
    catch(...){data_=WorkspaceCodecHsx::decode(before);throw;}
}
QJsonObject WorkspaceBackendHsx::adminSnapshot()const{
    // Negative viewer masks every credential and private conversation. Never send encode(data_,0).
    auto out=WorkspaceCodecHsx::encode(data_,-1);out["admin"]=true;out["viewer"]=0;
    out["adminName"]=QString::fromStdString(data_.adminName_);out["adminLogs"]=WorkspaceCodecHsx::encode(data_)["adminLogs"];
    out["groups"]=QJsonArray{};out["messages"]=QJsonArray{};out["requests"]=QJsonArray{};
    out["membershipOrders"]=orders();return out;
}
QJsonObject WorkspaceBackendHsx::snapshot(int viewer)const{auto out=WorkspaceCodecHsx::encode(data_,viewer);out["viewer"]=viewer;const auto social=SocialStoreHsx(db_,data_).snapshot(viewer);for(auto it=social.begin();it!=social.end();++it)out[it.key()]=it.value();return out;}
QJsonArray WorkspaceBackendHsx::orders()const{return SocialStoreHsx(db_,data_).orders();}
QJsonObject WorkspaceBackendHsx::filePermissions(int user){
    QJsonArray services;QJsonObject routes;
    if(!data_.users().count(user)||!data_.user(user).loginAllowed())return {{"user",user},{"services",services},{"routes",routes}};
    for(const auto& [type,account]:data_.user(user).services()){
        if(!activeService(type))continue;services.append(int(type));
        for(const auto& [id,friendship]:data_.account(user,type).friends()){
            try{if(data_.user(id).loginAllowed()&&FriendManagerHsx(data_).allowed(user,type,id,friendship.contact()))routes[QString::number(int(type))+":"+QString::number(id)]=int(friendship.contact());}catch(...){}
        }
    }
    return {{"user",user},{"services",services},{"routes",routes}};
}
void WorkspaceBackendHsx::reviewOrder(const QString& id,bool approve,const QString& note){command(0,ServiceHsx::Count,QUuid::createUuid().toString(),{{"op","admin.membership.review"},{"id",id},{"approve",approve},{"verified",approve},{"note",note}},true);}
int WorkspaceBackendHsx::registerUser(const QString& nickname,const std::string& hash,ServiceHsx service){
    require(PasswordHsx::valid(hash),"密码摘要无效");require(data_.users().size()<1000,"当前用户容量已满");
    auto before=WorkspaceCodecHsx::encode(data_);require(db_.transaction(),"无法开始注册事务");
    try{AccountManagerHsx accounts(data_);const int id=accounts.availableUserId();accounts.registerUser(id,nickname.trimmed().toStdString(),"2000-01-01","未设置",service,id,hash,false);recordAudit("用户:"+nickname.toStdString(),"注册账号",id,service);persist();require(db_.commit(),"注册保存失败");return id;}
    catch(...){db_.rollback();data_=WorkspaceCodecHsx::decode(before);throw;}
}
QJsonObject WorkspaceBackendHsx::command(int actor,ServiceHsx service,const QString& request,const QJsonObject& data,bool administrator){
    if(administrator)require(actor==0,"管理身份无效");else{require(activeService(service),"此服务已下线");data_.requireOnline(actor);data_.account(actor,service);}const auto op=data["op"].toString();
    const auto material=QJsonDocument(QJsonObject{{"service",int(service)},{"data",data}}).toJson(QJsonDocument::Compact);
    const bool secret=data.contains("password")||data.contains("old");
    QSqlQuery lookup(db_);lookup.prepare("SELECT digest,result FROM operations WHERE actor=? AND id=?");lookup.addBindValue(actor);lookup.addBindValue(request);sql(lookup);
    const bool repeated=lookup.next();QByteArray salt;
    if(secret){if(repeated)salt=lookup.value(0).toByteArray().left(16);else{salt.resize(16);for(auto& byte:salt)byte=char(QRandomGenerator::system()->generate());}}
    const auto digest=secret?salt+QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256,material,salt,100000,32):QCryptographicHash::hash(material,QCryptographicHash::Sha256);
    if(repeated){require(lookup.value(0).toByteArray()==digest,"请求编号与先前操作冲突");return QJsonDocument::fromJson(lookup.value(1).toByteArray()).object();}
    const auto before=WorkspaceCodecHsx::encode(data_);require(db_.transaction(),"无法开始事务");
    try{
        AccountManagerHsx a(data_);FriendManagerHsx f(data_);GroupManagerHsx g(data_);MessageManagerHsx m(data_);
        const int target=data["target"].toInt(),group=data["group"].toInt();const auto other=administrator?ServiceHsx::QQ:serviceType(data.value("contact").toInt(int(service)));const bool enabled=data["enabled"].toBool();if(!administrator)require(activeService(other),"此服务已下线");
        const auto id=data["message"].toString().toLongLong();QJsonObject result{{"ok",true}};
        if(administrator){
            AdminManagerHsx admin(data_);data_.adminOnline_=true;
            if(op=="sync"){}
            else if(op=="admin.allow"){admin.setLoginAllowed(target,enabled);if(!enabled)result["revokeUser"]=target;}
            else if(op=="admin.logout"){admin.forceLogout(target);result["revokeUser"]=target;}
            else if(op=="admin.reset"){require(data["password"].toString().toUtf8().size()>=qsizetype(PasswordHsx::RemoteMinimum),"新密码至少 12 字节");const int scope=data.value("scope").toInt(-1);require((scope>=0&&scope<ActiveServiceCountHsx)||scope==int(ServiceHsx::Count),"重置范围无效");admin.resetPassword(target,ServiceHsx(scope),s(data["password"]));result["revokeUser"]=target;}
            else if(op=="admin.password"){require(data["password"].toString().toUtf8().size()>=qsizetype(PasswordHsx::RemoteMinimum),"管理员新密码至少 12 字节");admin.changePassword(s(data["old"]),s(data["password"]));result["revokeAdmins"]=true;}
            else if(op=="admin.membership.review"){
                require(!data["approve"].toBool()||data["verified"].toBool(),"请先核验实际到账");
                SocialStoreHsx(db_,data_).review(data["id"].toString(),data["approve"].toBool(),data["note"].toString(),false);
                QSqlQuery reviewed(db_);reviewed.prepare("UPDATE membership_orders SET reviewer=?,reviewed=? WHERE id=?");reviewed.addBindValue(QString::fromStdString(data_.adminName_));reviewed.addBindValue(QDateTime::currentSecsSinceEpoch());reviewed.addBindValue(data["id"].toString());sql(reviewed);
                data_.audit(data_.adminName_,(data["approve"].toBool()?"确认会员到账：":"驳回会员申请：")+s(data["id"]));
            }else throw std::runtime_error("不支持此管理操作");
            data_.adminOnline_=false;
        }
        else if(SocialStoreHsx(db_,data_).command(actor,data,result)){}
        else if(op=="sync"){}
        else if(op=="chat.send"){require(data["kind"].isDouble()&&(data["kind"].toDouble()==0||data["kind"].toDouble()==1)&&data["target"].isDouble()&&target>0&&data["text"].isString(),"消息参数无效");const auto mid=data["kind"].toInt()==0?m.sendPrivate(actor,service,target,s(data["text"])):m.sendGroup(actor,service,target,s(data["text"]),data["mention"].toInt());result["message"]=QString::number(mid);}
        else if(op=="chat.read"){for(auto v:data["messages"].toArray())m.markRead(actor,service,v.toString().toLongLong());}
        else if(op=="chat.star")m.star(actor,service,id,enabled);
        else if(op=="chat.recall")m.recall(actor,id);
        else if(op=="profile")a.profile(actor,s(data["name"]),s(data["location"]));
        else if(op=="account.edit"){require(data["password"].toString().isEmpty()||data["password"].toString().toUtf8().size()>=qsizetype(PasswordHsx::RemoteMinimum),"新密码至少 12 字节");a.edit(actor,service,s(data["nick"]),s(data["old"]),s(data["password"]));}
        else if(op=="account.open"){require(data["password"].toString().toUtf8().size()>=qsizetype(PasswordHsx::RemoteMinimum),"服务密码至少 12 字节");a.open(actor,other,data["number"].toString().toLongLong(),data_.user(actor).name(),s(data["password"]),data["shared"].toBool());}
        else if(op=="account.close")a.close(actor,service);
        else if(op=="account.bind")a.bind(actor,data["number"].toString().toLongLong());
        else if(op=="friend.request")f.request(actor,service,target,other,s(data["reason"]));
        else if(op=="friend.respond")f.respond(actor,service,id,enabled);
        else if(op=="friend.cancel")f.cancel(actor,id);
        else if(op=="friend.remove")f.remove(actor,service,target);
        else if(op=="friend.edit")f.edit(actor,service,target,s(data["remark"]),s(data["category"]));
        else if(op=="friend.block")f.block(actor,service,target,enabled);
        else if(op=="group.create")g.create(actor,service,s(data["name"]),GroupModeHsx(data["mode"].toInt()),data["parent"].toInt());
        else if(op=="group.invite")g.invite(actor,service,group,target,other);
        else if(op=="group.apply")g.apply(actor,service,group);
        else if(op=="group.cancel")g.cancelApplication(actor,service,group);
        else if(op=="group.approve")g.approve(actor,service,group,target,other,enabled);
        else if(op=="group.leave")g.leave(actor,service,group);
        else if(op=="group.disband")g.disband(actor,service,group);
        else if(op=="group.edit")g.edit(actor,service,group,s(data["name"]),s(data["notice"]),data["capacity"].toInt());
        else if(op=="group.admin")g.admin(actor,service,group,target,enabled);
        else if(op=="group.mute")g.mute(actor,service,group,target,data["minutes"].toInt());
        else if(op=="group.kick")g.kick(actor,service,group,target);
        else if(op=="group.transfer")g.transfer(actor,service,group,target);
        else if(op=="group.mode")g.mode(actor,service,group,GroupModeHsx(data["mode"].toInt()));
        else throw std::runtime_error("服务器不支持此操作");
        persist();QSqlQuery save(db_);save.prepare("INSERT INTO operations(actor,id,digest,result) VALUES(?,?,?,?)");save.addBindValue(actor);save.addBindValue(request);save.addBindValue(digest);save.addBindValue(QJsonDocument(result).toJson(QJsonDocument::Compact));sql(save);require(db_.commit(),"服务器保存失败");return result;
    }catch(...){db_.rollback();data_=WorkspaceCodecHsx::decode(before);throw;}
}
}

