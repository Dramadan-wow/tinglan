#include "AppSessionHsx.h"
#include "DemoDataHsx.h"
#include "MainWindowHsx.h"
#include "AdminWindowHsx.h"
#include "ChatPageHsx.h"
#include "GuiChecksHsx.h"
#include "LoginDialogHsx.h"
#include "ThemeHsx.h"
#include "IconHsx.h"
#include "RuntimeHsx.h"
#include "ConnectionDialogHsx.h"
#include <QFile>
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QMessageBox>
#include <QTemporaryDir>
#include <QTimer>
#include <memory>
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setApplicationName("听澜");app.setOrganizationName("Hsx Coursework");app.setApplicationVersion("5.3.1");im::ThemeHsx::install(app);app.setWindowIcon(im::IconHsx::get("brand","#356552"));
    QCommandLineParser parser;parser.setApplicationDescription("听澜 · 课程即时通信系统 Qt 前端");parser.addOption({"offline-demo","使用独立本机模式（不生成演示账号）"});parser.addHelpOption();parser.addVersionOption();parser.addOption({"data","使用指定数据文件","file"});parser.addOption({"self-test","运行隔离的 GUI 测试并输出截图","directory"});parser.addOption({"preview","渲染演示会话截图后退出","image"});parser.addOption({"admin","默认选择系统管理员登录"});parser.addOption({"preview-admin","渲染隔离的管理员界面后退出","image"});parser.addOption({"client-config","使用指定连接配置（不覆盖本机配置）","file"});parser.addOption({"connection-settings","先打开服务器连接设置"});parser.process(app);
    if((parser.isSet("client-config") && parser.value("client-config").isEmpty()) || (parser.isSet("offline-demo") && (parser.isSet("client-config") || parser.isSet("connection-settings")))){QMessageBox::critical(nullptr,"配置错误","--client-config 必须指定文件，且不能与 --offline-demo 同时使用。");return 1;}
    if(parser.isSet("self-test"))return im::GuiChecksHsx::run(QFileInfo(parser.value("self-test")).absoluteFilePath());
    auto configOverride=parser.isSet("client-config")?QFileInfo(parser.value("client-config")).absoluteFilePath():QString{};
    if(parser.isSet("connection-settings")){
        const auto root=im::runtimeDirectory();const auto current=!configOverride.isEmpty()?configOverride:(QFile::exists(root+"/connection-active.json")?root+"/connection-active.json":root+"/client.json");
        im::ConnectionDialogHsx settings(current);if(settings.exec()!=QDialog::Accepted)return 0;configOverride=settings.savedFile();
    }
    for(;;){
    im::RuntimeHsx runtime;
    if(QFile::exists(QCoreApplication::applicationDirPath()+"/packaged.json")&&!parser.isSet("preview")&&!parser.isSet("preview-admin")){
        try{runtime=im::prepareRuntime(configOverride);}catch(const std::exception& e){QMessageBox::critical(nullptr,"启动失败",QString::fromUtf8(e.what()));return 1;}
    }
    QTemporaryDir previewData;QString file=parser.isSet("data")?QFileInfo(parser.value("data")).absoluteFilePath():QDir(QCoreApplication::applicationDirPath()).filePath("data/tencent_data.txt");if(!runtime.data.isEmpty()&&!parser.isSet("data"))file=runtime.data;
    if(parser.isSet("preview")||parser.isSet("preview-admin"))file=previewData.filePath("data.txt");QDir().mkpath(QFileInfo(file).absolutePath());
    QLockFile lock(file+".lock");if(!lock.tryLock(0)){QMessageBox::information(nullptr,"数据文件正在使用","另一个图形界面实例正在使用此数据文件。请关闭该实例，或使用 --data 指定另一份文件。");return 1;}
    try{
        if(parser.isSet("preview")||parser.isSet("preview-admin"))im::DemoDataHsx::fixture(im::AppSessionHsx::path(file));
        im::AppSessionHsx session(im::AppSessionHsx::path(file));
        if(parser.isSet("preview")||parser.isSet("preview-admin")) {
            std::unique_ptr<QMainWindow> window;
            if(parser.isSet("preview-admin")){session.loginAdmin("admin","admin123456");window=std::make_unique<im::AdminWindowHsx>(session);}
            else {session.login(10001,im::ServiceHsx::QQ,"123456");auto userWindow=std::make_unique<im::MainWindowHsx>(session);userWindow->chat()->open(im::MessageKindHsx::Private,10002);window=std::move(userWindow);}
            window->show();const auto image=parser.value(parser.isSet("preview-admin")?"preview-admin":"preview");QTimer::singleShot(250,window.get(),[&]{app.exit(window->grab().save(image)?0:1);});return app.exec();
        }
        if(!parser.isSet("offline-demo"))session.configureRemote(!configOverride.isEmpty()?configOverride:(runtime.config.isEmpty()?QDir(QCoreApplication::applicationDirPath()).filePath("client.json"):runtime.config));
        while(true) {
            QString requestedConfig;
            std::function<bool()> changeConnection;
            if(!parser.isSet("offline-demo"))changeConnection=[&]{
                const auto current=!configOverride.isEmpty()?configOverride:(runtime.config.isEmpty()?QDir(QCoreApplication::applicationDirPath()).filePath("client.json"):runtime.config);
                im::ConnectionDialogHsx settings(current);if(settings.exec()!=QDialog::Accepted)return false;requestedConfig=settings.savedFile();return true;
            };
            im::LoginDialogHsx login(session,nullptr,parser.isSet("admin"),changeConnection);
            const auto loginResult=login.exec();
            if(loginResult==2){session.logout();configOverride=requestedConfig;break;}
            if(loginResult!=QDialog::Accepted)return 0;
            bool again=false; std::unique_ptr<QMainWindow> window;
            if(session.adminSignedIn()) {
                auto admin=std::make_unique<im::AdminWindowHsx>(session);
                QObject::connect(admin.get(),&im::AdminWindowHsx::returnToLogin,admin.get(),[&]{again=window->close();});QObject::connect(&session,&im::AppSessionHsx::sessionEnded,admin.get(),[&]{again=window->close();});window=std::move(admin);
            } else {
                auto user=std::make_unique<im::MainWindowHsx>(session);
                QObject::connect(user.get(),&im::MainWindowHsx::returnToLogin,user.get(),[&]{again=window->close();});window=std::move(user);
            }
            window->show(); const auto result=app.exec(); session.logout(); window.reset();
            if(!again)return result;
        }
    }catch(const std::exception& e){QMessageBox::critical(nullptr,"启动失败",im::AppSessionHsx::q(e.what()));return 1;}
    }
}
