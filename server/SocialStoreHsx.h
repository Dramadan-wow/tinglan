#pragma once
#include "DataStoreHsx.h"
#include <QSqlDatabase>
#include <QJsonObject>
#include <QJsonArray>
namespace im {
class SocialStoreHsx {
public:
    SocialStoreHsx(QSqlDatabase db,const DataStoreHsx& data):db_(db),data_(data){}
    void migrate();
    QJsonObject snapshot(int actor) const;
    bool command(int actor,const QJsonObject& args,QJsonObject& result);
    QJsonArray orders(int actor=0) const;
    void review(const QString& id,bool approve,const QString& note,bool ownTransaction=true);
private:
    bool hasWeibo(int actor) const;
    bool friends(int a,int b) const;
    bool blocked(int a,int b) const;
    bool visible(int actor,int author,const QString& channel) const;
    QJsonArray posts(int actor,const QString& channel) const;
    QJsonObject member(int actor) const;
    QSqlDatabase db_;
    const DataStoreHsx& data_;
};
}
