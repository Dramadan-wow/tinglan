// Offline maintenance bridge: reuse the application's codecs and integrity checks.
#include "WorkspaceCodecHsx.h"
#include "StorageHsx.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QTextStream>
#include <QLockFile>
#include <memory>
static std::filesystem::path path(const QString& s) {
#ifdef _WIN32
    return std::filesystem::path(s.toStdWString());
#else
    return std::filesystem::u8path(s.toStdString());
#endif
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv); const auto a=app.arguments();
    try {
        im::require(a.size()>=3,"read-text FILE | validate JSON | write-text JSON FILE | lock FILE");
        if(a[1]=="lock") {
            QLockFile lock(a[2]+".lock"); im::require(lock.tryLock(0),"Data is in use; close the client/server first");
            QTextStream(stdout)<<"locked"<<Qt::endl; QTextStream(stdin).readLine(); return 0;
        }
        if(a[1]=="read-text") {
            im::DataStoreHsx data;im::StorageHsx::load(data,path(a[2]),false);
            QTextStream(stdout)<<QJsonDocument(im::WorkspaceCodecHsx::encode(data)).toJson(QJsonDocument::Compact);return 0;
        }
        QFile file(a[2]);im::require(file.open(QIODevice::ReadOnly),"Cannot read JSON");
        QJsonParseError error;const auto json=QJsonDocument::fromJson(file.readAll(),&error);
        im::require(error.error==QJsonParseError::NoError&&json.isObject(),"Invalid JSON");
        auto data=im::WorkspaceCodecHsx::decode(json.object());im::StorageHsx::validate(data);
        if(a[1]=="write-text") {im::require(a.size()==4,"Output file required");im::StorageHsx::save(data,path(a[3]));}
        else im::require(a[1]=="validate","Unknown command");
        return 0;
    } catch(const std::exception& e) {QTextStream(stderr)<<QString::fromUtf8(e.what())<<Qt::endl;return 1;}
}
