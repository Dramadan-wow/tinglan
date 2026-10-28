#include "UpdateInfoHsx.h"
#include <QRegularExpression>
#include <QVersionNumber>
namespace im::net {
bool validReleaseVersion(const QString& v) {
    static const QRegularExpression re("^(0|[1-9][0-9]{0,3})\\.(0|[1-9][0-9]{0,3})\\.(0|[1-9][0-9]{0,3})$");
    return re.match(v).hasMatch();
}
bool newerRelease(const QString& candidate,const QString& current) {
    return validReleaseVersion(candidate)&&validReleaseVersion(current)&&
        QVersionNumber::compare(QVersionNumber::fromString(candidate),QVersionNumber::fromString(current))>0;
}
bool validUpdatePlatform(const QString& p){return p=="windows-x64"||p=="android";}
QString updateFilename(const QString& v,const QString& platform){return platform=="android"?"Tinglan-Android-"+v+".apk":"Tinglan-"+v+"-Client-Setup.exe";}
QString updateManifestName(const QString& platform){return platform=="android"?"android-stable.json":"client-stable.json";}
bool validUpdateInfo(const QJsonObject& o,const QString& platform) {
    static const QRegularExpression hash("^[0-9a-f]{64}$");
    const double size=o["size"].toDouble(-1);
    return validUpdatePlatform(platform)&&o["schema"]==1&&o["edition"]=="client"&&o["platform"]==platform&&
        validReleaseVersion(o["version"].toString())&&size>=1024&&size<=MaxUpdateSize&&size==qint64(size)&&
        hash.match(o["sha256"].toString()).hasMatch()&&o["notes"].isString()&&o["notes"].toString().size()<=4000;
}
}
