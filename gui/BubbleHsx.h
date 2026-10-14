#pragma once
#include <QLabel>
#include <QJsonObject>
class QTimer;
namespace im {
class BubbleHsx final : public QLabel {
public:
    BubbleHsx(const QString& text,const QJsonObject& style,bool mine,QWidget* parent=nullptr);
protected:
    void paintEvent(QPaintEvent*) override;
    void showEvent(QShowEvent*) override;
    void hideEvent(QHideEvent*) override;
private:
    QJsonObject style_;
    bool mine_;
    QTimer* timer_;
    double phase_=0;
};
}
