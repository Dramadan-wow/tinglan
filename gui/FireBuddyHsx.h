#pragma once
#include <QWidget>
#include <QJsonObject>
class QTimer;
namespace im {
// The friendship flame. Card: a strip with status text (dress-up preview). Floating: hovers over the chat
// area, can be dragged, and answers a click with a speech bubble. Swatch: a static wardrobe sample.
class FireBuddyHsx final : public QWidget {
public:
    enum class Mode { Card, Floating, Swatch };
    explicit FireBuddyHsx(QWidget* parent=nullptr,Mode mode=Mode::Card);
    void setState(const QJsonObject& state,const QJsonObject& skin);
    void setOutfit(const QJsonObject& outfit);
protected:
    void paintEvent(QPaintEvent*) override;
    void showEvent(QShowEvent*) override;
    void hideEvent(QHideEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    bool eventFilter(QObject* watched,QEvent* event) override;
private:
    void drawBuddy(QPainter& p,const QRectF& box,bool active) const;
    void place();
    void greet();
    QString statusLine() const;
    Mode mode_;
    QJsonObject state_,skin_,outfit_;
    QTimer* timer_;
    double phase_=0;
    int greeting_=0,pokes_=0;
    QPoint pressed_,dragMargin_;
    QPoint margin_{18,12}; // gap between the floating buddy and its host's bottom-right corner
    bool dragging_=false;
};
}
