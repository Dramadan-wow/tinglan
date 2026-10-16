#include "FileTransferManagerHsx.h"
#include "FileMessageCardHsx.h"
#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTimer>
#include <QVBoxLayout>
using namespace im::net;
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);if(app.arguments().size()!=2)return 2;
    QFile input(app.arguments()[1]);if(!input.open(QIODevice::ReadOnly))return 3;const auto cfg=QJsonDocument::fromJson(input.readAll()).object();
    ClientOptionsHsx o;o.host=cfg["host"].toString();o.port=quint16(cfg["port"].toInt());o.caFile=cfg["caFile"].toString();o.workspace=true;o.username=cfg["username"].toString();o.password=cfg["password"].toString();o.service=0;
    ChatClientHsx client;FileTransferManagerHsx files;files.configure(o,cfg["cache"].toString());QJsonArray events;QString lastId;int seq=0;bool dirty=true,autoPause=false;QString actionError;
    QObject::connect(&client,&ChatClientHsx::fileSession,&files,&FileTransferManagerHsx::setSession);
    QObject::connect(&client,&ChatClientHsx::fileNotice,&files,&FileTransferManagerHsx::refresh);
    QObject::connect(&client,&ChatClientHsx::authenticatedChanged,&files,[&](bool online){if(!online)files.disconnected();dirty=true;});
    QObject::connect(&files,&FileTransferManagerHsx::problem,&app,[&](const QString& e){actionError=e;dirty=true;});
    QObject::connect(&files,&FileTransferManagerHsx::filesChanged,&app,[&]{dirty=true;});
    QObject::connect(&files,&FileTransferManagerHsx::taskChanged,&app,[&](const QString& id){dirty=true;const auto t=files.task(id);if(autoPause&&t["offset"].toDouble()>=1024*1024&&t["status"]!="complete"){autoPause=false;QTimer::singleShot(0,&app,[&,id]{files.pause(id);events.append("autoPaused");});}});
    QTimer tick;tick.setInterval(30);QObject::connect(&tick,&QTimer::timeout,&app,[&]{
        QFile command(cfg["command"].toString());if(command.open(QIODevice::ReadOnly)){auto c=QJsonDocument::fromJson(command.readAll()).object();command.close();if(c["seq"].toInt()>seq){seq=c["seq"].toInt();dirty=true;actionError.clear();const auto op=c["op"].toString();autoPause=c["autoPause"].toBool();
            if(op=="upload")lastId=files.upload(0,cfg["peer"].toInt(),c["path"].toString());
            else if(op=="download"){for(auto v:files.cards(0,cfg["peer"].toInt()))if(v.toObject()["fileId"]==c["fileId"]){lastId=files.download(v.toObject(),c["path"].toString());break;}}
            else if(op=="pause")files.pause(c["id"].toString(lastId));else if(op=="resume")files.resume(c["id"].toString(lastId));else if(op=="cancel")files.cancel(c["id"].toString(lastId));
            else if(op=="disconnect")client.stop();else if(op=="connect")client.start(o);else if(op=="refresh")files.refresh();
            else if(op=="screenshot"){QWidget panel;auto* layout=new QVBoxLayout(&panel);for(auto v:files.cards(0,cfg["peer"].toInt()))layout->addWidget(new im::FileMessageCardHsx(files,v.toObject(),cfg["username"]=="张三"));layout->addStretch();panel.resize(400,360);panel.show();app.processEvents();panel.grab().save(c["path"].toString());events.append("screenshot");}
            else if(op=="quit"){app.quit();return;}
        }}
        if(!dirty)return;dirty=false;QJsonObject result{{"seq",seq},{"available",files.available()},{"lastId",lastId},{"last",files.task(lastId)},{"cards",files.cards(0,cfg["peer"].toInt())},{"events",events},{"error",actionError}};
        QSaveFile out(cfg["output"].toString());if(out.open(QIODevice::WriteOnly)){out.write(QJsonDocument(result).toJson(QJsonDocument::Compact));out.commit();}
    });tick.start();client.start(o);QTimer::singleShot(240000,&app,&QCoreApplication::quit);return app.exec();
}
