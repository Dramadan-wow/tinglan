// Isolated driver of the real ordinary client library; no fixture or test path in the product.
#include "AppSessionHsx.h"
#include "LoginDialogHsx.h"
#include "MainWindowHsx.h"
#include "ChatPageHsx.h"
#include "ThemeHsx.h"
#include "AvatarHsx.h"
#include "BubbleHsx.h"
#include <QApplication>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QMessageBox>
#include <QTextStream>
#include <QTimer>
#include <memory>
static void output(QJsonObject value){const auto b=QJsonDocument(value).toJson(QJsonDocument::Compact);fwrite(b.constData(),1,size_t(b.size()),stdout);fputc('\n',stdout);fflush(stdout);}
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);im::ThemeHsx::install(app);QTextStream in(stdin);in.setEncoding(QStringConverter::Utf8);
    const auto c=QJsonDocument::fromJson(in.readLine().toUtf8()).object();const auto dir=c["directory"].toString();QDir().mkpath(dir);
    app.setOrganizationName("TinglanIsolatedTests");app.setApplicationName(QFileInfo(dir).fileName());QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir);
    im::AppSessionHsx session(im::AppSessionHsx::path(QDir(dir).filePath("data.txt")));session.configureRemote(c["config"].toString());
    std::unique_ptr<im::MainWindowHsx> window;im::LoginDialogHsx login(session);
    QObject::connect(&session,&im::AppSessionHsx::remoteLoginFinished,&app,[&](bool ok){if(ok&&!window){window=std::make_unique<im::MainWindowHsx>(session);window->show();}output({{"event","login"},{"ok",ok},{"user",session.user()},{"roles",login.findChild<QComboBox*>("loginRole")->count()}});});
    QObject::connect(&session,&im::AppSessionHsx::failed,&app,[](QString s){output({{"event","failure"},{"text",s}});});
    QObject::connect(session.remoteClient(),&im::net::ChatClientHsx::workspaceSnapshot,&app,[](QJsonObject v){output({{"event","snapshot"},{"data",v}});});
    QObject::connect(session.remoteClient(),&im::net::ChatClientHsx::workspaceResult,&app,[](QString id,QJsonObject v){output({{"event","result"},{"id",id},{"data",v}});});
    QObject::connect(&session,&im::AppSessionHsx::connectionChanged,&app,[](QString text){output({{"event","state"},{"text",text}});});
    QTimer dismiss;dismiss.setInterval(30);QObject::connect(&dismiss,&QTimer::timeout,&app,[]{if(auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))box->accept();});dismiss.start();
    QFile commands(QDir(dir).filePath("commands.jsonl"));qint64 offset=0;QByteArray pending;QTimer timer;timer.setInterval(40);
    QObject::connect(&timer,&QTimer::timeout,&app,[&]{
        if(!commands.open(QIODevice::ReadOnly))return;commands.seek(offset);pending+=commands.readAll();offset=commands.pos();commands.close();
        while(pending.contains('\n')){
            const auto end=pending.indexOf('\n');const auto cmd=QJsonDocument::fromJson(pending.left(end)).object();pending.remove(0,end+1);const auto action=cmd["action"].toString();
            if(action=="quit"){session.logout();app.quit();return;}
            if(action=="rpc")session.command(cmd["op"].toString(),cmd["args"].toObject());
            else if(action=="avatar")session.personalize([&]{session.studio().importImage("avatar",cmd["file"].toString());});
            else if(action=="style"){window->navigate(7);window->findChild<QPushButton*>("bubble_"+cmd["bubble"].toString())->click();}
            else if(action=="send"){window->navigate(0);window->chat()->open(im::MessageKindHsx::Private,cmd["target"].toInt());window->chat()->findChild<QLineEdit*>("messageInput")->setText(cmd["text"].toString());window->chat()->findChild<QPushButton*>("sendMessage")->click();}
            else if(action=="render"){
                window->navigate(0);window->chat()->open(im::MessageKindHsx::Private,cmd["target"].toInt());app.processEvents();QJsonArray bubbles,avatars;
                for(auto* label:window->chat()->findChildren<QLabel*>())if(auto* b=dynamic_cast<im::BubbleHsx*>(label))bubbles.append(QJsonObject{{"sender",b->property("sender").toInt()},{"style",b->property("bubbleStyle").toString()},{"text",b->text()}});
                for(auto* label:window->chat()->findChildren<QLabel*>())if(auto* a=dynamic_cast<im::AvatarHsx*>(label)){
                    const auto image=a->grab().toImage().convertToFormat(QImage::Format_RGBA8888);const auto digest=QCryptographicHash::hash(QByteArrayView(reinterpret_cast<const char*>(image.constBits()),image.sizeInBytes()),QCryptographicHash::Sha256).toHex();
                    avatars.append(QJsonObject{{"user",a->property("avatarUser").toInt()},{"custom",a->property("customAvatar").toBool()},{"size",a->width()},{"pixels",QString::fromLatin1(digest)}});
                }
                output({{"event","render"},{"bubbles",bubbles},{"avatars",avatars},{"ok",window->grab().save(QDir(dir).filePath("chat.png"))}});
            }else if(action=="adminAttempt")output({{"event","adminAttempt"},{"ok",session.loginAdmin("admin","not-a-real-password")},{"signedIn",session.adminSignedIn()}});
        }
    });timer.start();login.show();login.findChild<QLineEdit*>("loginUser")->setText(c["nickname"].toString());login.findChild<QLineEdit*>("loginPassword")->setText(c["password"].toString());QTimer::singleShot(0,&login,[&]{login.findChild<QPushButton*>("loginSubmit")->click();});return app.exec();
}
