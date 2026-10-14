#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
namespace im {
class AppSessionHsx;
class StudioStoreHsx final {
public:
    StudioStoreHsx(AppSessionHsx& session, const QString& file);
    static QJsonArray catalog(const QString& kind);
    static QJsonObject plan(int tier);
    static QJsonObject item(const QString& kind, const QString& id);
    QJsonObject profile(int user) const;
    QJsonObject membership(int user) const;
    QString choice(int user, const QString& kind) const;
    QString media(int user, const QString& kind) const;
    QString membershipText(int user) const;
    void setChoice(const QString& kind, const QString& value);
    void importImage(const QString& kind, const QString& file);
    void clearImage(const QString& kind);
    QString submitOrder(int tier, const QString& reference);
    void cancelOrder(const QString& id);
    void decideOrder(const QString& id, bool approve, const QString& note);
    QJsonArray orders(bool all = false) const;
    QJsonObject streak(int other) const;
    const QString& file() const { return file_; }
private:
    int actor() const;
    void commit(const QJsonObject& next);
    void updateProfile(const QJsonObject& value);
    AppSessionHsx& session_;
    QString file_, mediaDir_;
    QJsonObject root_;
};
}
