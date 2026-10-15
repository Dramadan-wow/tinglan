#include "UpdateManagerHsx.h"
#include "RuntimeHsx.h"
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QLabel>
#include <QPlainTextEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QProcess>
#include <QSettings>
#include <QVBoxLayout>
namespace im {
UpdateManagerHsx::UpdateManagerHsx(QObject* parent):QObject(parent){
    setObjectName("tinglanUpdater");periodic_.setInterval(6*60*60*1000);
    connect(&periodic_,&QTimer::timeout,this,[this]{if(QSettings().value("updates/automatic",true).toBool())check(false);});periodic_.start();
    connect(&client_,&net::UpdateClientHsx::failed,this,[this](const QString& text){status(text);});
    connect(&client_,&net::UpdateClientHsx::current,this,[this](bool published){status(published?"当前已是最新版本。":"中心服务器尚未发布客户端更新。");});
    connect(&client_,&net::UpdateClientHsx::available,this,[this](const QJsonObject& r){
        release_=r;show();status(QString("发现新版本 %1（当前 %2）\n安装包：%3 MiB\n\n%4\n\n下载并校验后，可选择关闭听澜并升级。")
            .arg(r["version"].toString(),QCoreApplication::applicationVersion()).arg(r["size"].toDouble()/1048576.0,0,'f',1).arg(r["notes"].toString()));
    });
    connect(&client_,&net::UpdateClientHsx::progress,this,[this](qint64 done,qint64 total){if(dialog_)progress_->setValue(int(done*100/total));});
    connect(&client_,&net::UpdateClientHsx::ready,this,[this](const QString& file){download_=file;status("下载完成，SHA-256 校验通过。点击“安装并升级”后将关闭听澜。账号和服务器连接设置会保留。");});
}
void UpdateManagerHsx::configure(const QString& file){
    client_.cancel();config_=file;release_={};download_.clear();status("尚未检查更新");
    QTimer::singleShot(4000,this,[this,file]{if(config_==file&&QSettings().value("updates/automatic",true).toBool())check(false);});
}
void UpdateManagerHsx::showUpdates(){
    for(auto* object:qApp->children())if(auto* manager=dynamic_cast<UpdateManagerHsx*>(object)){manager->show();return;}
}
void UpdateManagerHsx::show(){
    if(!dialog_){
        dialog_=new QDialog;dialog_->setAttribute(Qt::WA_DeleteOnClose);dialog_->setWindowTitle("听澜 · 软件更新");dialog_->setMinimumWidth(480);
        auto* layout=new QVBoxLayout(dialog_);layout->setContentsMargins(24,24,24,24);layout->setSpacing(14);
        auto* title=new QLabel("听澜 "+QCoreApplication::applicationVersion()+" · 普通客户端");layout->addWidget(title);
        label_=new QPlainTextEdit;label_->setReadOnly(true);label_->setMinimumHeight(150);label_->setMaximumHeight(240);layout->addWidget(label_);
        progress_=new QProgressBar;progress_->setRange(0,100);progress_->setValue(0);layout->addWidget(progress_);
        auto* automatic=new QCheckBox("启动时及每 6 小时自动检查更新");automatic->setChecked(QSettings().value("updates/automatic",true).toBool());layout->addWidget(automatic);
        connect(automatic,&QCheckBox::toggled,this,[](bool on){QSettings().setValue("updates/automatic",on);});
        check_=new QPushButton("检查更新");downloadButton_=new QPushButton("下载新版本");install_=new QPushButton("安装并升级");auto* cancel=new QPushButton("取消下载");
        for(auto* button:{check_,downloadButton_,install_,cancel})layout->addWidget(button);
        connect(check_,&QPushButton::clicked,this,[this]{check(true);});
        connect(downloadButton_,&QPushButton::clicked,this,[this]{client_.download(runtimeDirectory()+"/updates");if(client_.busy())status("正在通过加密连接下载安装包…");});
        connect(install_,&QPushButton::clicked,this,&UpdateManagerHsx::install);
        connect(cancel,&QPushButton::clicked,this,[this]{if(client_.busy()){client_.cancel();status("已取消，可以重新下载。");}});
    }
    status(status_);dialog_->show();dialog_->raise();dialog_->activateWindow();
}
void UpdateManagerHsx::status(const QString& text){
    status_=text;if(!dialog_)return;label_->setPlainText(text);check_->setEnabled(!client_.busy());
    downloadButton_->setEnabled(!client_.busy()&&!release_.isEmpty()&&download_.isEmpty());install_->setEnabled(!download_.isEmpty()&&!client_.busy());
}
void UpdateManagerHsx::check(bool manual){
    if(client_.busy()||(!manual&&!download_.isEmpty()))return;release_={};download_.clear();
    if(manual)show();status("正在检查中心服务器发布的更新…");client_.check(config_,QCoreApplication::applicationVersion());
    if(client_.busy())status("正在检查中心服务器发布的更新…");
}
void UpdateManagerHsx::install(){
    if(download_.isEmpty()||QMessageBox::question(dialog_,"安装更新","将关闭听澜并启动安装程序。安装完成后会重新打开客户端。现在升级吗？")!=QMessageBox::Yes)return;
    // Inno Setup waits for this PID to exit before touching installed files.
    // No shell and no server-supplied command arguments are involved.
    const QStringList args{"/SILENT","/NORESTART","/NOCLOSEAPPLICATIONS","/NORESTARTAPPLICATIONS","/TINGLANUPDATE=1","/TINGLANPID="+QString::number(QCoreApplication::applicationPid())};
    if(!QProcess::startDetached(download_,args)){status("无法启动安装程序，请稍后重试。");return;}
    if(dialog_)dialog_->close();qApp->closeAllWindows();
}
}
