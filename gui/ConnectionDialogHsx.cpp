#include "ConnectionDialogHsx.h"
#include "RuntimeHsx.h"
#include "ConnectionCodeHsx.h"
#include "TypesHsx.h"
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QRadioButton>
#include <QSaveFile>
#include <QSpinBox>
#include <QSslCertificate>
#include <QUrl>
#include <QVBoxLayout>
namespace im {
namespace {
QJsonObject readConfig(const QString& path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};return QJsonDocument::fromJson(f.readAll()).object();}
}
ConnectionDialogHsx::ConnectionDialogHsx(const QString& currentFile,QWidget* parent):QDialog(parent){
    setObjectName("connectionDialog");setWindowTitle("听澜 · 服务器连接设置");setMinimumWidth(660);
    setStyleSheet(R"(
        QDialog#connectionDialog {background:#ffffff;}
        QDialog#connectionDialog QLabel {color:#70757a;font-size:12px;background:transparent;}
        QDialog#connectionDialog QLabel#connectionTitle {color:#202326;font-size:17px;font-weight:600;}
        QDialog#connectionDialog QRadioButton {color:#24282b;font-size:13px;padding:5px 0;}
        QDialog#connectionDialog QRadioButton::indicator {width:17px;height:17px;border:1px solid #dce0e3;border-radius:9px;background:white;}
        QDialog#connectionDialog QRadioButton::indicator:checked {border-color:#202326;background:qradialgradient(cx:0.5,cy:0.5,radius:0.5,fx:0.5,fy:0.5,stop:0 white,stop:0.3 white,stop:0.35 #202326,stop:1 #202326);}
        QDialog#connectionDialog QLineEdit,QDialog#connectionDialog QSpinBox {background:white;border:1px solid #dfe3e6;border-radius:19px;padding:10px 14px;color:#24282b;min-height:20px;}
        QDialog#connectionDialog QLineEdit:focus,QDialog#connectionDialog QSpinBox:focus {border-color:#718f82;}
        QDialog#connectionDialog QPushButton {border:1px solid #dfe3e6;background:white;border-radius:17px;padding:9px 15px;color:#33383c;}
        QDialog#connectionDialog QPushButton#saveConnection {background:#1d2124;border-color:#1d2124;color:white;font-weight:600;}
    )");
    auto* layout=new QVBoxLayout(this);layout->setContentsMargins(24,22,24,22);layout->setSpacing(9);
    auto* title=new QLabel("听澜 · 服务器连接设置");title->setObjectName("connectionTitle");layout->addWidget(title);
    layout->addWidget(new QLabel("连接到你的听澜中心服务器"));layout->addSpacing(14);
    auto* code=new QPlainTextEdit;code->setObjectName("connectionCode");code->setPlaceholderText("粘贴服务器提供的 TINGLAN1. 连接码，自动识别地址和公共证书");code->setFixedHeight(60);layout->addWidget(code);
    connect(code,&QPlainTextEdit::textChanged,this,[this,code]{const auto value=code->toPlainText().trimmed();if(value.startsWith("TINGLAN1."))importCode(value);});
    auto* line=new QWidget;line->setFixedHeight(1);line->setStyleSheet("background:#eef0f2;");layout->addWidget(line);layout->addSpacing(8);
    layout->addWidget(new QLabel("连接模式"));
    local_=new QRadioButton("本地模式（自动启动本机服务器）",this);local_->setObjectName("modeLocal");
    lan_=new QRadioButton("局域网模式（连接其他电脑）",this);lan_->setObjectName("modeLan");
    cloud_=new QRadioButton("云服务器模式（通过互联网连接）",this);cloud_->setObjectName("modeCloud");
    for(auto* radio:{local_,lan_,cloud_})layout->addWidget(radio);
    layout->addWidget(new QLabel("服务器地址"));host_=new QLineEdit;host_->setObjectName("serverHost");host_->setPlaceholderText("填写电脑 A 的 IP 或已部署的服务器域名");layout->addWidget(host_);
    // A spin box accepts wheel edits by default; one scroll here silently moves the chat port
    // by one and the client then dials a port that never completes the TLS handshake.
    layout->addWidget(new QLabel("服务器端口"));port_=new QSpinBox;port_->setObjectName("serverPort");port_->setRange(1,65535);port_->setButtonSymbols(QAbstractSpinBox::NoButtons);port_->setFocusPolicy(Qt::StrongFocus);port_->setValue(56110);layout->addWidget(port_);
    tls_=new QLabel;tls_->setWordWrap(true);tls_->setObjectName("connectionTls");layout->addWidget(tls_);
    caBox_=new QWidget;auto* caLayout=new QVBoxLayout(caBox_);caLayout->setContentsMargins(0,0,0,0);caLayout->setSpacing(7);
    caLayout->addWidget(new QLabel("CA 证书（局域网测试选择电脑 A 提供的 ca.pem；留空使用系统信任证书）"));
    auto* row=new QHBoxLayout;ca_=new QLineEdit;ca_->setObjectName("serverCa");ca_->setPlaceholderText("选择可信 CA 文件，不需要导入系统证书库");row->addWidget(ca_);
    auto* browse=new QPushButton("选择证书");row->addWidget(browse);caLayout->addLayout(row);layout->addWidget(caBox_);
    connect(browse,&QPushButton::clicked,this,[this]{const auto f=QFileDialog::getOpenFileName(this,"选择服务器 CA",{},"PEM 证书 (*.pem *.crt);;所有文件 (*)");if(!f.isEmpty())ca_->setText(f);});
    auto* note=new QLabel("保存后返回登录页，登录时连接所选服务器。账号、聊天与会员权益以该服务器数据库为准。");note->setWordWrap(true);layout->addWidget(note);
    error_=new QLabel;error_->setObjectName("connectionError");error_->setWordWrap(true);error_->setStyleSheet("color:#b35243;");layout->addWidget(error_);
    auto* buttons=new QHBoxLayout;auto* copy=new QPushButton("复制连接码");copy->setObjectName("copyConnection");auto* cancel=new QPushButton("取消");auto* saveButton=new QPushButton("保存并返回登录");saveButton->setObjectName("saveConnection");buttons->addWidget(copy);buttons->addStretch();buttons->addWidget(cancel);buttons->addWidget(saveButton);layout->addLayout(buttons);
    originalLocal_=readConfig(runtimeDirectory()+"/client.json");
    if(originalLocal_["host"]!="127.0.0.1"||!originalLocal_["autoStartLocalServer"].toBool())originalLocal_={{"host","127.0.0.1"},{"port",56110},{"tls",false},{"allowInsecureLoopback",true},{"autoStartLocalServer",true}};
    const auto c=readConfig(currentFile);remoteHost_=c["autoStartLocalServer"].toBool()?QString{}:c["host"].toString();remotePort_=c["port"].toInt(56110);
    if(!c["caFile"].toString().isEmpty())remoteCa_=QDir(QFileInfo(currentFile).absolutePath()).absoluteFilePath(c["caFile"].toString());
    local_->setEnabled(QFile::exists(QCoreApplication::applicationDirPath()+"/im_server.exe"));local_->setToolTip("本地模式需要完整发行包中的 im_server.exe；朋友专用包仅支持远程连接。");
#ifdef IM_USER_CLIENT
    local_->setEnabled(false);local_->hide();
#endif
    if(c["autoStartLocalServer"].toBool()&&local_->isEnabled())local_->setChecked(true);else if(c["mode"]=="cloud")cloud_->setChecked(true);else lan_->setChecked(true);
    host_->setText(remoteHost_);port_->setValue(remotePort_);ca_->setText(remoteCa_);
    for(auto* radio:{local_,lan_,cloud_})connect(radio,&QRadioButton::toggled,this,[this](bool checked){if(checked)updateMode();});
    updateMode();
    connect(cancel,&QPushButton::clicked,this,&QDialog::reject);connect(saveButton,&QPushButton::clicked,this,[this]{save();});
    connect(copy,&QPushButton::clicked,this,[this]{try{const auto c=configuration();require(!local_->isChecked(),"本机开发模式不可分享，请先配置局域网或云服务器");QString pem;
        if(!c["caFile"].toString().isEmpty()){if(c["caFile"]==importedCaPath_&&!importedCa_.isEmpty())pem=importedCa_;else{QFile f(c["caFile"].toString());require(f.open(QIODevice::ReadOnly)&&f.size()<=8192,"无法读取公共 CA");pem=QString::fromUtf8(f.readAll());}}
        QApplication::clipboard()->setText(net::ConnectionCodeHsx::encode({{"version",1},{"host",c["host"]},{"port",c["port"]},{"mode",c["mode"]},{"ca",pem}}));error_->setText("连接码已复制，可以直接以文字发给朋友；不含账号、密码或私钥。");}catch(const std::exception& e){error_->setText(QString::fromUtf8(e.what()));}});
}
bool ConnectionDialogHsx::importCode(const QString& text){
    try{const auto c=net::ConnectionCodeHsx::decode(text);(c["mode"]=="cloud"?cloud_:lan_)->setChecked(true);
        importedCa_=c["ca"].toString();importedCaPath_=importedCa_.isEmpty()?QString{}:runtimeDirectory()+"/trusted-ca/"+net::ConnectionCodeHsx::fingerprint(importedCa_)+".pem";
        host_->setText(c["host"].toString());port_->setValue(c["port"].toInt());ca_->setText(importedCaPath_);
        error_->setText("已识别。请确认连接码来自服务器所有者，再保存。\nCA SHA-256："+net::ConnectionCodeHsx::fingerprint(importedCa_));return true;
    }catch(const std::exception& e){error_->setText(QString::fromUtf8(e.what()));return false;}
}
void ConnectionDialogHsx::updateMode(){
    const bool local=local_->isChecked();
    if(local&&!wasLocal_){remoteHost_=host_->text();remotePort_=port_->value();remoteCa_=ca_->text();host_->setText(originalLocal_["host"].toString());port_->setValue(originalLocal_["port"].toInt());}
    if(!local&&wasLocal_){host_->setText(remoteHost_);port_->setValue(remotePort_);ca_->setText(remoteCa_);}
    wasLocal_=local;host_->setEnabled(!local);port_->setEnabled(!local);caBox_->setVisible(!local);
    tls_->setText(local?(originalLocal_["tls"].toBool()?"TLS 加密连接 · 使用原有本机配置":"本地回环开发模式 · 仅连接 127.0.0.1，保留原有配置"):"✓ TLS 加密连接　·　必须启用\n严格验证服务器证书、有效期及地址，不允许忽略证书错误。");error_->clear();
}
QJsonObject ConnectionDialogHsx::configuration() const{
    if(local_->isChecked()){auto c=originalLocal_;c["mode"]="local";if(!c["caFile"].toString().isEmpty())c["caFile"]=QDir(runtimeDirectory()).absoluteFilePath(c["caFile"].toString());return c;}
    const auto host=host_->text().trimmed();QUrl url;url.setScheme("tls");url.setHost(host);
    require(!host.isEmpty()&&url.isValid()&&!url.host().isEmpty()&&!host.contains('/')&&!host.contains(':')&&!host.contains(' ')&&host!="0.0.0.0","请填写有效的 IPv4 地址或域名，不要填写协议、端口或 0.0.0.0");
    const auto ca=ca_->text().trimmed();if(!ca.isEmpty()&&(ca!=importedCaPath_||importedCa_.isEmpty())){QFile f(ca);require(f.open(QIODevice::ReadOnly)&&f.size()<=8192&&!QSslCertificate::fromData(f.readAll(),QSsl::Pem).isEmpty(),"无法读取有效的 PEM CA 证书");}
    return {{"mode",cloud_->isChecked()?"cloud":"lan"},{"host",host},{"port",port_->value()},{"tls",true},{"allowInsecureLoopback",false},{"autoStartLocalServer",false},{"caFile",ca.isEmpty()?QString{}:QFileInfo(ca).absoluteFilePath()}};
}
void ConnectionDialogHsx::save(){
    try{const auto c=configuration();
        if(c["caFile"]==importedCaPath_&&!importedCa_.isEmpty()){
            require(QDir().mkpath(QFileInfo(importedCaPath_).absolutePath()),"无法创建公共 CA 目录");QSaveFile cert(importedCaPath_);require(cert.open(QIODevice::WriteOnly),"无法保存公共 CA");const auto bytes=importedCa_.toUtf8();require(cert.write(bytes)==bytes.size()&&cert.commit(),"公共 CA 保存失败");}
        const auto path=runtimeDirectory()+"/connection-active.json";QSaveFile out(path);require(out.open(QIODevice::WriteOnly),"无法保存连接设置");const auto bytes=QJsonDocument(c).toJson();require(out.write(bytes)==bytes.size()&&out.commit(),"连接设置保存失败");savedFile_=path;accept();}
    catch(const std::exception& e){error_->setText(QString::fromUtf8(e.what()));}
}
}
