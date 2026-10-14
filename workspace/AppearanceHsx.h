#pragma once
#include <QJsonArray>
#include <QJsonObject>
namespace im {
// Shared, platform-independent catalogue; the server validates these identifiers.
class AppearanceHsx final {
public:
    static QJsonArray catalog(const QString& kind);
    static QJsonObject item(const QString& kind,const QString& id);
};
}
