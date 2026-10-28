#pragma once
#include <QJsonObject>
#include <QString>
namespace im::net {
constexpr qint64 MaxUpdateSize = 256LL * 1024 * 1024;
constexpr int UpdateChunkSize = 32768;
bool validReleaseVersion(const QString& version);
bool newerRelease(const QString& candidate, const QString& current);
// Ordinary-client platforms with a published feed: the Windows installer and the Android APK.
bool validUpdatePlatform(const QString& platform);
QString updateFilename(const QString& version, const QString& platform = "windows-x64");
QString updateManifestName(const QString& platform);
bool validUpdateInfo(const QJsonObject& info, const QString& platform = "windows-x64");
}
