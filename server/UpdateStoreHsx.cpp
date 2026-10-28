#include "UpdateStoreHsx.h"
#include "UpdateInfoHsx.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
namespace im::net {
QJsonObject openPublishedUpdate(const QString& directory,QFile& installer,const QString& platform){
    if(directory.isEmpty()||!validUpdatePlatform(platform))return {};
    QDir root(directory);QFile manifest(root.filePath(updateManifestName(platform)));
    if(!manifest.open(QIODevice::ReadOnly)||manifest.size()>16384)return {};
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(manifest.readAll(),&error);
    if(error.error!=QJsonParseError::NoError||!doc.isObject()||!validUpdateInfo(doc.object(),platform))return {};
    const auto info=doc.object();const QFileInfo file(root.filePath(updateFilename(info["version"].toString(),platform)));
    if(file.isSymLink()||file.canonicalPath()!=root.canonicalPath()||file.size()!=qint64(info["size"].toDouble()))return {};
    installer.setFileName(file.absoluteFilePath());if(!installer.open(QIODevice::ReadOnly))return {};
    return info;
}
}
