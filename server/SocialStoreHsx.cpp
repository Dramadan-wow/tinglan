#include "SocialStoreHsx.h"
#include <QSqlQuery>
#include <QSqlRecord>
#include <QDateTime>
#include <QBuffer>
#include <QImageReader>
#include <QImage>
#include <QFileInfo>
#include <QUuid>
#include <algorithm>
namespace im {
namespace {
QSqlQuery query(QSqlDatabase db,const QString& text,const QVariantList& values={}){
    QSqlQuery q(db);require(q.prepare(text),"数据库语句准备失败");for(const auto& v:values)q.addBindValue(v);
    require(q.exec(),"数据库操作失败");return q;
}
QString content(const QJsonValue& v,int limit,bool empty=false){
    require(v.isString(),"内容格式无效");const auto t=v.toString().trimmed();require((empty||!t.isEmpty())&&t.toUtf8().size()<=limit,"内容为空或超过长度限制");
    for(auto c:t)require(c.unicode()>=32||c==QChar(10)||c==QChar(9),"内容含非法控制字符");return t;
}
const int prices[]={0,1,6,128,648},days[]={0,7,30,365,0};
}
void SocialStoreHsx::migrate(){
    const auto columns=db_.record("posts");
    if(!columns.contains("channel")){const auto backup=db_.databaseName()+".before-social-v5.sqlite";if(!QFileInfo::exists(backup))query(db_,"VACUUM INTO ?",{backup});}
    if(!columns.contains("channel"))query(db_,"ALTER TABLE posts ADD COLUMN channel TEXT NOT NULL DEFAULT 'moments'");
    if(!columns.contains("image"))query(db_,"ALTER TABLE posts ADD COLUMN image BLOB");
    for(const auto* sql:{
        "CREATE TABLE IF NOT EXISTS follows(actor INTEGER,target INTEGER,PRIMARY KEY(actor,target))",
        "CREATE TABLE IF NOT EXISTS social_blocks(actor INTEGER,target INTEGER,PRIMARY KEY(actor,target))",
        "CREATE TABLE IF NOT EXISTS direct_messages(id INTEGER PRIMARY KEY AUTOINCREMENT,sender INTEGER,recipient INTEGER,post INTEGER,text TEXT,created INTEGER,is_read INTEGER DEFAULT 0)",
        "CREATE INDEX IF NOT EXISTS dm_participants ON direct_messages(sender,recipient,id)",
        "CREATE TABLE IF NOT EXISTS memberships(actor INTEGER PRIMARY KEY,tier INTEGER,expires INTEGER)",
        "CREATE TABLE IF NOT EXISTS membership_orders(id TEXT PRIMARY KEY,actor INTEGER,tier INTEGER,price INTEGER,reference TEXT,status TEXT,created INTEGER,note TEXT DEFAULT '')",
        "CREATE UNIQUE INDEX IF NOT EXISTS one_pending_order ON membership_orders(actor) WHERE status='pending'",
        "CREATE UNIQUE INDEX IF NOT EXISTS one_payment_reference ON membership_orders(note) WHERE status='approved'"
    })query(db_,sql);
    if(!db_.record("membership_orders").contains("reviewer"))query(db_,"ALTER TABLE membership_orders ADD COLUMN reviewer TEXT DEFAULT ''");
    if(!db_.record("membership_orders").contains("reviewed"))query(db_,"ALTER TABLE membership_orders ADD COLUMN reviewed INTEGER DEFAULT 0");
}
bool SocialStoreHsx::hasWeibo(int actor)const{return data_.users().count(actor)&&data_.user(actor).services().count(ServiceHsx::Weibo);}
bool SocialStoreHsx::friends(int a,int b)const{
    auto has=[&](int x,int y){for(const auto& [type,s]:data_.user(x).services())if(s.friends().count(y))return true;return false;};
    return has(a,b)&&has(b,a);
}
bool SocialStoreHsx::blocked(int a,int b)const{
    auto q=query(db_,"SELECT 1 FROM social_blocks WHERE (actor=? AND target=?) OR (actor=? AND target=?)",{a,b,b,a});if(q.next())return true;
    for(const auto& [type,s]:data_.user(a).services())if(s.blocked().count(b))return true;
    for(const auto& [type,s]:data_.user(b).services())if(s.blocked().count(a))return true;return false;
}
bool SocialStoreHsx::visible(int actor,int author,const QString& channel)const{
    if(channel=="weibo"&&(!hasWeibo(actor)||!hasWeibo(author)))return false;
    return actor==author||(!blocked(actor,author)&&(channel=="weibo"||friends(actor,author)));
}
QJsonArray SocialStoreHsx::posts(int actor,const QString& channel)const{
    QJsonArray out;auto q=query(db_,"SELECT id,author,text,created,image FROM posts WHERE channel=? ORDER BY id DESC",{channel});
    while(q.next()&&out.size()<20){
        const int author=q.value(1).toInt();if(!data_.users().count(author)||!visible(actor,author,channel))continue;const auto id=q.value(0).toLongLong();
        auto likes=query(db_,"SELECT COUNT(*),COALESCE(SUM(actor=?),0) FROM likes WHERE post=?",{actor,id});likes.next();
        QJsonArray comments;auto c=query(db_,"SELECT author,text,created FROM comments WHERE post=? ORDER BY id DESC LIMIT 10",{id});
        while(c.next())comments.append(QJsonObject{{"author",c.value(0).toInt()},{"text",c.value(1).toString()},{"created",c.value(2).toLongLong()}});
        out.append(QJsonObject{{"id",QString::number(id)},{"author",author},{"text",q.value(2).toString()},{"created",q.value(3).toLongLong()},{"image",QString::fromLatin1(q.value(4).toByteArray().toBase64())},{"channel",channel},{"likes",likes.value(0).toInt()},{"liked",likes.value(1).toInt()>0},{"comments",comments}});
    }return out;
}
QJsonObject SocialStoreHsx::member(int actor)const{
    auto q=query(db_,"SELECT tier,expires FROM memberships WHERE actor=?",{actor});int tier=0;qint64 expires=0;
    if(q.next()){tier=q.value(0).toInt();expires=q.value(1).toLongLong();if(tier!=4&&expires<=QDateTime::currentSecsSinceEpoch())tier=0;}
    return {{"tier",tier},{"expires",expires}};
}
QJsonArray SocialStoreHsx::orders(int actor)const{
    auto q=query(db_,actor?"SELECT id,actor,tier,price,reference,status,created,note,reviewer,reviewed FROM membership_orders WHERE actor=? ORDER BY created DESC LIMIT 100":"SELECT id,actor,tier,price,reference,status,created,note,reviewer,reviewed FROM membership_orders ORDER BY (status='pending') DESC,created DESC LIMIT 2000",actor?QVariantList{actor}:QVariantList{});
    QJsonArray out;while(q.next())out.append(QJsonObject{{"id",q.value(0).toString()},{"user",q.value(1).toInt()},{"tier",q.value(2).toInt()},{"price",q.value(3).toInt()},{"reference",q.value(4).toString()},{"status",q.value(5).toString()},{"created",q.value(6).toLongLong()},{"note",q.value(7).toString()},{"reviewer",q.value(8).toString()},{"reviewed",q.value(9).toLongLong()}});return out;
}
QJsonObject SocialStoreHsx::snapshot(int actor)const{
    QJsonArray follows,blocks,dms;auto f=query(db_,"SELECT target FROM follows WHERE actor=?",{actor});while(f.next())follows.append(f.value(0).toInt());
    auto b=query(db_,"SELECT target FROM social_blocks WHERE actor=?",{actor});while(b.next())blocks.append(b.value(0).toInt());
    auto q=query(db_,"SELECT id,sender,recipient,post,text,created,is_read FROM direct_messages WHERE sender=? OR recipient=? ORDER BY id DESC LIMIT 200",{actor,actor});
    while(q.next())dms.append(QJsonObject{{"id",q.value(0).toString()},{"sender",q.value(1).toInt()},{"recipient",q.value(2).toInt()},{"post",q.value(3).toString()},{"text",q.value(4).toString()},{"created",q.value(5).toLongLong()},{"read",q.value(6).toBool()}});
    const bool enabled=hasWeibo(actor);
    return {{"feed",posts(actor,"moments")},{"weibo",enabled?posts(actor,"weibo"):QJsonArray{}},{"follows",enabled?follows:QJsonArray{}},{"socialBlocks",enabled?blocks:QJsonArray{}},{"directMessages",enabled?dms:QJsonArray{}},{"membership",member(actor)},{"membershipOrders",orders(actor)}};
}
bool SocialStoreHsx::command(int actor,const QJsonObject& a,QJsonObject& result){
    const auto op=a["op"].toString();const auto now=QDateTime::currentSecsSinceEpoch();
    if(op.startsWith("weibo."))require(hasWeibo(actor),"请先在账号页面开通微博服务");
    if(op=="feed.publish"){
        const auto channel=a.value("channel").toString("moments");require(channel=="moments"||channel=="weibo","动态频道无效");
        if(channel=="weibo")require(hasWeibo(actor),"请先在账号页面开通微博服务");
        const auto text=content(a["text"],2000,true);const auto encoded=a["image"].toString().toLatin1();QByteArray photo;
        if(!encoded.isEmpty()){
            require(encoded.size()<=41000,"图片超过上传限制");auto bytes=QByteArray::fromBase64(encoded);require(bytes.toBase64()==encoded&&bytes.size()<=30000,"图片编码无效");
            QBuffer buffer(&bytes);buffer.open(QIODevice::ReadOnly);QImageReader reader(&buffer);const auto size=reader.size();
            require(size.isValid()&&size.width()<=1280&&size.height()<=1280,"图片尺寸无效");const auto image=reader.read();require(!image.isNull(),"图片内容无效");
            QBuffer out(&photo);out.open(QIODevice::WriteOnly);require(image.save(&out,"JPEG",75)&&photo.size()<=100000,"图片转换失败");
        }
        require(!text.isEmpty()||!photo.isEmpty(),"请输入内容或选择图片");auto q=query(db_,"INSERT INTO posts(author,text,created,channel,image) VALUES(?,?,?,?,?)",{actor,text,now,channel,photo});
        result["post"]=q.lastInsertId().toString();return true;
    }
    if(op=="feed.like"||op=="feed.comment"||op=="feed.delete"){
        bool ok=false;const auto post=a["post"].toString().toLongLong(&ok);require(ok&&post>0,"动态编号无效");
        auto q=query(db_,"SELECT author,channel FROM posts WHERE id=?",{post});require(q.next()&&visible(actor,q.value(0).toInt(),q.value(1).toString()),"动态不存在或不可见");
        if(op=="feed.like")query(db_,a["enabled"].toBool()?"INSERT OR IGNORE INTO likes(post,actor) VALUES(?,?)":"DELETE FROM likes WHERE post=? AND actor=?",{post,actor});
        else if(op=="feed.comment")query(db_,"INSERT INTO comments(post,author,text,created) VALUES(?,?,?,?)",{post,actor,content(a["text"],500),now});
        else{require(q.value(0).toInt()==actor,"只能删除自己的动态");for(const auto* table:{"posts","likes","comments"})query(db_,QString("DELETE FROM %1 WHERE %2=?").arg(table,QString(table)=="posts"?"id":"post"),{post});}
        return true;
    }
    if(op=="weibo.follow"||op=="weibo.block"){
        const int target=a["target"].toInt();require(target!=actor&&data_.users().count(target),"用户不存在");
        require(hasWeibo(target),"对方尚未开通微博服务");
        const auto table=op=="weibo.follow"?"follows":"social_blocks";query(db_,a["enabled"].toBool()?QString("INSERT OR IGNORE INTO %1(actor,target) VALUES(?,?)").arg(table):QString("DELETE FROM %1 WHERE actor=? AND target=?").arg(table),{actor,target});return true;
    }
    if(op=="weibo.dm.send"){
        const int target=a["target"].toInt();require(target!=actor&&data_.users().count(target)&&data_.user(target).loginAllowed()&&!blocked(actor,target),"无法向此用户发送私信");
        require(hasWeibo(target),"对方尚未开通微博服务");
        const auto text=content(a["text"],2000);const auto post=a["post"].toString().toLongLong();
        auto conversation=query(db_,"SELECT 1 FROM direct_messages WHERE (sender=? AND recipient=?) OR (sender=? AND recipient=?) LIMIT 1",{actor,target,target,actor});
        if(!conversation.next()){auto p=query(db_,"SELECT 1 FROM posts WHERE id=? AND author=? AND channel='weibo'",{post,target});require(p.next(),"首次私信请从对方的公开微博发起");}
        auto reply=query(db_,"SELECT 1 FROM direct_messages WHERE sender=? AND recipient=? LIMIT 1",{target,actor});
        if(!reply.next()){auto count=query(db_,"SELECT COUNT(*) FROM direct_messages WHERE sender=? AND recipient=? AND created>?",{actor,target,now-86400});count.next();require(count.value(0).toInt()<3,"对方回复前每天最多发送 3 条私信");}
        auto q=query(db_,"INSERT INTO direct_messages(sender,recipient,post,text,created) VALUES(?,?,?,?,?)",{actor,target,post,text,now});result["message"]=q.lastInsertId().toString();return true;
    }
    if(op=="weibo.dm.read"){query(db_,"UPDATE direct_messages SET is_read=1 WHERE recipient=? AND sender=?",{actor,a["target"].toInt()});return true;}
    if(op=="membership.order"){
        const int tier=a["tier"].toInt();require(tier>=1&&tier<=4,"会员档位无效");const auto reference=content(a["reference"],360);require(reference.size()>=6&&reference.size()<=120,"请填写 6–120 字付款凭证");
        const int current=member(actor)["tier"].toInt();require(current<=tier&&current!=4,"当前权益已覆盖该档位");
        auto pending=query(db_,"SELECT 1 FROM membership_orders WHERE actor=? AND status='pending'",{actor});require(!pending.next(),"已有待核验订单，请先等待处理或取消");
        const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);query(db_,"INSERT INTO membership_orders(id,actor,tier,price,reference,status,created) VALUES(?,?,?,?,?,'pending',?)",{id,actor,tier,prices[tier],reference,now});result["order"]=id;return true;
    }
    if(op=="membership.cancel"){auto q=query(db_,"UPDATE membership_orders SET status='cancelled' WHERE id=? AND actor=? AND status='pending'",{a["id"].toString(),actor});require(q.numRowsAffected()==1,"只能取消自己的待核验订单");return true;}
    return false;
}
void SocialStoreHsx::review(const QString& id,bool approve,const QString& note,bool ownTransaction){
    require(note.trimmed().size()>=6&&note.size()<=200,"请输入 6–200 字核验依据（通过时填完整交易单号）");if(ownTransaction)require(db_.transaction(),"无法开始核验事务");
    try{
        auto q=query(db_,"SELECT actor,tier,status FROM membership_orders WHERE id=?",{id});require(q.next()&&q.value(2)=="pending","订单不存在或已经处理");const int actor=q.value(0).toInt(),tier=q.value(1).toInt();
        if(approve){const auto m=member(actor);require(m["tier"].toInt()<=tier&&m["tier"].toInt()!=4,"已有更高或永久权益，请驳回重复订单");
            auto prior=query(db_,"SELECT 1 FROM membership_orders WHERE status='approved' AND note=?",{note.trimmed()});require(!prior.next(),"该交易单号已被核销");
            const auto expires=tier==4?0:std::max(QDateTime::currentSecsSinceEpoch(),m["expires"].toVariant().toLongLong())+days[tier]*86400LL;
            query(db_,"INSERT OR REPLACE INTO memberships(actor,tier,expires) VALUES(?,?,?)",{actor,tier,expires});}
        query(db_,"UPDATE membership_orders SET status=?,note=? WHERE id=?",{approve?"approved":"rejected",note.trimmed(),id});if(ownTransaction)require(db_.commit(),"核验保存失败");
    }catch(...){if(ownTransaction)db_.rollback();throw;}
}
}
