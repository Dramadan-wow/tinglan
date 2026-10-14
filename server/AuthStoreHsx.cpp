#include "AuthStoreHsx.h"
#include "ProtocolHsx.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QSaveFile>
#include <cstring>
#include <stdexcept>
namespace im::net {
namespace {
constexpr int Iterations=600000;
void require(bool ok,const char* error){if(!ok)throw std::runtime_error(error);}
QByteArray randomBytes(){QByteArray result(32,Qt::Uninitialized);for(int i=0;i<32;i+=4){const quint32 n=QRandomGenerator::system()->generate();memcpy(result.data()+i,&n,4);}return result;}
QByteArray derive(const QString& password,const QByteArray& salt){return QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256,password.toUtf8(),salt,Iterations,32);}
}
AuthStoreHsx::AuthStoreHsx(const QString& path):path_(path){
    dummy_={{"algorithm","pbkdf2-sha256"},{"iterations",Iterations},{"salt",QString::fromLatin1(randomBytes().toHex())},{"hash",QString::fromLatin1(randomBytes().toHex())}};
    QFile file(path);if(!file.exists())return;
    require(file.open(QIODevice::ReadOnly),"Cannot open credentials store");
    require(file.size()<=16*1024*1024,"Credentials store too large");
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(file.readAll(),&error);
    require(error.error==QJsonParseError::NoError&&doc.isObject(),"Invalid credentials JSON");
    const auto root=doc.object();require(root.value("version")==QJsonValue(1)&&root.value("users").isObject(),"Unsupported credentials format");
    users_=root.value("users").toObject();
    for(auto it=users_.begin();it!=users_.end();++it){const auto r=it.value().toObject();const auto salt=r.value("salt").toString().toLatin1(),hash=r.value("hash").toString().toLatin1();
        require(validUser(it.key())&&r.value("algorithm")=="pbkdf2-sha256"&&r.value("iterations")==QJsonValue(Iterations)&&
            salt.size()==64&&hash.size()==64&&QByteArray::fromHex(salt).toHex()==salt&&QByteArray::fromHex(hash).toHex()==hash,"Invalid credential record");}
}
QJsonObject AuthStoreHsx::record(const QString& username)const{return users_.contains(username)?users_.value(username).toObject():dummy_;}
bool AuthStoreHsx::verify(const QJsonObject& record,const QString& password){
    const auto actual=derive(password,QByteArray::fromHex(record.value("salt").toString().toLatin1()));
    const auto expected=QByteArray::fromHex(record.value("hash").toString().toLatin1());
    if(actual.size()!=32||expected.size()!=32)return false;
    unsigned difference=0;for(int i=0;i<32;++i)difference|=static_cast<unsigned char>(actual[i])^static_cast<unsigned char>(expected[i]);return difference==0;
}
void AuthStoreHsx::addUser(const QString& username,const QString& password){
    require(validUser(username),"Username must match [a-z][a-z0-9_]{2,31}");
    require(!users_.contains(username),"Username already exists");
    require(password.toUtf8().size()>=12&&password.toUtf8().size()<=128,"Password must be 12-128 UTF-8 bytes");
    const auto salt=randomBytes(),hash=derive(password,salt);require(hash.size()==32,"Password derivation failed");
    auto next=users_;next[username]=QJsonObject{{"algorithm","pbkdf2-sha256"},{"iterations",Iterations},{"salt",QString::fromLatin1(salt.toHex())},{"hash",QString::fromLatin1(hash.toHex())}};
    require(QDir().mkpath(QFileInfo(path_).absolutePath()),"Cannot create credentials directory");
    QSaveFile file(path_);require(file.open(QIODevice::WriteOnly),"Cannot write credentials");
    require(file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner),"Cannot restrict credentials permissions");
    const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"users",next}}).toJson();
    require(file.write(bytes)==bytes.size()&&file.commit(),"Cannot commit credentials");users_=next;
}
}
