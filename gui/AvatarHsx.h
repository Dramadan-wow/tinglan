#pragma once
#include <QLabel>
#include <QJsonObject>
#include <QPixmap>
namespace im {
class StudioStoreHsx;
class AvatarHsx final : public QLabel {
public:
    AvatarHsx(const StudioStoreHsx& store,int user,const QString& name,int size=42,QWidget* parent=nullptr);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QPixmap image_;
    QString name_,color_;
    QJsonObject frame_;
};
}
