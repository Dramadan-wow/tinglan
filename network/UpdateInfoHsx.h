#pragma once
#include <QJsonObject>
#include <QString>
namespace im::net {
constexpr qint64 MaxUpdateSize = 256LL * 1024 * 1024;
constexpr int UpdateChunkSize = 32768;
bool validReleaseVersion(const QString& version);
bool newerRelease(const QString& candidate, const QString& current);
QString updateFilename(const QString& version);
bool validUpdateInfo(const QJsonObject& info);
}
