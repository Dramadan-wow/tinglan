#include "WorkspaceCodecHsx.h"
#include "PasswordHsx.h"
#include <QJsonArray>
#include <set>
namespace im {
namespace {
QString q(const std::string& s){return QString::fromStdString(s);}
std::string s(const QJsonValue& v){return v.toString().toStdString();}
template<class Set> QJsonArray array(const Set& values){QJsonArray out;for(auto v:values)out.append(int(v));return out;}
// Placeholders keep hidden fields decodable by already-installed clients (DateHsx and UserHsx reject empty values).
const QString HiddenDate="1900-01-01",HiddenLocation="未公开";
}
QJsonObject WorkspaceCodecHsx::encode(const DataStoreHsx& d,int viewer){
    QJsonArray users,groups,messages,requests;
    // Friendship is two-way, but collect both directions so a half-written relation never widens visibility.
    std::set<int> contacts;
    if(viewer>0)for(const auto& [id,ptr]:d.users_)for(const auto& [type,a]:ptr->services_)for(const auto& [fid,f]:a.friends_){if(id==viewer)contacts.insert(fid);else if(fid==viewer)contacts.insert(id);}
    for(const auto& [id,ptr]:d.users_){const auto& u=*ptr;QJsonArray services;const bool self=viewer<=0||viewer==id,known=self||contacts.count(id)!=0;
        for(const auto& [type,a]:u.services_){QJsonArray friends;for(const auto& [fid,f]:a.friends_)if(!viewer||viewer==id)friends.append(QJsonObject{{"id",fid},{"contact",int(f.contact())},{"remark",q(f.remark())},{"category",q(f.category())}});
            services.append(QJsonObject{{"type",int(type)},{"number",QString::number(a.number_)},{"nick",q(a.nick_)},{"password",viewer?QString{}:q(a.password_)},{"shared",a.shared_},{"bound",self?QString::number(a.boundQQ_):QString("0")},{"friends",friends},{"blocked",(!viewer||viewer==id)?array(a.blocked_):QJsonArray{}}});}
        users.append(QJsonObject{{"id",id},{"type",q(u.typeName())},{"name",q(u.name_)},{"birth",self?q(u.birth_.str()):HiddenDate},{"applied",self?q(u.applied_.str()):HiddenDate},{"location",known?q(u.location_):HiddenLocation},{"shared",u.sharesQQ()},{"online",known&&u.online_},{"disabled",self&&u.disabled_},{"services",services}});
    }
    for(const auto& [id,ptr]:d.groups_){const auto& g=*ptr;QJsonArray members,pending,mutes;
        for(const auto& [uid,types]:g.members_)members.append(QJsonObject{{"id",uid},{"services",array(types)}});
        for(const auto& [uid,type]:g.pending_)if(!viewer||uid==viewer||g.manages(viewer))pending.append(QJsonArray{uid,int(type)});
        if(viewer<=0||g.contains(viewer))for(const auto& [uid,time]:g.mutedUntil_)mutes.append(QJsonArray{uid,QString::number(time)});
        groups.append(QJsonObject{{"id",id},{"owner",g.owner_},{"parent",g.parent_},{"capacity",g.capacity_},{"native",int(g.native_)},{"mode",int(g.policy().mode())},{"name",q(g.name_)},{"notice",q(g.announcement_)},{"members",members},{"admins",array(g.admins_)},{"pending",pending},{"mutes",mutes}});
    }
    for(const auto& [id,m]:d.messages_){
        const bool visible=!viewer||(m.kind_==MessageKindHsx::Private?(m.sender_==viewer||m.target_==viewer):(d.groups_.count(m.target_)&&d.group(m.target_).contains(viewer)));
        if(!visible)continue;QJsonArray stars;for(int who:m.stars_)if(!viewer||who==viewer)stars.append(who);
        messages.append(QJsonObject{{"id",QString::number(id)},{"kind",int(m.kind_)},{"sender",m.sender_},{"target",m.target_},{"source",int(m.source_)},{"destination",int(m.destination_)},{"text",q(m.text_)},{"created",QString::number(m.created_)},{"recalled",m.recalled_},{"read",array(m.readBy_)},{"stars",stars}});
    }
    for(const auto& [id,r]:d.requests_)if(!viewer||r.from()==viewer||r.to()==viewer)requests.append(QJsonObject{{"id",QString::number(id)},{"from",r.from()},{"to",r.to()},{"source",int(r.source())},{"destination",int(r.destination())},{"reason",q(r.reason())},{"status",int(r.status())}});
    QJsonObject out{{"version",1},{"users",users},{"groups",groups},{"messages",messages},{"requests",requests},{"nextGroup",d.nextGroup_},{"nextMessage",QString::number(d.nextMessage_)},{"nextRequest",QString::number(d.nextRequest_)}};
    if(!viewer){
        out["adminName"]=q(d.adminName_);out["adminHash"]=q(d.adminHash_);out["nextAudit"]=QString::number(d.nextAudit_);
        QJsonArray logs;for(const auto& [id,log]:d.adminLogs_)logs.append(QJsonObject{{"id",QString::number(id)},{"created",QString::number(log.created())},{"actor",q(log.actor())},{"action",q(log.action())},{"target",log.target()},{"service",int(log.service())}});out["adminLogs"]=logs;
    }
    return out;
}
DataStoreHsx WorkspaceCodecHsx::decode(const QJsonObject& root){
    require(root.value("version").toInt()==1,"不支持的数据版本");DataStoreHsx d;
    // No real credential is included in a client snapshot.
    static const auto placeholder=PasswordHsx::hash("client-cache-not-a-login-credential");
    for(auto value:root["users"].toArray()){const auto u=value.toObject();const int id=u["id"].toInt();auto user=UserHsx::create(s(u["type"]),id,s(u["name"]),DateHsx(s(u["birth"])),DateHsx(s(u["applied"])),s(u["location"]),u["shared"].toBool());user->online_=u["online"].toBool();user->disabled_=u["disabled"].toBool();
        for(auto v:u["services"].toArray()){const auto a=v.toObject();const auto type=serviceType(a["type"].toInt());ServiceAccountHsx account(type,a["number"].toString().toLongLong(),s(a["nick"]),a["password"].toString().isEmpty()?placeholder:s(a["password"]),a["shared"].toBool());account.boundQQ_=a["bound"].toString().toLongLong();
            for(auto item:a["friends"].toArray()){const auto f=item.toObject();account.friends_.emplace(f["id"].toInt(),FriendHsx(f["id"].toInt(),serviceType(f["contact"].toInt()),s(f["remark"]),s(f["category"])));}
            for(auto item:a["blocked"].toArray())account.blocked_.insert(item.toInt());user->services_.emplace(type,std::move(account));}
        d.users_.emplace(id,std::move(user));}
    for(auto value:root["groups"].toArray()){const auto g=value.toObject();auto group=GroupHsx::create(g["id"].toInt(),s(g["name"]),g["owner"].toInt(),GroupModeHsx(g["native"].toInt()),g["parent"].toInt());group->capacity_=g["capacity"].toInt();group->announcement_=s(g["notice"]);group->policy_=GroupPolicyHsx::create(GroupModeHsx(g["mode"].toInt()));
        for(auto v:g["members"].toArray()){const auto member=v.toObject();for(auto type:member["services"].toArray())group->members_[member["id"].toInt()].insert(serviceType(type.toInt()));}
        for(auto v:g["admins"].toArray())group->admins_.insert(v.toInt());for(auto v:g["pending"].toArray()){const auto a=v.toArray();group->pending_.insert({a[0].toInt(),serviceType(a[1].toInt())});}
        for(auto v:g["mutes"].toArray()){const auto a=v.toArray();group->mutedUntil_[a[0].toInt()]=a[1].toString().toLongLong();}d.groups_.emplace(group->id(),std::move(group));}
    for(auto value:root["messages"].toArray()){const auto m=value.toObject();const auto id=m["id"].toString().toLongLong();MessageHsx message(id,MessageKindHsx(m["kind"].toInt()),m["sender"].toInt(),m["target"].toInt(),serviceType(m["source"].toInt()),serviceType(m["destination"].toInt()),s(m["text"]),m["created"].toString().toLongLong());message.recalled_=m["recalled"].toBool();for(auto v:m["read"].toArray())message.readBy_.insert(v.toInt());for(auto v:m["stars"].toArray())message.stars_.insert(v.toInt());d.messages_.emplace(id,std::move(message));}
    for(auto value:root["requests"].toArray()){const auto r=value.toObject();const auto id=r["id"].toString().toLongLong();FriendRequestHsx request(id,r["from"].toInt(),r["to"].toInt(),serviceType(r["source"].toInt()),serviceType(r["destination"].toInt()),s(r["reason"]));request.status_=RequestStatusHsx(r["status"].toInt());d.requests_.emplace(id,std::move(request));}
    for(auto value:root["adminLogs"].toArray()){const auto l=value.toObject();const auto id=l["id"].toString().toLongLong();d.adminLogs_.emplace(id,AdminLogHsx(id,l["created"].toString().toLongLong(),s(l["actor"]),s(l["action"]),l["target"].toInt(),ServiceHsx(l["service"].toInt())));}
    d.nextAudit_=root["nextAudit"].toString("1").toLongLong();if(!d.adminLogs_.empty())d.nextAudit_=std::max(d.nextAudit_,d.adminLogs_.rbegin()->first+1);
    d.nextGroup_=root["nextGroup"].toInt(1001);d.nextMessage_=root["nextMessage"].toString("1").toLongLong();d.nextRequest_=root["nextRequest"].toString("1").toLongLong();d.adminName_=s(root["adminName"]);d.adminHash_=s(root["adminHash"]);d.dirty_=false;return d;
}
}

