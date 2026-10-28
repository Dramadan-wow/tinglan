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
class FileStorageServiceHsx;
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
    void setFileService(FileStorageServiceHsx* service,quint16 port);
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
        QString fileToken;
        qint64 updateOffset=0;
        // "desktop" or "mobile": one connection per account and device class. Clients from before 5.7
        // send no device field and are told "session_revoked", which they already treat as final.
        QString device,onlineKey,qrCode,qrRequest;
        bool legacyClient=true,wantsFiles=false;
        qint64 qrUntil=0;
        qint64 searchWindow=0;
        int searches=0;
    };
    struct Ticket { std::weak_ptr<Peer> peer; int service=0; qint64 expires=0; int scannedBy=0; };
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
    void tokenLogin(const std::shared_ptr<Peer>&,const QJsonObject&);
    void admit(const std::shared_ptr<Peer>&,int user,int service,const QString& request,QJsonObject result);
    bool throttle(const std::shared_ptr<Peer>&,const QString& request);
    void qrStart(const std::shared_ptr<Peer>&,const QJsonObject&);
    void qrAction(const std::shared_ptr<Peer>&,const QJsonObject&);
    void synchronize(const std::shared_ptr<Peer>&);
    void sendSnapshot(const std::shared_ptr<Peer>&,const QByteArray&,const QString&,int);
    im::WorkspaceBackendHsx* workspace_=nullptr;
    AuthStoreHsx& auth_;
    ServerOptionsHsx options_;
    QSslConfiguration ssl_;
    QHash<QTcpSocket*,std::shared_ptr<Peer>> peers_;
    QHash<QString,QTcpSocket*> online_;
    QHash<QString,Ticket> tickets_;
    QHash<QString,Seen> seen_;
    QHash<QString,QPair<qint64,int>> attempts_;
    QElapsedTimer clock_;
    QThreadPool workers_;
    QTimer timer_;
    FileStorageServiceHsx* files_=nullptr;
    quint16 filePort_=0;
    void refreshFileGrants();
    int authJobs_=0;
};
}
