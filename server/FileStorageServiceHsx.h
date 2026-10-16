#pragma once
#include <QObject>
#include <QHostAddress>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSslConfiguration>
#include <QThreadPool>
#include <QTimer>
#include <QLockFile>
#include <memory>
class QTcpServer;
namespace im::net {
class FileHttpConnectionHsx;
struct FileStorageOptionsHsx {
    QString directory,certificate,key;
    QHostAddress address=QHostAddress::LocalHost;
    quint16 port=56111;
    qint64 maxFileBytes=2LL*1024*1024*1024,quotaBytes=20LL*1024*1024*1024,userQuotaBytes=5LL*1024*1024*1024;
    int maxConnections=32,tempLifetimeSeconds=7*86400;
};
// Lives entirely on a dedicated thread, including its SQLite connection and HTTPS sockets.
class FileStorageServiceHsx final:public QObject {
    Q_OBJECT
public:
    explicit FileStorageServiceHsx(FileStorageOptionsHsx options);
    ~FileStorageServiceHsx() override;
    bool start(QString& error);
    void stop();
    void grant(const QString& token,const QJsonObject& permissions);
    void revoke(const QString& token);
    void handle(FileHttpConnectionHsx* connection,const QByteArray& method,const QString& path,
                const QMap<QByteArray,QByteArray>& headers,const QByteArray& body);
signals:
    void fileReady(int sender,int recipient,const QString& fileId);
private:
    QJsonObject record(const QString& id,bool fileId=false);
    QJsonObject publicRecord(const QJsonObject& row)const;
    void cleanup();
    FileStorageOptionsHsx options_;
    QTcpServer* listener_=nullptr;
    QSslConfiguration ssl_;
    QSqlDatabase db_;
    QString dbName_;
    QHash<QString,QJsonObject> grants_;
    QThreadPool hashes_;
    QTimer* cleanup_=nullptr;
    std::unique_ptr<QLockFile> lock_;
    bool stopping_=false;
};
}
