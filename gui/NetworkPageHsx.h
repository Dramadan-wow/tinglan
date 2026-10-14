#pragma once
#include <QWidget>
#include "ChatClientHsx.h"
class QLineEdit; class QSpinBox; class QLabel; class QPlainTextEdit; class QPushButton; class QComboBox; class QCheckBox;
namespace im {
class NetworkPageHsx final : public QWidget {
    Q_OBJECT
public:
    explicit NetworkPageHsx(QWidget* parent=nullptr);
    void connectPeer();
    void stop();
    bool sendText(const QString& text);
    bool connected() const;
    quint16 port() const;
    void refreshAddresses();
    net::ChatClientHsx* client() const { return client_; }
signals:
    void received(const QString& text);
private:
    void updateState(const QString&);
    net::ChatClientHsx* client_;
    QLineEdit *ip_,*name_,*password_,*recipient_,*input_,*ca_;
    QSpinBox* port_;
    QLabel* state_;
    QPlainTextEdit* log_;
    QPushButton *connect_,*send_;
    QComboBox* addresses_;
    QCheckBox* tls_;
};
}
