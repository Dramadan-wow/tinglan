// Server-side fire buddy outfits: accepted for the right membership tier and published in snapshots.
#include "WorkspaceBackendHsx.h"
#include "PasswordHsx.h"
#include <QCoreApplication>
#include <QJsonArray>
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
        std::printf("PASS %d appearance server checks\n",checks);return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
