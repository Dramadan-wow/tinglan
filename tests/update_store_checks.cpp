// The server's published update feeds: the Windows installer and the Android APK are read from
// separate manifests, each only for its own platform, and never from a path outside the folder.
#include "UpdateInfoHsx.h"
#include "UpdateStoreHsx.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <cstdio>
#include <stdexcept>
using namespace im::net;
namespace {
int checks=0;
void expect(bool ok,const char* what){++checks;if(!ok)throw std::runtime_error(std::string("FAILED: ")+what);}
void write(const QString& path,const QByteArray& bytes){QFile f(path);if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size())throw std::runtime_error("write failed");}
QJsonObject manifest(const QString& platform,const QString& version,const QByteArray& body){
    return {{"schema",1},{"edition","client"},{"platform",platform},{"version",version},{"size",double(body.size())},
            {"sha256",QString::fromLatin1(QCryptographicHash::hash(body,QCryptographicHash::Sha256).toHex())},{"notes","说明"}};
}
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    try{
        expect(validReleaseVersion("1.3.3")&&!validReleaseVersion("1a3b3")&&!validReleaseVersion("../1.3"),"versions are three dotted numbers only");
        expect(validUpdatePlatform("android")&&validUpdatePlatform("windows-x64")&&!validUpdatePlatform("ios")&&!validUpdatePlatform(""),"only the two ordinary-client platforms have feeds");
        expect(updateFilename("1.3.3","android")=="Tinglan-Android-1.3.3.apk"&&updateFilename("5.7.3")=="Tinglan-5.7.3-Client-Setup.exe","package filenames per platform");
        expect(updateManifestName("android")=="android-stable.json"&&updateManifestName("windows-x64")=="client-stable.json","separate manifests per platform");
        QTemporaryDir dir;expect(dir.isValid(),"temporary updates folder");const QDir root(dir.path());
        QByteArray apk("PK");apk+=QByteArray(4096,'a');QByteArray exe("MZ");exe+=QByteArray(4096,'b');
        const auto android=manifest("android","1.3.3",apk),windows=manifest("windows-x64","5.7.3",exe);
        expect(validUpdateInfo(android,"android")&&!validUpdateInfo(android)&&validUpdateInfo(windows)&&!validUpdateInfo(windows,"android"),"a manifest is valid only for its own platform");

        QFile none;expect(openPublishedUpdate(root.path(),none,"android").isEmpty(),"nothing published yet");
        write(root.filePath("android-stable.json"),QJsonDocument(android).toJson());write(root.filePath("Tinglan-Android-1.3.3.apk"),apk);
        QFile file;auto info=openPublishedUpdate(root.path(),file,"android");
        expect(info==android&&file.isOpen()&&file.readAll()==apk,"published APK is served with its manifest");file.close();
        QFile winFile;expect(openPublishedUpdate(root.path(),winFile).isEmpty(),"the Windows feed does not serve the APK");
        write(root.filePath("client-stable.json"),QJsonDocument(windows).toJson());write(root.filePath("Tinglan-5.7.3-Client-Setup.exe"),exe);
        expect(openPublishedUpdate(root.path(),winFile)==windows&&winFile.readAll()==exe,"Windows feed is unchanged alongside Android");winFile.close();
        QFile other;expect(openPublishedUpdate(root.path(),other,"ios").isEmpty(),"unknown platform is refused");

        write(root.filePath("android-stable.json"),QJsonDocument(windows).toJson());
        QFile swapped;expect(openPublishedUpdate(root.path(),swapped,"android").isEmpty(),"Windows manifest in the Android feed is refused");
        auto traversal=android;traversal["version"]="../1.3.3";write(root.filePath("android-stable.json"),QJsonDocument(traversal).toJson());
        QFile escaped;expect(openPublishedUpdate(root.path(),escaped,"android").isEmpty(),"version cannot name a path");
        auto resized=android;resized["size"]=double(apk.size()+1);write(root.filePath("android-stable.json"),QJsonDocument(resized).toJson());
        QFile wrongSize;expect(openPublishedUpdate(root.path(),wrongSize,"android").isEmpty(),"size must match the file on disk");
        std::printf("PASS %d update store checks\n",checks);return 0;
    }catch(const std::exception& e){std::printf("%s\n",e.what());return 1;}
}
