#pragma once
#include "WorkspaceCodecHsx.h"
#include <QSqlDatabase>
namespace im {
class WorkspaceBackendHsx {
public:
    WorkspaceBackendHsx(const QString& file,const QString& importFile={});
    ~WorkspaceBackendHsx();
    int resolve(const QString& nickname,ServiceHsx service) const;
    std::string credential(int user,ServiceHsx service) const;
    void online(int user,bool enabled,ServiceHsx service=ServiceHsx::Count);
    std::string adminCredential(const QString& name) const;
    void setAdmin(const QString& name,const QString& password);
    int importAudit(const QString& file);
    void loginAudit(const QString& nickname,int user,ServiceHsx service,const QString& action,bool admin=false);
    QJsonObject adminSnapshot() const;
    QJsonObject snapshot(int viewer) const;
    QJsonObject command(int actor,ServiceHsx service,const QString& request,const QJsonObject& data,bool administrator=false);
    QJsonArray orders()const;
    void reviewOrder(const QString& id,bool approve,const QString& note);
    int registerUser(const QString& nickname,const std::string& passwordHash,ServiceHsx service);
    QJsonObject filePermissions(int user);
private:
    void persist();
    void recordAudit(const std::string& actor,const std::string& action,int user,ServiceHsx service);

    DataStoreHsx data_;
    QSqlDatabase db_;
    QString connection_;
};
}

