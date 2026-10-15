#pragma once
#include "AuthStoreHsx.h"
#include "ProtocolHsx.h"
#include <QTcpServer>
#include <QElapsedTimer>
#include <QHash>
#include <QPointer>
#include <QSslConfiguration>
#include <QThreadPool>
#include <QTimer>
#include <memory>
#include <QFile>
class QTcpSocket;
namespace im { class WorkspaceBackendHsx; }
namespace im::net {
struct ServerOptionsHsx {
    QHostAddress address=QHostAddress::LocalHost;
    quint16 port=56000;
    bool tls=true,allowInsecureLoopback=false;
    QString certificateFile,keyFile,updatesDirectory;
    int maxConnections=128,idleTimeoutMs=60000,loginTimeoutMs=30000;
};
class ChatServerHsx final : public QTcpServer {
    Q_OBJECT
public:
    ChatServerHsx(AuthStoreHsx& auth,const ServerOptionsHsx& options,QObject* parent=nullptr);
    ~ChatServerHsx() override;
    void setWorkspace(im::WorkspaceBackendHsx* backend){workspace_=backend;}
    bool start(QString& error);
    void shutdown();
signals:
    void logLine(const QString& line);
protected:
    void incomingConnection(qintptr descriptor) override;
private:
    struct Peer {
        QPointer<QTcpSocket> socket;
        DecoderHsx decoder;
        QString user;
        qint64 created=0,lastActive=0,window=0;
        int count=0;
        bool authenticating=false,closing=false,syncing=false,resync=false,administrator=false;
        int workspaceUser=0,service=0;
        bool updater=false;
        QFile updateFile;
        QJsonObject updateInfo;
        qint64 updateOffset=0;
    };
    struct Seen { QByteArray digest; qint64 time; };
    void read(QTcpSocket* socket);
    void handle(const std::shared_ptr<Peer>& peer,const QJsonObject& message);
    bool send(const std::shared_ptr<Peer>& peer,const QJsonObject& message);
    void error(const std::shared_ptr<Peer>& peer,const QString& code,const QString& request={},bool close=false);
    void login(const std::shared_ptr<Peer>& peer,const QJsonObject& message);
    void chat(const std::shared_ptr<Peer>& peer,const QJsonObject& message);
    void update(const std::shared_ptr<Peer>& peer,const QJsonObject& message);
    void tick();
    void workspaceLogin(const std::shared_ptr<Peer>&,const QJsonObject&);
    void synchronize(const std::shared_ptr<Peer>&);
    void sendSnapshot(const std::shared_ptr<Peer>&,const QByteArray&,const QString&,int);
    im::WorkspaceBackendHsx* workspace_=nullptr;
    AuthStoreHsx& auth_;
    ServerOptionsHsx options_;
    QSslConfiguration ssl_;
    QHash<QTcpSocket*,std::shared_ptr<Peer>> peers_;
    QHash<QString,QTcpSocket*> online_;
    QHash<QString,Seen> seen_;
    QHash<QString,QPair<qint64,int>> attempts_;
    QElapsedTimer clock_;
    QThreadPool workers_;
    QTimer timer_;
    int authJobs_=0;
};
}
