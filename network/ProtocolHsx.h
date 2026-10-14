#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>

namespace im::net {
constexpr quint32 MaxFrame = 64 * 1024;
constexpr int MaxText = 16 * 1024;
constexpr qint64 MaxQueue = 512 * 1024;
QString newId();
bool validId(const QString& id);
bool validUser(const QString& user);
QJsonObject envelope(const QString& type, const QJsonObject& data = {},
                     const QString& sender = {}, const QString& recipient = {}, const QString& id = {});
QByteArray encode(const QJsonObject& object);
// Incremental decoder: a bad frame invalidates the connection; callers must close it.
class DecoderHsx {
public:
    bool feed(const QByteArray& bytes, QList<QJsonObject>& messages, QString& error);
    void clear() { buffer_.clear(); }
private:
    QByteArray buffer_;
};
}
