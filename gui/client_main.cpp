#include "AppSessionHsx.h"
#include "MainWindowHsx.h"
#include "LoginDialogHsx.h"
#include "ThemeHsx.h"
#include "IconHsx.h"
#include "RuntimeHsx.h"
#include "ConnectionDialogHsx.h"
#include "UpdateManagerHsx.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLockFile>
#include <QMessageBox>
namespace {
bool remoteProfile(const QString& path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return false;
    const auto c=QJsonDocument::fromJson(f.readAll()).object();return !c["host"].toString().isEmpty()&&c["tls"].toBool()&&!c["autoStartLocalServer"].toBool()&&!c["allowInsecureLoopback"].toBool();}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setApplicationName("听澜");app.setOrganizationName("Hsx Coursework");app.setApplicationVersion(IM_APP_VERSION);
    auto* updater=new im::UpdateManagerHsx(&app);
    im::ThemeHsx::install(app);app.setWindowIcon(im::IconHsx::get("brand","#356552"));
    QCommandLineParser parser;parser.setApplicationDescription("听澜 · 普通客户端");parser.addHelpOption();parser.addVersionOption();
    parser.addOption({"client-config","使用指定的 TLS 中心服务器配置","file"});parser.addOption({"connection-settings","打开连接设置"});parser.addOption({"connection-code","导入服务器文字连接码","code"});parser.process(app);
    try{
        auto config=parser.value("client-config");const auto root=im::runtimeDirectory();
        if(config.isEmpty())config=QFile::exists(root+"/connection-active.json")?root+"/connection-active.json":root+"/client.json";
        if(parser.isSet("connection-settings")||parser.isSet("connection-code")||!remoteProfile(config)){
            im::ConnectionDialogHsx settings(config);if(parser.isSet("connection-code"))settings.importCode(parser.value("connection-code"));
            if(settings.exec()!=QDialog::Accepted)return 0;config=settings.savedFile();
        }
        for(;;){
            const auto runtime=im::prepareRuntime(config);QLockFile lock(runtime.data+".lock");
            updater->configure(runtime.config);
            if(!lock.tryLock()){QMessageBox::information(nullptr,"听澜已经打开","请使用已打开的客户端窗口。");return 1;}
            im::AppSessionHsx session(im::AppSessionHsx::path(runtime.data));session.configureRemote(runtime.config);
            QString requested;
            const auto changeConnection=[&]{im::ConnectionDialogHsx settings(config);if(settings.exec()!=QDialog::Accepted)return false;requested=settings.savedFile();return true;};
            for(;;){
                im::LoginDialogHsx login(session,nullptr,false,changeConnection);const auto result=login.exec();
                if(result==2){session.logout();config=requested;break;}if(result!=QDialog::Accepted)return 0;
                bool again=false;im::MainWindowHsx window(session);
                QObject::connect(&window,&im::MainWindowHsx::returnToLogin,&window,[&]{again=window.close();});
                window.show();const auto exitCode=app.exec();session.logout();if(!again)return exitCode;
            }
        }
    }catch(const std::exception& e){QMessageBox::critical(nullptr,"启动失败",QString::fromUtf8(e.what()));return 1;}
}
