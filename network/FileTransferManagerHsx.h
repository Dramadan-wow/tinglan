#pragma once
#include "ChatClientHsx.h"
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QThreadPool>
#include <functional>
#include <memory>
#include <atomic>
class QNetworkReply;
namespace im::net {
// No Widgets dependency: the same authenticated transport can serve a mobile UI.
class FileTransferManagerHsx final : public QObject {
    Q_OBJECT
public:
    explicit FileTransferManagerHsx(QObject* parent=nullptr);
    ~FileTransferManagerHsx() override;
    void configure(const ClientOptionsHsx& options,const QString& cacheRoot);
    void setSession(const QJsonObject& session);
    void disconnected();
    bool available() const {return !token_.isEmpty();}
    QString upload(int service,int recipient,const QString& path);
    QString download(const QJsonObject& file,const QString& path);
    void pause(const QString& id);
    void resume(const QString& id);
    void cancel(const QString& id);
    void refresh();
    QJsonObject task(const QString& id) const;
    QJsonArray cards(int service,int peer) const;
signals:
    void filesChanged();
    void taskChanged(const QString& id);
    void problem(const QString& detail);
private:
    using Callback=std::function<void(int,const QJsonObject&,const QByteArray&,QNetworkReply*)>;
    struct Running {QPointer<QNetworkReply> reply;quint64 generation=0;std::shared_ptr<std::atomic_bool> cancel;};
    void request(const QString& id,const QByteArray& method,const QString& path,const QByteArray& body,const QMap<QByteArray,QByteArray>& headers,Callback callback);
    void run(const QString& id);
    void nextUpload(const QString& id);
    void nextDownload(const QString& id);
    void hash(const QString& id,const QString& path,std::function<void(const QString&)> callback);
    void stop(const QString& id);
    void state(const QString& id,const QString& status,const QString& error={});
    bool response(const QString& id,int code,const QJsonObject& data);
    void save();
    void load();
    QString partial(const QString& id) const;
    QNetworkAccessManager network_;
    ClientOptionsHsx options_;
    QString root_,cache_,token_;
    int user_=0,port_=0;
    quint64 epoch_=0;
    qint64 maximum_=2LL*1024*1024*1024;
    QHash<QString,QJsonObject> tasks_;
    QHash<QString,Running> running_;
    QJsonArray files_;
    QThreadPool workers_;
    QTimer poll_,retry_;
};
}
