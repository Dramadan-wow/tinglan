#pragma once
#include <QWidget>
#include <QPixmap>
namespace im {
class ChatBackdropHsx final : public QWidget {
public:
    explicit ChatBackdropHsx(QWidget* parent=nullptr);
    void setBackground(const QString& path);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QString path_;
    QPixmap image_;
    QPixmap scaled_;
    QSize cacheSize_;
};
}
