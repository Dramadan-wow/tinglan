#include "QrLoginDialogHsx.h"
#include "AppSessionHsx.h"
#include "QrCodeHsx.h"
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
namespace im {
QrLoginDialogHsx::QrLoginDialogHsx(AppSessionHsx& session,ServiceHsx service,QWidget* parent):QDialog(parent),session_(session),service_(service),countdown_(new QTimer(this)){
    setWindowTitle("扫码登录听澜");setObjectName("qrLoginDialog");setFixedWidth(380);
    setStyleSheet("QDialog#qrLoginDialog{background:#fbfcf7;}QLabel#qrTitle{font-size:20px;font-weight:600;color:#2e472f;}QLabel#qrHint{color:#8c9a7d;font-size:12px;}QLabel#qrStatus{color:#566b4e;font-size:12px;}"
                  "QLabel#loginQr{background:#ffffff;border:1px solid #e2e8dc;border-radius:14px;}QPushButton{border-radius:10px;padding:10px;}QPushButton#refreshQr{background:#345f44;color:#ffffff;border:1px solid #345f44;}");
    auto* layout=new QVBoxLayout(this);layout->setContentsMargins(30,26,30,22);layout->setSpacing(12);
    auto* title=new QLabel("扫码登录");title->setObjectName("qrTitle");layout->addWidget(title);
    auto* hint=new QLabel(QString("打开听澜手机端，在「我的」点「扫一扫」，扫描后在手机上确认，即可登录%1。").arg(AppSessionHsx::q(serviceName(service))));hint->setObjectName("qrHint");hint->setWordWrap(true);layout->addWidget(hint);
    code_=new QLabel;code_->setObjectName("loginQr");code_->setAlignment(Qt::AlignCenter);code_->setFixedSize(276,276);layout->addWidget(code_,0,Qt::AlignHCenter);
    status_=new QLabel;status_->setObjectName("qrStatus");status_->setAlignment(Qt::AlignCenter);status_->setWordWrap(true);layout->addWidget(status_);
    refresh_=new QPushButton("刷新二维码");refresh_->setObjectName("refreshQr");refresh_->hide();layout->addWidget(refresh_);
    auto* cancel=new QPushButton("返回密码登录");cancel->setObjectName("cancelQr");layout->addWidget(cancel);
    connect(refresh_,&QPushButton::clicked,this,[this]{start();});connect(cancel,&QPushButton::clicked,this,&QDialog::reject);
    countdown_->setInterval(1000);
    connect(countdown_,&QTimer::timeout,this,[this]{if(--remaining_<=0){countdown_->stop();expire("二维码已过期，请刷新");}else if(!status_->property("scanned").toBool())status_->setText(QString("等待手机扫码 · %1 秒后过期").arg(remaining_));});
    connect(&session_,&AppSessionHsx::qrTicket,this,[this](const QString& text,int seconds){
        code_->setPixmap(render(text,256));code_->setEnabled(true);status_->setProperty("scanned",false);remaining_=seconds;status_->setText(QString("等待手机扫码 · %1 秒后过期").arg(seconds));countdown_->start();
    });
    connect(&session_,&AppSessionHsx::qrStatus,this,[this](const QString& state){
        if(state=="scanned"){status_->setProperty("scanned",true);status_->setText("已扫描，请在手机上确认登录");}
    });
    connect(&session_,&AppSessionHsx::remoteLoginFinished,this,[this](bool ok){if(finished_)return;if(ok){finished_=true;countdown_->stop();accept();}});
    connect(&session_,&AppSessionHsx::failed,this,[this](const QString& message){if(!finished_&&isVisible())expire(message);});
    start();
}
QPixmap QrLoginDialogHsx::render(const QString& text,int pixels){
    const auto code=net::QrCodeHsx::encode(text.toUtf8());const int modules=code.size()+8,scale=std::max(1,pixels/modules); // 4-module quiet zone
    QImage image(modules*scale,modules*scale,QImage::Format_RGB32);image.fill(Qt::white);QPainter p(&image);
    for(int y=0;y<code.size();++y)for(int x=0;x<code.size();++x)if(code.dark(x,y))p.fillRect((x+4)*scale,(y+4)*scale,scale,scale,QColor("#1d2b22"));
    return QPixmap::fromImage(image);
}
void QrLoginDialogHsx::start(){
    refresh_->hide();code_->clear();code_->setText("正在获取二维码…");status_->setProperty("scanned",false);status_->clear();session_.loginRemoteQr(service_);
}
void QrLoginDialogHsx::expire(const QString& reason){
    countdown_->stop();if(!session_.connected())session_.logout();status_->setText(reason);refresh_->show();
    // A spent code must not stay scannable on screen.
    code_->clear();code_->setText("二维码已失效");
}
void QrLoginDialogHsx::reject(){countdown_->stop();if(!finished_&&!session_.connected())session_.logout();QDialog::reject();}
}
