#include "ChatServerHsx.h"
#include "WorkspaceBackendHsx.h"
#include <memory>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTcpSocket>
#include <QElapsedTimer>
#include <QLockFile>
#include <QTextStream>
#include <csignal>
#include <cstdio>
#include <stdexcept>
namespace {
volatile std::sig_atomic_t stopped=0;
void onSignal(int){stopped=1;}
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int integer(const QJsonObject& o,const QString& key,int fallback,int minimum,int maximum){
    if(!o.contains(key))return fallback;const auto v=o.value(key);const double n=v.toDouble(-1);
    require(v.isDouble()&&n>=minimum&&n<=maximum&&n==int(n),"Invalid integer configuration");return int(n);
}
bool boolean(const QJsonObject& o,const QString& key,bool fallback){if(!o.contains(key))return fallback;require(o.value(key).isBool(),"Invalid boolean configuration");return o.value(key).toBool();}
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);app.setApplicationName("tinglan-server");app.setApplicationVersion("1.0");
    QCommandLineParser parser;parser.addHelpOption();parser.addVersionOption();
    parser.addOption({"import-audit","Import existing matching local audit records without credentials (server must be stopped)","path"});
    parser.addOption({"set-admin","Set server admin using TINGLAN_ADMIN_PASSWORD environment variable (server must be stopped)","name"});
    parser.addOption({"config","Server JSON configuration (required)","path"});
    parser.addOption({"add-user","Provision an immutable remote account ID; password is read from stdin","username"});parser.addOption({"orders","List server membership orders (stop server first)"});parser.addOption({"review-order","Review a membership order locally","id"});parser.addOption({"approve","Confirm payment received"});parser.addOption({"note","Full payment transaction ID or rejection reason","text"});parser.process(app);
    try{
        require(parser.isSet("config"),"Use --config <server.json>");
        const QFileInfo configPath(parser.value("config"));QFile configFile(configPath.absoluteFilePath());
        require(configFile.open(QIODevice::ReadOnly)&&configFile.size()<65536,"Cannot read server configuration");
        QJsonParseError parse;const auto doc=QJsonDocument::fromJson(configFile.readAll(),&parse);
        require(parse.error==QJsonParseError::NoError&&doc.isObject(),"Invalid configuration JSON");const auto config=doc.object();
        auto path=[&](const QString& key){require(config.value(key).isString(),"Missing path configuration");const auto value=config.value(key).toString();require(!value.isEmpty(),"Empty path configuration");return QDir(configPath.absolutePath()).absoluteFilePath(value);};
        const auto usersFile=path("usersFile");require(QDir().mkpath(QFileInfo(usersFile).absolutePath()),"Cannot create server data directory");
        QLockFile lock(usersFile+".lock");require(lock.tryLock(0),"Server/account store already in use; stop server before provisioning");
        im::net::AuthStoreHsx auth(usersFile);
        if(parser.isSet("add-user")){
            QTextStream input(stdin);input.setEncoding(QStringConverter::Utf8);QString password=input.readLine(129);
            require(password.size()<=128,"Password too long");auth.addUser(parser.value("add-user"),password);password.fill(QChar(0));
            QTextStream(stdout)<<"Created account: "<<parser.value("add-user")<<Qt::endl;return 0;
        }
        im::net::ServerOptionsHsx options;
        require(config.value("listenAddress").isString()&&options.address.setAddress(config.value("listenAddress").toString()),"listenAddress must be a numeric IP");
        options.port=quint16(integer(config,"port",56000,0,65535));options.tls=boolean(config,"tls",true);options.allowInsecureLoopback=boolean(config,"allowInsecureLoopback",false);
        options.maxConnections=integer(config,"maxConnections",128,1,512);options.idleTimeoutMs=integer(config,"idleTimeoutMs",60000,2000,300000);options.loginTimeoutMs=integer(config,"loginTimeoutMs",30000,2000,60000);
        if(options.tls){options.certificateFile=path("certificateFile");options.keyFile=path("keyFile");}
        options.updatesDirectory=config.contains("updatesDirectory")?path("updatesDirectory"):QDir(configPath.absolutePath()).filePath("updates");
        std::unique_ptr<im::WorkspaceBackendHsx> workspace;
        std::unique_ptr<QLockFile> workspaceLock;
        if(config.contains("workspaceFile")){
            const auto file=path("workspaceFile");require(QDir().mkpath(QFileInfo(file).absolutePath()),"Cannot create workspace directory");
            workspaceLock=std::make_unique<QLockFile>(file+".lock");require(workspaceLock->tryLock(0),"Workspace database already in use");
        }
        if(config.contains("workspaceFile"))workspace=std::make_unique<im::WorkspaceBackendHsx>(path("workspaceFile"),config.contains("workspaceImport")?path("workspaceImport"):QString{});
        if(parser.isSet("import-audit")){require(bool(workspace),"workspaceFile required");QTextStream(stdout)<<"Imported audit records: "<<workspace->importAudit(parser.value("import-audit"))<<Qt::endl;return 0;}
        if(parser.isSet("set-admin")){require(bool(workspace),"workspaceFile required");workspace->setAdmin(parser.value("set-admin"),qEnvironmentVariable("TINGLAN_ADMIN_PASSWORD"));QTextStream(stdout)<<"Administrator credential saved"<<Qt::endl;return 0;}
        if(parser.isSet("orders")||parser.isSet("review-order")){require(bool(workspace),"workspaceFile required");if(parser.isSet("review-order"))workspace->reviewOrder(parser.value("review-order"),parser.isSet("approve"),parser.value("note"));QTextStream(stdout)<<QJsonDocument(workspace->orders()).toJson()<<Qt::endl;return 0;}
        im::net::ChatServerHsx server(auth,options);server.setWorkspace(workspace.get());
        QObject::connect(&server,&im::net::ChatServerHsx::logLine,&app,[](const QString& line){QTextStream(stdout)<<QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)<<" "<<line<<Qt::endl;});
        QString error;require(server.start(error),error.toUtf8().constData());
        std::signal(SIGINT,onSignal);std::signal(SIGTERM,onSignal);QTimer signalTimer;signalTimer.setInterval(200);
        const int idleExit=integer(config,"idleExitMs",0,0,3600000);QElapsedTimer idle;idle.start();
        QObject::connect(&signalTimer,&QTimer::timeout,&app,[&]{if(stopped)app.quit();if(idleExit){bool busy=false;for(auto* socket:server.findChildren<QTcpSocket*>())if(socket->state()==QAbstractSocket::ConnectedState)busy=true;if(busy)idle.restart();else if(idle.elapsed()>idleExit)app.quit();}});signalTimer.start();
        QObject::connect(&app,&QCoreApplication::aboutToQuit,&server,&im::net::ChatServerHsx::shutdown);
        return app.exec();
    }catch(const std::exception& e){QTextStream(stderr)<<"Server error: "<<e.what()<<Qt::endl;return 1;}
}
