#include "UpdateStoreHsx.h"
#include "UpdateInfoHsx.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
namespace im::net {
QJsonObject openPublishedUpdate(const QString& directory,QFile& installer){
    if(directory.isEmpty())return {};
    QDir root(directory);QFile manifest(root.filePath("client-stable.json"));
    if(!manifest.open(QIODevice::ReadOnly)||manifest.size()>16384)return {};
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(manifest.readAll(),&error);
    if(error.error!=QJsonParseError::NoError||!doc.isObject()||!validUpdateInfo(doc.object()))return {};
    const auto info=doc.object();const QFileInfo file(root.filePath(updateFilename(info["version"].toString())));
    if(file.isSymLink()||file.canonicalPath()!=root.canonicalPath()||file.size()!=qint64(info["size"].toDouble()))return {};
    installer.setFileName(file.absoluteFilePath());if(!installer.open(QIODevice::ReadOnly))return {};
    return info;
}
}
