#include "DemoDataHsx.h"
// Isolated test executable. Exercises the same MainWindow/NetworkPage used by im_desktop.
#include "AppSessionHsx.h"
#include "MainWindowHsx.h"
#include "NetworkPageHsx.h"
#include "ThemeHsx.h"
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTextStream>
#include <QTimer>
#include <cstdio>
#include <memory>
namespace {
void event(const QJsonObject& value){const auto bytes=QJsonDocument(value).toJson(QJsonDocument::Compact);fwrite(bytes.constData(),1,size_t(bytes.size()),stdout);fputc('\n',stdout);fflush(stdout);}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);im::ThemeHsx::install(app);
    QTextStream input(stdin);input.setEncoding(QStringConverter::Utf8);const auto config=QJsonDocument::fromJson(input.readLine().toUtf8()).object();
    const auto folder=config.value("directory").toString();if(folder.isEmpty())return 2;
    const auto data=QDir(folder).filePath("data.txt");if(QFile::exists(data))return 3;
    im::AppSessionHsx session(im::DemoDataHsx::fixture(im::AppSessionHsx::path(data)));if(!session.login(10001,im::ServiceHsx::QQ,"123456"))return 4;
    im::MainWindowHsx window(session);window.show();window.navigate(6);auto* page=window.network();
    page->findChild<QLineEdit*>("tcpIp")->setText(config.value("host").toString("127.0.0.1"));
    page->findChild<QSpinBox*>("tcpPort")->setValue(config.value("port").toInt());
    page->findChild<QLineEdit*>("tcpName")->setText(config.value("username").toString());
    page->findChild<QLineEdit*>("tcpPassword")->setText(config.value("password").toString());
    page->findChild<QCheckBox*>("tcpTls")->setChecked(config.value("tls").toBool());
    page->findChild<QLineEdit*>("tcpCaFile")->setText(config.value("caFile").toString());
    auto* client=page->client();
    QObject::connect(client,&im::net::ChatClientHsx::stateChanged,&app,[](const QString& text){event({{"event","state"},{"text",text}});});
    QObject::connect(client,&im::net::ChatClientHsx::authenticatedChanged,&app,[](bool online){event({{"event","online"},{"value",online}});});
    QObject::connect(client,&im::net::ChatClientHsx::chatReceived,&app,[](const QString& sender,const QString& text,const QString& id){event({{"event","chat"},{"sender",sender},{"text",text},{"id",id}});});
    QObject::connect(client,&im::net::ChatClientHsx::messageResult,&app,[](const QString& id,bool ok,const QString& text){event({{"event","ack"},{"id",id},{"ok",ok},{"text",text}});});
    QObject::connect(client,&im::net::ChatClientHsx::problem,&app,[](const QString& text){event({{"event","problem"},{"text",text}});});
    QFile commands(QDir(folder).filePath("commands.jsonl"));qint64 offset=0;QByteArray pending;QTimer timer;timer.setInterval(50);
    QObject::connect(&timer,&QTimer::timeout,&app,[&]{
        if(!commands.open(QIODevice::ReadOnly))return;commands.seek(offset);pending+=commands.readAll();offset=commands.pos();commands.close();
        while(pending.contains('\n')){const auto end=pending.indexOf('\n');const auto cmd=QJsonDocument::fromJson(pending.left(end)).object();pending.remove(0,end+1);const auto action=cmd.value("action").toString();
            if(action=="send"){page->findChild<QLineEdit*>("tcpRecipient")->setText(cmd.value("to").toString());page->findChild<QLineEdit*>("tcpInput")->setText(cmd.value("text").toString());page->findChild<QPushButton*>("tcpSend")->click();}
            else if(action=="stop")page->stop();
            else if(action=="connect"){page->findChild<QLineEdit*>("tcpPassword")->setText(config.value("password").toString());page->connectPeer();}
            else if(action=="shot"){const bool ok=window.grab().save(QDir(folder).filePath("network.png"));event({{"event","screenshot"},{"ok",ok}});}
            else if(action=="quit")app.quit();
            else if(action=="pulse")event({{"event","pulse"}});
        }
    });timer.start();QTimer::singleShot(0,page,[page]{page->findChild<QPushButton*>("tcpConnect")->click();});
    const auto result=app.exec();page->stop();session.logout();return result;
}
