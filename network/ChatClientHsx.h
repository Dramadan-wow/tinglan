#pragma once
#include "ProtocolHsx.h"
#include <QObject>
#include <QElapsedTimer>
#include <QHash>
#include <QTimer>
class QTcpSocket;
namespace im::net {
struct ClientOptionsHsx {
    QString host="127.0.0.1";
    quint16 port=56000;
    QString username, password, caFile;
    bool workspace=false,registerAccount=false,administrator=false;
    int service=0;
    bool tls=true;
    bool allowInsecureLoopback=false;
};
class ChatClientHsx final : public QObject {
    Q_OBJECT
public:
    explicit ChatClientHsx(QObject* parent=nullptr);
    ~ChatClientHsx() override;
    void start(const ClientOptionsHsx& options);
    void stop();
    bool authenticated() const { return authenticated_; }
    QString user() const { return user_; }
    int loginService() const {return options_.service;}
    void updatePassword(const QString& password){options_.password=password;}
    void updateNickname(const QString& nickname){options_.username=nickname;}
    QString workspaceCommand(const QJsonObject& data);
    QString sendChat(const QString& recipient,const QString& text,const QString& id={});
signals:
    void stateChanged(const QString& state);
    void authenticatedChanged(bool online);
    void chatReceived(const QString& sender,const QString& text,const QString& id);
    void messageResult(const QString& id,bool accepted,const QString& detail);
    void workspaceSnapshot(const QJsonObject& data);
    void workspaceResult(const QString& id,const QJsonObject& data);
    void problem(const QString& detail);
    void sessionRevoked();
private:
    void open();
    void login();
    void read();
    void handle(const QJsonObject& message);
    void lost(const QString& reason,bool retry=true);
    bool write(const QJsonObject& message);
    void tick();
    ClientOptionsHsx options_;
    QTcpSocket* socket_=nullptr;
    DecoderHsx decoder_;
    QTimer retry_,timer_;
    QElapsedTimer clock_;
    QHash<QString,qint64> pending_,received_;
    qint64 lastReceived_=0,started_=0,lastHeartbeat_=0;
    QString user_,loginId_,snapshotId_;
    QByteArray snapshotBytes_;
    int snapshotPart_=0,snapshotCount_=0;
    QHash<QString,qint64> commands_;
    bool active_=false,authenticated_=false;
    int retryMs_=1000;
};
}
