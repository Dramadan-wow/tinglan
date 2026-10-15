#pragma once
#include "UpdateClientHsx.h"
#include <QObject>
#include <QPointer>
class QDialog;
class QPlainTextEdit;
class QPushButton;
class QProgressBar;
namespace im {
class UpdateManagerHsx final:public QObject {
public:
    explicit UpdateManagerHsx(QObject* parent);
    void configure(const QString& file);
    static void showUpdates();
private:
    void show();
    void check(bool manual);
    void status(const QString& text);
    void install();
    net::UpdateClientHsx client_;
    QTimer periodic_;
    QString config_,download_,status_="尚未检查更新";
    QJsonObject release_;
    QPointer<QDialog> dialog_;
    QPlainTextEdit* label_=nullptr;
    QProgressBar* progress_=nullptr;
    QPushButton *check_=nullptr,*downloadButton_=nullptr,*install_=nullptr;
};
}
