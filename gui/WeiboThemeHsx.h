#pragma once
#include <QWidget>
#include <QString>
namespace im {
class WeiboHeroHsx final:public QWidget {
public:
    explicit WeiboHeroHsx(QWidget* parent=nullptr);
protected:
    void paintEvent(QPaintEvent*) override;
};
QString weiboChrome();
QString weiboContentStyle();
}
