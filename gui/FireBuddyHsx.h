#pragma once
#include <QWidget>
#include <QJsonObject>
class QTimer;
namespace im {
class FireBuddyHsx final : public QWidget {
public:
    explicit FireBuddyHsx(QWidget* parent=nullptr);
    void setState(const QJsonObject& state,const QJsonObject& skin);
protected:
    void paintEvent(QPaintEvent*) override;
    void showEvent(QShowEvent*) override;
    void hideEvent(QHideEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
private:
    QJsonObject state_,skin_;
    QTimer* timer_;
    double phase_=0;
    int greeting_=0;
};
}
