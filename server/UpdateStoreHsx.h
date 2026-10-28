#pragma once
#include <QFile>
#include <QJsonObject>
namespace im::net {
// Public ordinary-client binaries only; no arbitrary path/URL or admin package access.
QJsonObject openPublishedUpdate(const QString& directory, QFile& installer, const QString& platform = "windows-x64");
}
