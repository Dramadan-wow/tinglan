#pragma once
#include "FileTransferManagerHsx.h"
#include <QFrame>
class QLabel;class QPushButton;class QProgressBar;
namespace im {
class FileMessageCardHsx final : public QFrame {
public:
    FileMessageCardHsx(net::FileTransferManagerHsx& manager,const QJsonObject& record,bool mine,QWidget* parent=nullptr);
private:
    void refresh();
    net::FileTransferManagerHsx& manager_;
    QJsonObject record_;
    QString id_;
    QLabel *status_,*rate_;
    QProgressBar* progress_;
    QPushButton *download_,*pause_,*resume_,*cancel_,*folder_;
};
}
