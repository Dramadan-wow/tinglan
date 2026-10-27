#pragma once
#include <utility>
#include "AccountManagerHsx.h"
#include "AdminManagerHsx.h"
#include "FriendManagerHsx.h"
#include "GroupManagerHsx.h"
#include "MessageManagerHsx.h"
#include "StudioStoreHsx.h"
#include <QObject>
#include "ChatClientHsx.h"
#include "FileTransferManagerHsx.h"
#include <QJsonArray>
#include <QSet>
#include <QString>
#include <functional>

namespace im {
class AppSessionHsx final : public QObject {
    Q_OBJECT
public:
    explicit AppSessionHsx(std::filesystem::path file, QObject* parent = nullptr);
    DataStoreHsx& data() { return data_; }
    const DataStoreHsx& data() const { return data_; }
    AccountManagerHsx& accounts() { return accounts_; }
    AdminManagerHsx& admins() { return admins_; }
    FriendManagerHsx& friends() { return friends_; }
    GroupManagerHsx& groups() { return groups_; }
    MessageManagerHsx& messages() { return messages_; }
    StudioStoreHsx& studio() { return studio_; }
    const StudioStoreHsx& studio() const { return studio_; }
    int user() const { return user_; }
    ServiceHsx service() const { return service_; }
    const std::filesystem::path& file() const { return file_; }
    bool signedIn() const { return user_ != 0 && data_.user(user_).online() && data_.user(user_).loginAllowed(); }
    bool adminSignedIn() const { return remoteConfigured_?remoteAdmin_:admins_.active(); }
    bool login(int user, ServiceHsx type, const QString& password);
    bool loginNickname(const QString& nickname, ServiceHsx type, const QString& password);
    bool loginAdmin(const QString& name, const QString& password);
    void logout();
    bool switchService(ServiceHsx type);
    bool mutate(const std::function<void()>& action, const QString& success = {});
    bool save();
    void configureRemote(const QString& file);
    bool remote() const {return remoteConfigured_;}
    bool connected() const {return remoteClient_.authenticated();}
    void loginRemote(const QString& nickname,ServiceHsx service,const QString& password,bool registration=false);
    void loginRemoteAdmin(const QString& name,const QString& password);
    // Sign in by scanning a QR code with the phone app; qrTicket/qrStatus report progress.
    void loginRemoteQr(ServiceHsx service);
    // Why the last remote session ended (replaced by another device, ended by the administrator); read once.
    QString takeEndReason(){return std::exchange(endReason_,{});}
    bool command(const QString& op,QJsonObject arguments,const std::function<void()>& local={},const QString& success={});
    const QJsonArray& feed()const{return feed_;}
    const QJsonObject& social()const{return social_;}
    bool personalize(const std::function<void()>& action,const QString& success={});
    QString lastNickname()const{return lastNickname_;}
    QString connectionStatus()const{return connectionStatus_;}
    net::ChatClientHsx* remoteClient(){return &remoteClient_;}
    net::FileTransferManagerHsx& files(){return files_;}
    void markDisplayed(const std::vector<long long>& messages);
    int unreadCount() const;
    int requestCount() const;
    static QString q(const std::string& value);
    static std::string s(const QString& value);
    static std::filesystem::path path(const QString& value);
signals:
    void changed();
    void countersChanged();
    void failed(const QString& message);
    void succeeded(const QString& message);
    void sessionEnded();
    void remoteLoginFinished(bool success);
    void operationRejected(const QJsonObject& request);
    void connectionChanged(const QString& status);
    void qrTicket(const QString& text,int seconds);
    void qrStatus(const QString& state);
private:
    DataStoreHsx data_;
    std::filesystem::path file_;
    AccountManagerHsx accounts_;
    AdminManagerHsx admins_;
    FriendManagerHsx friends_;
    GroupManagerHsx groups_;
    MessageManagerHsx messages_;
    StudioStoreHsx studio_;
    net::FileTransferManagerHsx files_;
    // Destroy the chat client first: its disconnect signal suspends live file tasks.
    net::ChatClientHsx remoteClient_;
    net::ClientOptionsHsx remoteOptions_;
    QHash<QString,QJsonObject> pendingCommands_;
    QHash<QString,QString> pendingSuccess_;
    QSet<long long> readPending_;
    QJsonArray feed_;
    QJsonObject social_;
    QString lastNickname_,connectionStatus_="未连接",endReason_;
    bool legacyAppearanceAttempted_=false;
    bool remoteConfigured_=false,remoteLoginPending_=false,remoteAdmin_=false;
    int user_ = 0;
    ServiceHsx service_ = ServiceHsx::Count;
};
}
