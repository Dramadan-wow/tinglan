// Server-side fire buddy outfits: accepted for the right membership tier and published in snapshots.
#include "WorkspaceBackendHsx.h"
#include "PasswordHsx.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <cstdio>
#include <stdexcept>
namespace {
int checks=0;
void expect(bool ok,const char* what){++checks;if(!ok)throw std::runtime_error(std::string("FAILED: ")+what);}
template<class F> void rejects(F action,const char* what){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}expect(rejected,what);}
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    try{
        QTemporaryDir dir;expect(dir.isValid(),"temporary workspace directory");
        im::WorkspaceBackendHsx backend(dir.filePath("workspace.sqlite"));
        const int user=backend.registerUser("衣橱测试",im::PasswordHsx::hash("wardrobe-check-2026"),im::ServiceHsx::QQ);backend.online(user,true,im::ServiceHsx::QQ);
        auto dress=[&](const QString& outfit){return backend.command(user,im::ServiceHsx::QQ,QUuid::createUuid().toString(QUuid::WithoutBraces),{{"op","appearance.update"},{"profile",QJsonObject{{"outfit",outfit}}}});};
        auto published=[&]{return backend.snapshot(user)["appearances"].toObject()[QString::number(user)].toObject()["outfit"].toString();};
        expect(dress("scarf")["ok"].toBool()&&published()=="scarf","free outfit is saved and published");
        rejects([&]{dress("crown");},"member-only outfit is refused for a free user");
        expect(published()=="scarf","a refused outfit leaves the previous one in place");
        rejects([&]{dress("cape");},"unknown outfit is refused");

        // One fire buddy per pair of friends: either friend dresses it and both snapshots show the same look.
        const int mate=backend.registerUser("衣橱好友",im::PasswordHsx::hash("wardrobe-check-2026"),im::ServiceHsx::QQ);backend.online(mate,true,im::ServiceHsx::QQ);
        auto run=[&](int actor,const QJsonObject& data){return backend.command(actor,im::ServiceHsx::QQ,QUuid::createUuid().toString(QUuid::WithoutBraces),data);};
        auto pair=[&](int actor,const QString& item){
            QJsonObject p{{"target",actor==user?mate:user},{"profile",QJsonObject{{item=="flame"||item=="mint"||item=="violet"?"pet":"outfit",item}}}};p["op"]="buddy.update";return run(actor,p);};
        auto seen=[&](int viewer,const char* kind){return backend.snapshot(viewer)["buddies"].toObject()[QString::number(viewer==user?mate:user)].toObject()[kind].toString();};
        rejects([&]{pair(user,"scarf");},"strangers cannot share a fire buddy");
        run(user,{{"op","friend.request"},{"target",mate},{"contact",0},{"reason","一起养小火人"}});
        QString request;for(const auto& r:backend.snapshot(mate)["requests"].toArray())if(r.toObject()["to"].toInt()==mate)request=r.toObject()["id"].toVariant().toString();
        expect(!request.isEmpty()&&run(mate,{{"op","friend.respond"},{"message",request},{"enabled",true}})["ok"].toBool(),"the pair become friends");
        expect(pair(user,"scarf")["ok"].toBool()&&seen(user,"outfit")=="scarf"&&seen(mate,"outfit")=="scarf"&&seen(mate,"pet")=="flame","a buddy dressed by one friend looks the same to the other");
        expect(pair(mate,"bow")["ok"].toBool()&&seen(user,"outfit")=="bow","the other friend can change it back");
        rejects([&]{pair(mate,"crown");},"a member-only outfit is refused for a free user");
        const auto order=run(user,{{"op","membership.order"},{"tier",3},{"reference","TEST-BUDDY-ORDER"}})["order"].toString();backend.reviewOrder(order,true,"TEST-BUDDY-PAYMENT-0001");
        expect(pair(user,"crown")["ok"].toBool()&&seen(mate,"outfit")=="crown","a member's crown shows on the shared buddy for both friends");
        {auto db=QSqlDatabase::addDatabase("QSQLITE","expire-membership");db.setDatabaseName(dir.filePath("workspace.sqlite"));expect(db.open(),"second connection to the test database");
            QSqlQuery q(db);q.prepare("UPDATE memberships SET expires=1 WHERE actor=?");q.addBindValue(user);expect(q.exec(),"membership expires");db.close();}
        QSqlDatabase::removeDatabase("expire-membership");
        expect(seen(mate,"outfit")=="none"&&seen(user,"outfit")=="none","the crown falls back once the friend who chose it loses the tier");
        std::printf("PASS %d appearance server checks\n",checks);return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
