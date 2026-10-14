#pragma once
#include "AppSessionHsx.h"
#include <QMainWindow>
#include <array>
class QTableWidget; class QLineEdit; class QComboBox; class QLabel; class QCloseEvent;
namespace im {
class MembershipAdminHsx;
class AdminWindowHsx final : public QMainWindow {
    Q_OBJECT
public:
    explicit AdminWindowHsx(AppSessionHsx&, QWidget* parent = nullptr);
signals:
    void returnToLogin();
protected:
    void closeEvent(QCloseEvent*) override;
private:
    void refresh();
    void details();
    void resetPassword();
    void changePassword();
    void setAllowed(bool);
    AppSessionHsx& session_;
    QTableWidget *users_, *logs_;
    QLineEdit* search_;
    QComboBox *filter_, *logFilter_;
    QLabel *details_, *logInfo_;
    std::array<QLabel*,4> counts_;
    MembershipAdminHsx* membership_;
};
}
