#pragma once
#include "ProtocolHsx.h"
#include <QObject>
#include <QSslSocket>
#include <QSaveFile>
#include <QCryptographicHash>
#include <QTimer>
#include <memory>
namespace im::net {
// Independent TLS connection: update checks work before login and never send passwords.
class UpdateClientHsx final:public QObject {
    Q_OBJECT
public:
    explicit UpdateClientHsx(QObject* parent=nullptr);
    void check(const QString& configFile,const QString& currentVersion);
    void download(const QString& directory);
    void cancel();
    bool busy()const{return active_;}
signals:
    void available(const QJsonObject& release);
    void current(bool published);
    void failed(const QString& detail);
    void progress(qint64 received,qint64 total);
    void ready(const QString& file);
private:
    void open();
    void request(const QString& type,const QJsonObject& data);
    void read();
    void handle(const QJsonObject& message);
    void fail(const QString& detail);
    void close();
    QSslSocket socket_;
    QTimer timeout_;
    DecoderHsx decoder_;
    QString config_,version_,requestId_,file_;
    QJsonObject release_;
    std::unique_ptr<QSaveFile> output_;
    QCryptographicHash hash_{QCryptographicHash::Sha256};
    qint64 received_=0;
    bool active_=false,downloading_=false;
};
}
