#pragma once
#include <QJsonObject>
#include <QString>
namespace im::net {
// This store belongs only to the server. No client/local-coursework credentials are imported.
class AuthStoreHsx {
public:
    explicit AuthStoreHsx(const QString& path);
    void addUser(const QString& username,const QString& password);
    bool contains(const QString& username) const { return users_.contains(username); }
    QJsonObject record(const QString& username) const;
    static bool verify(const QJsonObject& record,const QString& password);
private:
    QString path_;
    QJsonObject users_,dummy_;
};
}
