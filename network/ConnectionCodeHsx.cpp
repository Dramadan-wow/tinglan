#include "ConnectionCodeHsx.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QHostAddress>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSslCertificate>
#include <stdexcept>
namespace im::net {
namespace {
void ensure(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
const auto options=QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals;
}
QJsonObject ConnectionCodeHsx::validate(const QJsonObject& v){
    for(auto it=v.begin();it!=v.end();++it)ensure(QStringList{"version","host","port","ca","mode"}.contains(it.key()),"连接码包含不支持的字段");
    ensure(v["version"].toInt()==1&&v["host"].isString()&&v["ca"].isString(),"连接码格式或版本无效");
    const auto host=v["host"].toString();QHostAddress address;
    if(address.setAddress(host))ensure(address.protocol()==QAbstractSocket::IPv4Protocol&&!address.isNull()&&host!="0.0.0.0"&&!address.isMulticast(),"连接码中的 IP 地址无效");
    else ensure(QRegularExpression("^(?=.{1,253}$)(?:[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?\\.)*[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?$").match(host).hasMatch()&&!QRegularExpression("^[0-9.]+$").match(host).hasMatch(),"连接码中的域名无效");
    const auto port=v["port"].toInt();ensure(port>0&&port<=65535&&v["port"].toDouble()==port,"连接码中的端口无效");
    ensure(v["mode"]=="lan"||v["mode"]=="cloud","连接码中的模式无效");
    const auto pem=v["ca"].toString().toUtf8();ensure(pem.size()<=8192&&!pem.contains("PRIVATE KEY")&&!pem.contains("PUBLIC KEY"),"连接码不得包含私钥");
    if(!pem.isEmpty()){
        ensure(pem.trimmed().startsWith("-----BEGIN CERTIFICATE-----")&&pem.trimmed().endsWith("-----END CERTIFICATE-----")&&pem.count("-----BEGIN ")==1&&pem.count("-----END ")==1,"连接码必须包含单个公共 CA 证书");
        const auto certificates=QSslCertificate::fromData(pem,QSsl::Pem);ensure(certificates.size()==1&&!certificates.first().isNull(),"连接码中的 CA 证书无效");
        const auto now=QDateTime::currentDateTimeUtc();ensure(certificates.first().effectiveDate()<=now&&certificates.first().expiryDate()>now,"连接码中的 CA 证书未生效或已过期");
    }return v;
}
QString ConnectionCodeHsx::fingerprint(const QString& pem){
    if(pem.isEmpty())return "系统信任证书";const auto certs=QSslCertificate::fromData(pem.toUtf8(),QSsl::Pem);
    ensure(certs.size()==1,"CA 证书无效");return QString::fromLatin1(certs.first().digest(QCryptographicHash::Sha256).toHex());
}
QString ConnectionCodeHsx::encode(const QJsonObject& value){
    const auto bytes=QJsonDocument(validate(value)).toJson(QJsonDocument::Compact);
    return "TINGLAN1."+QString::fromLatin1(bytes.toBase64(options))+"."+QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex().left(16));
}
QJsonObject ConnectionCodeHsx::decode(const QString& text){
    const auto trimmed=text.trimmed();ensure(trimmed.size()<=16384,"连接码超过长度限制");const auto parts=trimmed.split('.');
    ensure(parts.size()==3&&parts.first()=="TINGLAN1","请粘贴以 TINGLAN1. 开头的完整连接码");
    const auto encoded=parts[1].toLatin1();const auto bytes=QByteArray::fromBase64(encoded,QByteArray::Base64UrlEncoding);
    ensure(bytes.toBase64(options)==encoded&&QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex().left(16))==parts[2],"连接码不完整或已被改变");
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(bytes,&error);ensure(error.error==QJsonParseError::NoError&&doc.isObject(),"连接码内容无效");return validate(doc.object());
}
}
