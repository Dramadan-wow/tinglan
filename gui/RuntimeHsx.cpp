#include "RuntimeHsx.h"
#include "TypesHsx.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QProcess>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif
namespace im {
QString runtimeDirectory(){
    QString root=qEnvironmentVariable("TINGLAN_HOME");
    if(root.isEmpty())root=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    root=QDir(root).absolutePath();require(QDir().mkpath(root),"无法创建用户数据目录");
    return root;
}
RuntimeHsx prepareRuntime(const QString& configOverride){
    const auto app=QCoreApplication::applicationDirPath();const auto root=runtimeDirectory();
    const auto selected=QFile::exists(root+"/connection-active.json")?root+"/connection-active.json":root+"/client.json";
    const auto cfg=configOverride.isEmpty()?selected:QFileInfo(configOverride).absoluteFilePath();
    // An explicit profile never replaces the user's existing local-mode configuration.
    if(configOverride.isEmpty()&&!QFile::exists(cfg))require(QFile::copy(app+"/client.json",cfg),"无法创建连接配置");
    QFile input(cfg);require(input.open(QIODevice::ReadOnly),"无法读取连接配置");const auto c=QJsonDocument::fromJson(input.readAll()).object();
    if(c["autoStartLocalServer"].toBool()&&c["host"]=="127.0.0.1"){
        const auto serverConfig=root+"/server.json";
        if(!QFile::exists(serverConfig)){QSaveFile out(serverConfig);require(out.open(QIODevice::WriteOnly),"无法写入本机服务配置");const auto json=QJsonDocument(QJsonObject{{"listenAddress","127.0.0.1"},{"port",c["port"]},{"tls",false},{"allowInsecureLoopback",true},{"usersFile",root+"/server/users.json"},{"workspaceFile",root+"/server/workspace.sqlite"},{"idleExitMs",60000}}).toJson();require(out.write(json)==json.size()&&out.commit(),"无法保存本机服务配置");}
        // Only packaged, explicitly configured loopback mode starts a local helper.
#ifdef Q_OS_WIN
        const auto exe=QDir::toNativeSeparators(app+"/im_server.exe").toStdWString();auto command=L"\""+exe+L"\" --config \""+QDir::toNativeSeparators(serverConfig).toStdWString()+L"\"";
        STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
        if(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,app.toStdWString().c_str(),&startup,&process)){CloseHandle(process.hThread);CloseHandle(process.hProcess);}
        else throw std::runtime_error("无法启动本机服务器，请检查安装文件");
#else
        require(QProcess::startDetached(app+"/im_server",{"--config",serverConfig},app),"无法启动本机服务器");
#endif
    }
    return {root+"/client-data.txt",cfg};
}
}
