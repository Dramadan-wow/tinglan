#pragma once
#include <QJsonObject>
namespace im::net {
// Connection codes carry public trust material only, never a password or private key.
class ConnectionCodeHsx final {
public:
    static QJsonObject validate(const QJsonObject& value);
    static QString encode(const QJsonObject& value);
    static QJsonObject decode(const QString& text);
    static QString fingerprint(const QString& pem);
};
}
