#include "NetworkPageHsx.h"
#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkInterface>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
namespace im {
NetworkPageHsx::NetworkPageHsx(QWidget* parent):QWidget(parent),client_(new net::ChatClientHsx(this)){
    auto* layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);
    auto* card=new QFrame;card->setObjectName("card");auto* body=new QVBoxLayout(card);body->setContentsMargins(22,22,22,20);body->setSpacing(10);
    auto* title=new QLabel("远方来信 · 中心服务器聊天");title->setObjectName("sectionTitle");body->addWidget(title);
    auto* hint=new QLabel("使用服务端创建的远程账号登录，填写对方远程账号即可实时私聊。课程演示账号与远程账号分别管理。");
    hint->setWordWrap(true);hint->setObjectName("muted");body->addWidget(hint);
    auto* local=new QHBoxLayout;local->addWidget(new QLabel("本机 IPv4"));addresses_=new QComboBox;addresses_->setObjectName("localAddresses");local->addWidget(addresses_,1);
    auto* refresh=new QPushButton("刷新 IP");local->addWidget(refresh);connect(refresh,&QPushButton::clicked,this,&NetworkPageHsx::refreshAddresses);
    auto* copy=new QPushButton("复制");local->addWidget(copy);connect(copy,&QPushButton::clicked,this,[this]{QApplication::clipboard()->setText(addresses_->currentData().toString());});body->addLayout(local);
    refreshAddresses();auto* addressTimer=new QTimer(this);addressTimer->setInterval(15000);connect(addressTimer,&QTimer::timeout,this,&NetworkPageHsx::refreshAddresses);addressTimer->start();
    auto* serverRow=new QHBoxLayout;ip_=new QLineEdit("127.0.0.1");ip_->setObjectName("tcpIp");ip_->setPlaceholderText("服务器 IP 或域名");
    port_=new QSpinBox;port_->setObjectName("tcpPort");port_->setRange(1,65535);port_->setValue(56000);
    tls_=new QCheckBox("TLS 加密");tls_->setObjectName("tcpTls");tls_->setChecked(true);tls_->setToolTip("仅本机回环测试可关闭；局域网及互联网必须启用");
    serverRow->addWidget(new QLabel("服务器"));serverRow->addWidget(ip_,1);serverRow->addWidget(new QLabel("端口"));serverRow->addWidget(port_);serverRow->addWidget(tls_);body->addLayout(serverRow);
    auto* accountRow=new QHBoxLayout;name_=new QLineEdit;name_->setObjectName("tcpName");name_->setPlaceholderText("远程账号，如 alice");name_->setMaxLength(32);
    password_=new QLineEdit;password_->setObjectName("tcpPassword");password_->setEchoMode(QLineEdit::Password);password_->setPlaceholderText("远程密码，仅当前会话使用");password_->setMaxLength(128);
    accountRow->addWidget(new QLabel("远程账号"));accountRow->addWidget(name_,1);accountRow->addWidget(new QLabel("密码"));accountRow->addWidget(password_,1);body->addLayout(accountRow);
    auto* certRow=new QHBoxLayout;ca_=new QLineEdit;ca_->setObjectName("tcpCaFile");ca_->setPlaceholderText("可选 CA 证书；留空使用系统信任证书");
    auto* choose=new QPushButton("选择 CA");connect(choose,&QPushButton::clicked,this,[this]{const auto file=QFileDialog::getOpenFileName(this,"选择可信 CA 证书",{},"PEM 证书 (*.pem *.crt);;所有文件 (*)");if(!file.isEmpty())ca_->setText(file);});certRow->addWidget(ca_,1);certRow->addWidget(choose);body->addLayout(certRow);
    auto* controls=new QHBoxLayout;connect_=new QPushButton("连接并登录");connect_->setObjectName("tcpConnect");connect_->setProperty("primary",true);controls->addWidget(connect_);connect(connect_,&QPushButton::clicked,this,&NetworkPageHsx::connectPeer);
    auto* disconnect=new QPushButton("断开");disconnect->setObjectName("tcpDisconnect");controls->addWidget(disconnect);connect(disconnect,&QPushButton::clicked,this,&NetworkPageHsx::stop);
    state_=new QLabel("未连接");state_->setObjectName("tcpState");state_->setWordWrap(true);controls->addWidget(state_,1);body->addLayout(controls);
    log_=new QPlainTextEdit;log_->setObjectName("tcpLog");log_->setReadOnly(true);log_->setMaximumBlockCount(1000);log_->setPlaceholderText("远程消息与发送确认会显示在这里");body->addWidget(log_,1);
    auto* sendRow=new QHBoxLayout;recipient_=new QLineEdit;recipient_->setObjectName("tcpRecipient");recipient_->setPlaceholderText("对方远程账号");recipient_->setMaxLength(32);recipient_->setMaximumWidth(190);sendRow->addWidget(recipient_);
    input_=new QLineEdit;input_->setObjectName("tcpInput");input_->setMaxLength(16384);input_->setPlaceholderText("输入消息，按 Enter 发送");sendRow->addWidget(input_,1);
    send_=new QPushButton("发送");send_->setObjectName("tcpSend");send_->setProperty("primary",true);sendRow->addWidget(send_);body->addLayout(sendRow);
    auto submit=[this]{if(sendText(input_->text()))input_->clear();};connect(send_,&QPushButton::clicked,this,submit);connect(input_,&QLineEdit::returnPressed,this,submit);
    layout->addWidget(card,1);auto* note=new QLabel("自动重连 · 心跳检测 · JSON 消息协议 · 当前远程聊天仅在线转发，尚未同步至本地会话和会员数据");
    note->setObjectName("muted");note->setWordWrap(true);layout->addWidget(note);
    connect(client_,&net::ChatClientHsx::stateChanged,this,&NetworkPageHsx::updateState);
    connect(client_,&net::ChatClientHsx::authenticatedChanged,this,[this](bool online){send_->setEnabled(online);input_->setEnabled(online);});
    connect(client_,&net::ChatClientHsx::problem,this,[this](const QString& text){log_->appendPlainText("[提示] "+text);});
    connect(client_,&net::ChatClientHsx::chatReceived,this,[this](const QString& sender,const QString& text,const QString&){log_->appendPlainText("["+sender+"] "+text);emit received(sender+": "+text);});
    connect(client_,&net::ChatClientHsx::messageResult,this,[this](const QString& id,bool ok,const QString& detail){log_->appendPlainText(QString("[%1 · %2] %3").arg(ok?"确认":"未确认",id.left(8),detail));});
    updateState("未连接");
}
void NetworkPageHsx::updateState(const QString& state){state_->setText(state);log_->appendPlainText("[连接] "+state);send_->setEnabled(connected());input_->setEnabled(connected());}
void NetworkPageHsx::connectPeer(){
    net::ClientOptionsHsx options;options.host=ip_->text().trimmed();options.port=port();options.username=name_->text().trimmed();options.password=password_->text();options.tls=tls_->isChecked();options.allowInsecureLoopback=!options.tls;options.caFile=ca_->text().trimmed();client_->start(options);
}
void NetworkPageHsx::stop(){client_->stop();password_->clear();}
bool NetworkPageHsx::sendText(const QString& text){const auto id=client_->sendChat(recipient_->text().trimmed(),text);if(id.isEmpty())return false;log_->appendPlainText("[发送中 · "+id.left(8)+"] "+text);return true;}
bool NetworkPageHsx::connected()const{return client_->authenticated();}
quint16 NetworkPageHsx::port()const{return quint16(port_->value());}
void NetworkPageHsx::refreshAddresses(){
    const auto previous=addresses_->currentData().toString();QStringList values,names;
    for(const auto& interface:QNetworkInterface::allInterfaces()){
        if(!(interface.flags()&QNetworkInterface::IsUp)||!(interface.flags()&QNetworkInterface::IsRunning)||(interface.flags()&QNetworkInterface::IsLoopBack))continue;
        for(const auto& entry:interface.addressEntries()){const auto address=entry.ip();if(address.protocol()!=QAbstractSocket::IPv4Protocol||address.isLoopback()||address.isNull())continue;const auto value=address.toString();if(values.contains(value))continue;values<<value;names<<value+" · "+interface.humanReadableName();}
    }
    values<<"127.0.0.1";names<<"127.0.0.1 · 本机回环";QSignalBlocker guard(addresses_);addresses_->clear();for(int i=0;i<values.size();++i)addresses_->addItem(names[i],values[i]);const int selected=addresses_->findData(previous);if(selected>=0)addresses_->setCurrentIndex(selected);
}
}
