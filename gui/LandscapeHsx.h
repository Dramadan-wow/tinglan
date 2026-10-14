#pragma once
#include <QWidget>
#include <QPixmap>
namespace im {
class LandscapeHsx final : public QWidget {
public:
    explicit LandscapeHsx(QWidget* parent=nullptr);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QPixmap artwork_;
};
}
