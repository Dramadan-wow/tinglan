#include "StudioStoreHsx.h"
#include "AppSessionHsx.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonDocument>
#include <QMap>
#include <QSaveFile>
#include <QUuid>
#include <algorithm>
namespace im {
namespace {
void ensure(bool ok, const QString& message) { if (!ok) throw std::runtime_error(AppSessionHsx::s(message)); }
QJsonObject style(const char* id,const char* name,int tier,const char* a,const char* b,const char* ink,const char* effect="plain") {
    return {{"id",id},{"name",name},{"tier",tier},{"a",a},{"b",b},{"ink",ink},{"effect",effect}};
}
}
StudioStoreHsx::StudioStoreHsx(AppSessionHsx& session,const QString& file)
    :session_(session),file_(file+".studio.json"),mediaDir_(file+".media") {
    root_={{"version",1},{"profiles",QJsonObject{}},{"orders",QJsonArray{}}};
    if(!QFile::exists(file_))return;
    QFile input(file_);ensure(input.open(QIODevice::ReadOnly),"无法读取装扮与会员存档");
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(input.readAll(),&error);
    ensure(error.error==QJsonParseError::NoError&&doc.isObject(),"装扮与会员存档损坏，请保留原文件并检查");
    const auto saved=doc.object();ensure(saved.value("version").toInt()==1&&saved.value("profiles").isObject()&&saved.value("orders").isArray(),"不支持的装扮与会员存档格式");root_=saved;
    for(const auto& value:root_.value("orders").toArray()){const auto o=value.toObject();const int tier=o.value("tier").toInt();ensure(!o.value("id").toString().isEmpty()&&o.value("user").toInt()>0&&tier>=1&&tier<=4,"会员订单记录损坏");ensure(o.value("price")==plan(tier).value("price")&&QStringList{"pending","approved","rejected","cancelled"}.contains(o.value("status").toString()),"会员订单金额或状态无效");}
}
QJsonArray StudioStoreHsx::catalog(const QString& kind) {
    if(kind=="theme")return {style("jade","听澜青",0,"#356552","#eef5e9","#173e34"),style("ocean","晴空蓝",0,"#3977b7","#eef5fd","#173b5f"),style("lilac","鸢尾紫",0,"#8164b3","#f5f0fb","#36264f"),style("rose","落樱粉",0,"#b66986","#fff0f5","#592e43"),style("amber","日落橙",0,"#b77b3c","#fff7e9","#533c25"),style("night","星夜靛",0,"#5768ae","#f0f2fc","#252e53")};
    if(kind=="bubble")return {
        style("jade","初见青竹",0,"#dcebd5","#dcebd5","#304d35"),style("sky","晴空来信",0,"#ddecff","#e9f3ff","#274c70"),style("peach","蜜桃心事",0,"#ffe1e8","#fff1ef","#7d4157"),style("lilac","紫藤花语",0,"#e8dffc","#f3edff","#5a467b"),style("sun","橘子汽水",0,"#ffe8ba","#fff3d6","#79522e"),style("paper","素笺留白",0,"#ffffff","#f7f8fc","#47566c"),
        style("mint","薄荷汽泡",1,"#a9f1df","#ddfcef","#216352","dots"),style("sakura","樱花信笺",1,"#f9cbe1","#fce9f5","#824765","flowers"),
        style("candy","糖果星球",2,"#f7c7ff","#bedfff","#63407c","stars"),style("ocean","海盐蓝调",2,"#95d8ff","#d6f4ff","#255b80","waves"),style("sunset","落日橘海",2,"#ffc89b","#ffe2e9","#864c3d","dots"),
        style("aurora","极光流彩",3,"#b3eee2","#d4c4ff","#425b72","aurora"),style("galaxy","银河漫游",3,"#41497e","#7461a3","#ffffff","stars"),
        style("gold","云境鎏金",4,"#ffedb2","#efd4a2","#7d5930","gold"),style("prism","琉璃幻境",4,"#eec7ff","#adf5ed","#574678","prism")};
    if(kind=="frame")return {style("none","自然头像",0,"#ffffff","#ffffff","#ffffff"),style("mint","青芽环",1,"#70cba3","#a8ead2","#ffffff"),style("rose","月汐环",2,"#ed91bd","#b8a3fa","#ffffff"),style("star","星河环",3,"#798be8","#89dfec","#ffffff"),style("gold","云境金环",4,"#d7a74a","#ffe8a4","#ffffff")};
    if(kind=="pet")return {style("flame","暖橙小火人",0,"#ff963b","#ffd56e","#603d29"),style("mint","薄荷精灵",1,"#52c7a8","#bcf2bb","#325b4f"),style("pink","樱粉小火人",2,"#eb80ac","#ffd0e8","#6b3e57"),style("violet","星焰精灵",3,"#9b81ed","#d6c8ff","#4c3e75"),style("gold","鎏金守护者",4,"#e7ad3d","#fff3a9","#694918")};
    return {};
}
QJsonObject StudioStoreHsx::item(const QString& kind,const QString& id) { for(const auto& value:catalog(kind))if(value.toObject().value("id")==id)return value.toObject();return {}; }
QJsonObject StudioStoreHsx::plan(int tier) {
    static const char* names[]={"免费用户","星芽","月汐","星河","云境"};static const int prices[]={0,1,6,128,648},days[]={0,7,30,365,0};
    ensure(tier>=0&&tier<=4,"会员档位不存在");return {{"tier",tier},{"name",names[tier]},{"price",prices[tier]},{"days",days[tier]},{"duration",tier==4?QString("永久"):QString("%1 天").arg(days[tier])}};
}
int StudioStoreHsx::actor() const {ensure(session_.signedIn(),"请先以普通用户身份登录");return session_.user();}
QJsonObject StudioStoreHsx::profile(int user) const {return root_.value("profiles").toObject().value(QString::number(user)).toObject();}
QJsonObject StudioStoreHsx::membership(int user) const {
    if(session_.remote())return user==session_.user()?session_.social()["membership"].toObject():QJsonObject{{"tier",0}};
    const auto p=profile(user);int tier=p.value("tier").toInt();const qint64 expires=static_cast<qint64>(p.value("expires").toDouble());
    if(tier<1||tier>4||(tier!=4&&expires<=QDateTime::currentSecsSinceEpoch())||(tier==4&&expires!=0&&expires<=QDateTime::currentSecsSinceEpoch()))tier=0;
    return {{"tier",tier},{"expires",static_cast<double>(expires)}};
}
QString StudioStoreHsx::membershipText(int user) const {
    const auto member=membership(user);const int tier=member.value("tier").toInt();if(!tier)return "免费用户 · 自由装扮日常";
    const auto until=static_cast<qint64>(member.value("expires").toDouble());return plan(tier).value("name").toString()+"会员 · "+(until?QDateTime::fromSecsSinceEpoch(until).toString("yyyy-MM-dd 到期"):"永久权益");
}
QString StudioStoreHsx::choice(int user,const QString& kind) const {
    const auto all=catalog(kind);if(all.isEmpty())return {};const auto stored=profile(user).value(kind).toString();const auto selected=item(kind,stored);
    return !selected.isEmpty()&&selected.value("tier").toInt()<=membership(user).value("tier").toInt()?stored:all.first().toObject().value("id").toString();
}
QString StudioStoreHsx::media(int user,const QString& kind) const {
    if(kind!="avatar"&&kind!="background")return {};const auto name=profile(user).value(kind).toString();
    if(name.isEmpty()||QFileInfo(name).fileName()!=name)return {};const auto path=QDir(mediaDir_).filePath(name);return QFile::exists(path)?path:QString{};
}
void StudioStoreHsx::commit(const QJsonObject& next) {
    QDir().mkpath(QFileInfo(file_).absolutePath());QSaveFile file(file_);ensure(file.open(QIODevice::WriteOnly),"无法保存装扮与会员信息");const auto bytes=QJsonDocument(next).toJson();ensure(file.write(bytes)==bytes.size()&&file.commit(),"装扮与会员信息保存失败，操作未生效");root_=next;
}
void StudioStoreHsx::updateProfile(const QJsonObject& value) {auto next=root_,profiles=next.value("profiles").toObject();profiles[QString::number(actor())]=value;next["profiles"]=profiles;commit(next);}
void StudioStoreHsx::setChoice(const QString& kind,const QString& value) {
    const int user=actor();const auto selected=item(kind,value);ensure(!selected.isEmpty(),"装扮不存在");ensure(selected.value("tier").toInt()<=membership(user).value("tier").toInt(),"此装扮需开通对应会员后使用");auto p=profile(user);p[kind]=value;updateProfile(p);
}
void StudioStoreHsx::importImage(const QString& kind,const QString& source) {
    const int user=actor();ensure(kind=="avatar"||kind=="background","不支持的图片类型");const QFileInfo info(source);ensure(info.exists()&&info.size()<=20*1024*1024,"请选择 20 MB 以内的图片");
    QImageReader reader(source);reader.setAutoTransform(true);const auto size=reader.size();ensure(size.isValid()&&static_cast<qint64>(size.width())*size.height()<=40000000,"图片尺寸无效或超过 4000 万像素");const int limit=kind=="avatar"?512:1920;reader.setScaledSize(size.scaled(limit,limit,Qt::KeepAspectRatio));const auto image=reader.read();ensure(!image.isNull(),"无法读取此图片，请选择 PNG、JPG 或 BMP 文件");
    ensure(QDir().mkpath(mediaDir_),"无法创建个人图片目录");const auto name=QUuid::createUuid().toString(QUuid::WithoutBraces)+".png";QSaveFile output(QDir(mediaDir_).filePath(name));ensure(output.open(QIODevice::WriteOnly)&&image.save(&output,"PNG")&&output.commit(),"图片保存失败");auto p=profile(user);p[kind]=name;updateProfile(p);
}
void StudioStoreHsx::clearImage(const QString& kind) {ensure(kind=="avatar"||kind=="background","不支持的图片类型");auto p=profile(actor());p.remove(kind);updateProfile(p);}
QString StudioStoreHsx::submitOrder(int tier,const QString& reference) {
    const int user=actor();ensure(tier>=1&&tier<=4,"请选择会员档位");ensure(reference.trimmed().size()>=6&&reference.trimmed().size()<=120,"请填写 6–120 字的交易单号或付款备注");const int current=membership(user).value("tier").toInt();ensure(current<=tier&&current!=4,"当前权益已覆盖该档位，请选择同档续期或更高档位");
    auto next=root_;auto list=next.value("orders").toArray();for(const auto& value:list){const auto row=value.toObject();ensure(!(row.value("user").toInt()==user&&row.value("status")=="pending"),"已有待核验订单，请等待处理或先取消原订单");}
    const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);list.append(QJsonObject{{"id",id},{"user",user},{"tier",tier},{"price",plan(tier).value("price")},{"created",static_cast<double>(QDateTime::currentSecsSinceEpoch())},{"reference",reference.trimmed()},{"status","pending"}});next["orders"]=list;commit(next);return id;
}
QJsonArray StudioStoreHsx::orders(bool all) const {if(session_.remote()){ensure(!all||session_.adminSignedIn(),"需要服务器管理员登录");return session_.social()["membershipOrders"].toArray();}if(all)session_.admins().requireAdmin();else actor();QJsonArray result;const auto list=root_.value("orders").toArray();for(auto it=list.constEnd();it!=list.constBegin();){const auto value=*--it;if(all||value.toObject().value("user").toInt()==session_.user())result.append(value);}return result;}
void StudioStoreHsx::cancelOrder(const QString& id) {
    const int user=actor();auto next=root_;auto list=next.value("orders").toArray();for(qsizetype i=0;i<list.size();++i){auto order=list.at(i).toObject();if(order.value("id")!=id)continue;ensure(order.value("user").toInt()==user&&order.value("status")=="pending","只能取消自己的待核验订单");order["status"]="cancelled";order["reviewed"]=static_cast<double>(QDateTime::currentSecsSinceEpoch());list[i]=order;next["orders"]=list;commit(next);return;}ensure(false,"订单不存在");
}
void StudioStoreHsx::decideOrder(const QString& id,bool approve,const QString& note) {
    session_.admins().requireAdmin();ensure(note.trimmed().size()>=6&&note.trimmed().size()<=200,"请填写 6–200 字的核验依据或驳回原因");auto next=root_;auto list=next.value("orders").toArray();
    if(approve)for(const auto& value:list){const auto prior=value.toObject();ensure(!(prior.value("status")=="approved"&&prior.value("note").toString()==note.trimmed()),"该微信交易单号已用于开通，请勿重复核销");}
    for(qsizetype i=0;i<list.size();++i){auto order=list.at(i).toObject();if(order.value("id")!=id)continue;ensure(order.value("status")=="pending","订单已处理，不能重复开通");const int user=order.value("user").toInt(),tier=order.value("tier").toInt();session_.data().user(user);ensure(tier>=1&&tier<=4&&order.value("price")==plan(tier).value("price"),"订单档位或金额不匹配");
        const auto now=QDateTime::currentSecsSinceEpoch();if(approve){const auto member=membership(user);const int current=member.value("tier").toInt();ensure(current<=tier&&current!=4,"用户已有更高或永久权益，请核对后驳回此订单");auto profiles=next.value("profiles").toObject(),p=profile(user);const auto base=std::max(now,static_cast<qint64>(member.value("expires").toDouble()));p["tier"]=tier;p["expires"]=tier==4?0.0:static_cast<double>(base+plan(tier).value("days").toInt()*86400LL);profiles[QString::number(user)]=p;next["profiles"]=profiles;}
        order["status"]=approve?"approved":"rejected";order["note"]=note.trimmed();order["reviewer"]=AppSessionHsx::q(session_.admins().name());order["reviewed"]=static_cast<double>(now);list[i]=order;next["orders"]=list;commit(next);return;
    }ensure(false,"订单不存在");
}
QJsonObject StudioStoreHsx::streak(int other) const {
    const int user=actor();QMap<QDate,int> days;const auto today=QDate::currentDate();
    for(const auto& entry:session_.data().messages()){const auto& m=entry.second;if(m.kind()!=MessageKindHsx::Private||m.recalled())continue;const bool outgoing=m.sender()==user&&m.target()==other&&m.source()==session_.service(),incoming=m.sender()==other&&m.target()==user&&m.destination()==session_.service();if(!outgoing&&!incoming)continue;const auto date=QDateTime::fromSecsSinceEpoch(m.created()).date();if(date<=today)days[date]|=outgoing?1:2;}
    int count=0;auto date=days.value(today)==3?today:today.addDays(-1);while(days.value(date)==3){++count;date=date.addDays(-1);}return {{"days",count},{"level",std::min(5,1+count/7)},{"today",days.value(today)},{"active",count>0}};
}
}
