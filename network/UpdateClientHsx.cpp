#include "UpdateClientHsx.h"
#include "UpdateInfoHsx.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSslConfiguration>
#include <QSslError>
namespace im::net {
UpdateClientHsx::UpdateClientHsx(QObject* parent):QObject(parent){
    timeout_.setSingleShot(true);timeout_.setInterval(30000);socket_.setReadBufferSize(2*MaxFrame+4);
    connect(&timeout_,&QTimer::timeout,this,[this]{fail("更新连接超时，请稍后重试。");});
    connect(&socket_,&QSslSocket::encrypted,this,[this]{request("update_check",{{"edition","client"},{"platform","windows-x64"},{"version",version_}});});
    connect(&socket_,&QSslSocket::readyRead,this,&UpdateClientHsx::read);
    connect(&socket_,&QSslSocket::sslErrors,this,[this](const QList<QSslError>&){fail("服务器证书验证失败，更新已停止。");});
    connect(&socket_,&QSslSocket::errorOccurred,this,[this](QAbstractSocket::SocketError){fail("更新连接失败："+socket_.errorString());});
    connect(&socket_,&QSslSocket::disconnected,this,[this]{if(active_)fail("更新连接已中断，可以重新下载。");});
}
void UpdateClientHsx::close(){active_=false;timeout_.stop();socket_.abort();decoder_.clear();requestId_.clear();}
void UpdateClientHsx::cancel(){close();output_.reset();downloading_=false;}
void UpdateClientHsx::fail(const QString& detail){if(!active_)return;cancel();emit failed(detail);}
void UpdateClientHsx::check(const QString& config,const QString& version){
    cancel();release_={};config_=config;version_=version;open();
}
void UpdateClientHsx::open(){
    active_=true;QFile file(config_);
    if(!validReleaseVersion(version_)||!file.open(QIODevice::ReadOnly)||file.size()>65536){fail("无法读取更新连接配置。");return;}
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(file.readAll(),&error);const auto c=doc.object();
    const auto host=c["host"].toString().trimmed();const double port=c["port"].toDouble(-1);
    if(error.error!=QJsonParseError::NoError||c["tls"]!=true||host.isEmpty()||port<1||port>65535||port!=int(port)){
        fail("自动更新需要已验证的 TLS 中心服务器连接；本机明文模式不提供更新。");return;
    }
    auto ssl=QSslConfiguration::defaultConfiguration();ssl.setProtocol(QSsl::TlsV1_2OrLater);
    const auto ca=c["caFile"].toString();
    if(!ca.isEmpty()){
        QFile cert(QDir(QFileInfo(config_).absolutePath()).absoluteFilePath(ca));
        if(!cert.open(QIODevice::ReadOnly)||cert.size()>65536){fail("无法读取更新服务器 CA 证书。");return;}
        const auto certs=QSslCertificate::fromData(cert.readAll());if(certs.isEmpty()){fail("更新服务器 CA 证书无效。");return;}
        ssl.setCaCertificates(certs);
    }
    socket_.setSslConfiguration(ssl);socket_.setPeerVerifyMode(QSslSocket::VerifyPeer);
    timeout_.start();socket_.connectToHostEncrypted(host,quint16(port));
}
void UpdateClientHsx::request(const QString& type,const QJsonObject& data){
    if(!active_)return;requestId_=newId();const auto bytes=encode(envelope(type,data,{},{},requestId_));
    if(socket_.write(bytes)!=bytes.size()){fail("更新请求发送失败。");return;}timeout_.start();
}
void UpdateClientHsx::read(){
    while(active_&&socket_.bytesAvailable()){
        QList<QJsonObject> messages;QString error;
        if(!decoder_.feed(socket_.read(MaxFrame),messages,error)){fail("更新数据格式无效。");return;}
        for(const auto& m:messages){if(!active_)return;handle(m);}
    }
}
void UpdateClientHsx::download(const QString& directory){
    if(active_||!validUpdateInfo(release_)||!newerRelease(release_["version"].toString(),version_))return;
    const auto folder=QDir(directory).filePath(newId());active_=true;
    if(!QDir().mkpath(folder)){fail("无法创建更新缓存目录。");return;}
    file_=QDir(folder).filePath(updateFilename(release_["version"].toString()));
    output_=std::make_unique<QSaveFile>(file_);output_->setDirectWriteFallback(false);
    if(!output_->open(QIODevice::WriteOnly)){fail("无法写入更新缓存，请检查磁盘空间。");return;}
    received_=0;hash_.reset();downloading_=true;open();
}
void UpdateClientHsx::handle(const QJsonObject& m){
    const auto d=m["data"].toObject();const auto type=m["type"].toString();
    if(requestId_.isEmpty()||d["requestId"]!=requestId_||!m["senderId"].toString().isEmpty()||!m["recipientId"].toString().isEmpty()){
        fail("更新响应不匹配。");return;
    }
    requestId_.clear();
    if(type=="error"){fail("服务器暂时无法提供更新："+d["code"].toString());return;}
    if(type=="update_info"){
        if(!d["available"].isBool()||!d["published"].isBool()){fail("更新清单无效。");return;}
        if(!d["available"].toBool()){
            if(downloading_){fail("发布版本已变化，请重新检查更新。");return;}
            close();emit current(d["published"].toBool());return;
        }
        const auto release=d["release"].toObject();
        if(!validUpdateInfo(release)||!newerRelease(release["version"].toString(),version_)){fail("更新版本或安装包信息无效。");return;}
        if(!downloading_){release_=release;close();emit available(release_);return;}
        if(release!=release_){fail("发布版本已变化，请重新检查更新。");return;}
        request("update_chunk",{{"sha256",release_["sha256"]},{"offset",0}});return;
    }
    if(type!="update_chunk"||!downloading_||!output_||d["offset"].toDouble(-1)!=received_||!d["payload"].isString()){
        fail("更新分片顺序无效。");return;
    }
    const auto encoded=d["payload"].toString().toLatin1();
    const auto result=QByteArray::fromBase64Encoding(encoded,QByteArray::AbortOnBase64DecodingErrors);
    const auto bytes=result.decoded;const auto size=qint64(release_["size"].toDouble());
    if(!result||bytes.isEmpty()||bytes.size()>UpdateChunkSize||received_+bytes.size()>size||
       (received_==0&&!bytes.startsWith("MZ"))){fail("安装包内容无效。");return;}
    if(output_->write(bytes)!=bytes.size()){fail("更新写入失败，请检查磁盘空间。");return;}
    hash_.addData(bytes);received_+=bytes.size();emit progress(received_,size);
    if(!active_||!output_)return; // A progress observer may cancel the download.
    if(received_==size){
        if(QString::fromLatin1(hash_.result().toHex())!=release_["sha256"].toString()){fail("安装包 SHA-256 校验失败，已丢弃下载。");return;}
        if(!output_->commit()){fail("无法完成安装包保存。");return;}
        output_.reset();downloading_=false;close();emit ready(file_);return;
    }
    // At most 50 requests/sec, below the shared server's 100/sec limit.
    QTimer::singleShot(22,this,[this]{if(active_&&downloading_)request("update_chunk",{{"sha256",release_["sha256"]},{"offset",double(received_)}});});
}
}
