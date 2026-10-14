#include "ProtocolHsx.h"
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStringDecoder>
#include <QUuid>
#include <QtEndian>
#include <stdexcept>

namespace im::net {
QString newId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
bool validId(const QString& id) {
    static const QRegularExpression pattern("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");
    return pattern.match(id).hasMatch();
}
bool validUser(const QString& user) {
    static const QRegularExpression pattern("^[a-z][a-z0-9_]{2,31}$");
    return pattern.match(user).hasMatch();
}
QJsonObject envelope(const QString& type, const QJsonObject& data, const QString& sender,
                     const QString& recipient, const QString& id) {
    return {{"v",1},{"type",type},{"id",id.isEmpty()?newId():id},
            {"senderId",sender},{"recipientId",recipient},{"data",data}};
}
QByteArray encode(const QJsonObject& object) {
    const auto body = QJsonDocument(object).toJson(QJsonDocument::Compact);
    if (body.isEmpty() || body.size() > MaxFrame) throw std::runtime_error("消息帧超过 64 KiB");
    QByteArray frame(4,Qt::Uninitialized);
    qToBigEndian<quint32>(quint32(body.size()),frame.data());
    return frame + body;
}
bool DecoderHsx::feed(const QByteArray& bytes, QList<QJsonObject>& messages, QString& error) {
    // Callers read at most 64 KiB per iteration. Bound both partial and coalesced input.
    if (buffer_.size()+bytes.size() > 2*MaxFrame+4) { error="receive_buffer_limit"; return false; }
    buffer_ += bytes;
    while (buffer_.size() >= 4) {
        const auto length=qFromBigEndian<quint32>(buffer_.constData());
        if (!length || length>MaxFrame) { error="invalid_frame_length"; return false; }
        if (buffer_.size()<qsizetype(length+4)) break;
        const auto body=buffer_.mid(4,length); buffer_.remove(0,length+4);
        QStringDecoder utf8(QStringDecoder::Utf8); const QString decoded=utf8(body);
        if (utf8.hasError()) { error="invalid_utf8"; return false; }
        QJsonParseError parse; const auto doc=QJsonDocument::fromJson(body,&parse);
        if (parse.error!=QJsonParseError::NoError || !doc.isObject()) { error="invalid_json"; return false; }
        const auto obj=doc.object();
        if (obj.value("v")!=QJsonValue(1) || !validId(obj.value("id").toString()) ||
            !obj.value("type").isString() || obj.value("type").toString().size()>32 ||
            obj.value("type").toString().isEmpty() || !obj.value("senderId").isString() ||
            !obj.value("recipientId").isString() || !obj.value("data").isObject() ||
            obj.value("senderId").toString().size()>32 || obj.value("recipientId").toString().size()>32) {
            error="invalid_envelope"; return false;
        }
        messages.append(obj);
    }
    return true;
}
}
