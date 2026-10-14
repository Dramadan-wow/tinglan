#include "StudioStoreHsx.h"
#include "AppearanceHsx.h"
#include <QBuffer>
#include <QPixmap>
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
QString encodedAvatar(const QImage& image){
    for(int edge:{192,160,128})for(int quality:{85,65,45}){
        QByteArray bytes;QBuffer output(&bytes);output.open(QIODevice::WriteOnly);
        if(image.scaled(edge,edge,Qt::KeepAspectRatio,Qt::SmoothTransformation).save(&output,"JPEG",quality)&&bytes.size()<=24576)return QString::fromLatin1(bytes.toBase64());
    }throw std::runtime_error("无法将头像压缩到上传限制以内");
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
QJsonArray StudioStoreHsx::catalog(const QString& kind) {return AppearanceHsx::catalog(kind);}
QJsonObject StudioStoreHsx::item(const QString& kind,const QString& id) {return AppearanceHsx::item(kind,id);}
QJsonObject StudioStoreHsx::plan(int tier) {
    static const char* names[]={"免费用户","星芽","月汐","星河","云境"};static const int prices[]={0,1,6,128,648},days[]={0,7,30,365,0};
    ensure(tier>=0&&tier<=4,"会员档位不存在");return {{"tier",tier},{"name",names[tier]},{"price",prices[tier]},{"days",days[tier]},{"duration",tier==4?QString("永久"):QString("%1 天").arg(days[tier])}};
}
int StudioStoreHsx::actor() const {ensure(session_.signedIn(),"请先以普通用户身份登录");return session_.user();}
QJsonObject StudioStoreHsx::profile(int user) const {
    auto p=root_.value("profiles").toObject().value(QString::number(user)).toObject();
    if(session_.remote()){
        // Theme/background stay on this computer; public identity comes only from the server.
        auto shared=session_.social()["appearances"].toObject()[QString::number(user)].toObject();
        for(const auto* kind:{"avatar","bubble","frame","pet","tier","expires"})p.remove(kind);
        for(auto it=shared.begin();it!=shared.end();++it)p[it.key()]=it.value();
    }return p;
}
QJsonObject StudioStoreHsx::membership(int user) const {
    if(session_.remote())return user==session_.user()?session_.social()["membership"].toObject():QJsonObject{{"tier",profile(user)["tier"].toInt()}};
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
    if(session_.remote()&&kind=="avatar")return {};
    if(kind!="avatar"&&kind!="background")return {};const auto name=profile(user).value(kind).toString();
    if(name.isEmpty()||QFileInfo(name).fileName()!=name)return {};const auto path=QDir(mediaDir_).filePath(name);return QFile::exists(path)?path:QString{};
}
QPixmap StudioStoreHsx::avatar(int user)const{
    if(!session_.remote())return QPixmap(media(user,"avatar"));
    QPixmap out;const auto encoded=profile(user)["avatar"].toString().toLatin1();
    if(encoded.size()<=32768)out.loadFromData(QByteArray::fromBase64(encoded));return out;
}
void StudioStoreHsx::syncLegacyAppearance(){
    const int user=actor();if(session_.social()["appearances"].toObject().contains(QString::number(user)))return;
    const auto old=root_["profiles"].toObject()[QString::number(user)].toObject();QJsonObject patch;
    for(const auto* kind:{"bubble","frame","pet"}){
        const auto id=old[kind].toString();const auto style=item(kind,id);
        if(!style.isEmpty()&&style["tier"].toInt()<=membership(user)["tier"].toInt())patch[kind]=id;
    }
    const auto name=old["avatar"].toString();
    if(!name.isEmpty()&&QFileInfo(name).fileName()==name){QImageReader reader(QDir(mediaDir_).filePath(name));const auto size=reader.size();
        if(size.isValid()&&qint64(size.width())*size.height()<=40000000){reader.setScaledSize(size.scaled(192,192,Qt::KeepAspectRatio));const auto image=reader.read();if(!image.isNull())patch["avatar"]=encodedAvatar(image);}}
    if(!patch.isEmpty())session_.command("appearance.update",{{"profile",patch},{"createOnly",true}}, {},"原有头像与装扮已同步到服务器");
}
void StudioStoreHsx::commit(const QJsonObject& next) {
    QDir().mkpath(QFileInfo(file_).absolutePath());QSaveFile file(file_);ensure(file.open(QIODevice::WriteOnly),"无法保存装扮与会员信息");const auto bytes=QJsonDocument(next).toJson();ensure(file.write(bytes)==bytes.size()&&file.commit(),"装扮与会员信息保存失败，操作未生效");root_=next;
}
void StudioStoreHsx::updateProfile(const QJsonObject& value) {auto next=root_,profiles=next.value("profiles").toObject();profiles[QString::number(actor())]=value;next["profiles"]=profiles;commit(next);}
void StudioStoreHsx::setChoice(const QString& kind,const QString& value) {
    const int user=actor();const auto selected=item(kind,value);ensure(!selected.isEmpty(),"装扮不存在");ensure(selected.value("tier").toInt()<=membership(user).value("tier").toInt(),"此装扮需开通对应会员后使用");
    if(session_.remote()&&kind!="theme"){ensure(session_.command("appearance.update",{{"profile",QJsonObject{{kind,value}}}}, {},"装扮已由服务器保存并同步"),"装扮未发送，请连接服务器后重试");return;}
    auto p=root_["profiles"].toObject()[QString::number(user)].toObject();p[kind]=value;updateProfile(p);
}
void StudioStoreHsx::importImage(const QString& kind,const QString& source) {
    const int user=actor();ensure(kind=="avatar"||kind=="background","不支持的图片类型");const QFileInfo info(source);ensure(info.exists()&&info.size()<=20*1024*1024,"请选择 20 MB 以内的图片");
    QImageReader reader(source);reader.setAutoTransform(true);const auto size=reader.size();ensure(size.isValid()&&static_cast<qint64>(size.width())*size.height()<=40000000,"图片尺寸无效或超过 4000 万像素");const int limit=kind=="avatar"?512:1920;reader.setScaledSize(size.scaled(limit,limit,Qt::KeepAspectRatio));const auto image=reader.read();ensure(!image.isNull(),"无法读取此图片，请选择 PNG、JPG 或 BMP 文件");
    if(session_.remote()&&kind=="avatar"){ensure(session_.command("appearance.update",{{"profile",QJsonObject{{"avatar",encodedAvatar(image)}}}}, {},"头像已由服务器保存并同步"),"头像未发送，请连接服务器后重试");return;}
    ensure(QDir().mkpath(mediaDir_),"无法创建个人图片目录");const auto name=QUuid::createUuid().toString(QUuid::WithoutBraces)+".png";QSaveFile output(QDir(mediaDir_).filePath(name));ensure(output.open(QIODevice::WriteOnly)&&image.save(&output,"PNG")&&output.commit(),"图片保存失败");auto p=root_["profiles"].toObject()[QString::number(user)].toObject();p[kind]=name;updateProfile(p);
}
void StudioStoreHsx::clearImage(const QString& kind) {ensure(kind=="avatar"||kind=="background","不支持的图片类型");
    if(session_.remote()&&kind=="avatar"){ensure(session_.command("appearance.update",{{"profile",QJsonObject{{"avatar",""}}}}, {},"头像已恢复并同步"),"头像恢复未发送");return;}
    auto p=root_["profiles"].toObject()[QString::number(actor())].toObject();p.remove(kind);updateProfile(p);}
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
