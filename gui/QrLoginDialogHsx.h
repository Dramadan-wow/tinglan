#pragma once
#include "TypesHsx.h"
#include <QDialog>
class QLabel;
class QPushButton;
class QTimer;
namespace im {
class AppSessionHsx;
// Shows a one-time sign-in QR code; the phone app scans it and confirms, then this computer signs in.
class QrLoginDialogHsx final : public QDialog {
public:
    QrLoginDialogHsx(AppSessionHsx& session,ServiceHsx service,QWidget* parent=nullptr);
    static QPixmap render(const QString& text,int pixels);
protected:
    void reject() override;
private:
    void start();
    void expire(const QString& reason);
    AppSessionHsx& session_;
    ServiceHsx service_;
    QLabel *code_,*status_;
    QPushButton* refresh_;
    QTimer* countdown_;
    int remaining_=0;
    bool finished_=false;
};
}
