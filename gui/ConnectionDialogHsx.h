#pragma once
#include <QDialog>
#include <QJsonObject>
class QRadioButton;
class QLineEdit;
class QSpinBox;
class QLabel;
namespace im {
class ConnectionDialogHsx final : public QDialog {
public:
    explicit ConnectionDialogHsx(const QString& currentFile, QWidget* parent=nullptr);
    QString savedFile() const { return savedFile_; }
    bool importCode(const QString& text);
private:
    void updateMode();
    QJsonObject configuration() const;
    void save();
    QRadioButton *local_, *lan_, *cloud_;
    QLineEdit *host_, *ca_;
    QSpinBox* port_;
    QLabel *tls_, *error_;
    QWidget* caBox_;
    QJsonObject originalLocal_;
    QString savedFile_, remoteHost_, remoteCa_;
    QString importedCa_,importedCaPath_;
    int remotePort_=56110;
    bool wasLocal_=false;
};
}
