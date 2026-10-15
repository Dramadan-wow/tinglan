#include "UpdateClientHsx.h"
#include "UpdateInfoHsx.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QTextStream>
#include <QTimer>
using namespace im::net;
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);const auto args=app.arguments();if(args.size()<5)return 2;
    if(!newerRelease("5.10.0","5.9.0")||newerRelease("5.5.0","5.5.0")||validReleaseVersion("../evil"))return 3;
    auto event=[](QJsonObject o){QTextStream(stdout)<<QJsonDocument(o).toJson(QJsonDocument::Compact)<<Qt::endl;};
    UpdateClientHsx client;bool cancel=args.size()>5&&args[5]=="cancel";
    QObject::connect(&client,&UpdateClientHsx::available,&app,[&](const QJsonObject& r){event({{"event","available"},{"version",r["version"]}});client.download(args[4]);});
    QObject::connect(&client,&UpdateClientHsx::current,&app,[&](bool p){event({{"event","current"},{"published",p}});app.quit();});
    QObject::connect(&client,&UpdateClientHsx::ready,&app,[&](const QString& path){event({{"event","ready"},{"file",path}});app.quit();});
    QObject::connect(&client,&UpdateClientHsx::failed,&app,[&](const QString& detail){event({{"event","failed"},{"detail",detail}});app.quit();});
    QObject::connect(&client,&UpdateClientHsx::progress,&app,[&](qint64 bytes,qint64){if(cancel){client.cancel();event({{"event","cancelled"},{"bytes",double(bytes)}});app.quit();}});
    QTimer::singleShot(0,&app,[&]{client.check(args[1],args[2]);});
    QTimer::singleShot(args[3].toInt(),&app,[&]{event({{"event","timeout"}});app.exit(4);});
    return app.exec();
}
