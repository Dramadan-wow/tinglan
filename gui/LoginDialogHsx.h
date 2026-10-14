#pragma once
#include <QDialog>
#include <functional>
class QLineEdit;
class QComboBox;
class QLabel;
class QCheckBox;
class QPushButton;
class QAction;
namespace im {
class AppSessionHsx;
class LoginDialogHsx final : public QDialog {
public:
    explicit LoginDialogHsx(AppSessionHsx& session, QWidget* parent = nullptr, bool admin = false, std::function<bool()> settings = {});
private:
    void services();
    void login();
    void registerUser();
    void roleChanged();
    AppSessionHsx& session_;
    QLineEdit *user_, *password_;
    QComboBox *service_, *role_;
    QCheckBox* linked_;
    QLabel *error_, *title_, *intro_, *hint_, *userLabel_;
    QPushButton *register_, *userRole_, *adminRole_;
    QWidget* serviceBox_;
    QAction* reveal_;
};
}
