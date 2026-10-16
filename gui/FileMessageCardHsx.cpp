#include "FileMessageCardHsx.h"
#include "IconHsx.h"
#include <QDateTime>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>
namespace im {
namespace {QString bytes(double value){for(const auto& unit:QStringList{"B","KB","MB","GB"}){if(value<1024||unit=="GB")return QString::number(value,'f',unit=="B"?0:1)+" "+unit;value/=1024;}return {};}}
FileMessageCardHsx::FileMessageCardHsx(net::FileTransferManagerHsx& manager,const QJsonObject& record,bool mine,QWidget* p):QFrame(p),manager_(manager),record_(record),id_(record["localId"].toString()){
    setObjectName("fileMessageCard");setMinimumWidth(220);setMaximumWidth(360);setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
    setStyleSheet(QString("QFrame#fileMessageCard{background:%1;border:1px solid #d1e3f7;border-radius:16px;} QLabel{background:transparent;border:0;} QPushButton{padding:5px 9px;border-radius:8px;background:#f4f9ff;color:#2675b8;border:1px solid #dce9f6;} QProgressBar{border:0;background:#e4edf8;border-radius:3px;} QProgressBar::chunk{background:#369aff;border-radius:3px;}").arg(mine?"#dcefff":"#ffffff"));
    auto* layout=new QVBoxLayout(this);layout->setContentsMargins(14,14,14,12);layout->setSpacing(8);layout->setAlignment(Qt::AlignTop);auto* head=new QHBoxLayout;
    auto* icon=new QLabel;icon->setPixmap(IconHsx::get("file","#2388eb").pixmap(36,36));icon->setFixedSize(40,44);head->addWidget(icon);
    auto* title=new QVBoxLayout;auto* name=new QLabel(record["name"].toString());name->setTextFormat(Qt::PlainText);name->setWordWrap(true);name->setStyleSheet("font-size:13px;font-weight:600;color:#213c60;");title->addWidget(name);
    auto suffix=QFileInfo(record["name"].toString()).suffix().toUpper();auto* info=new QLabel(bytes(record["size"].toDouble())+" · "+(suffix.isEmpty()?"文件":suffix+" 文件"));info->setTextFormat(Qt::PlainText);info->setStyleSheet("font-size:10px;color:#758ba5;");title->addWidget(info);head->addLayout(title,1);layout->addLayout(head);
    status_=new QLabel;status_->setTextFormat(Qt::PlainText);status_->setWordWrap(true);status_->setStyleSheet("color:#466685;font-size:11px;");layout->addWidget(status_);
    progress_=new QProgressBar;progress_->setObjectName("fileProgress");progress_->setRange(0,1000);progress_->setTextVisible(false);progress_->setFixedHeight(5);layout->addWidget(progress_);
    rate_=new QLabel;rate_->setTextFormat(Qt::PlainText);rate_->setStyleSheet("color:#7990aa;font-size:10px;");layout->addWidget(rate_);
    auto* actions=new QHBoxLayout;actions->setSpacing(5);auto button=[&](const QString& text,const QString& name){auto* b=new QPushButton(text);b->setObjectName(name);b->setCursor(Qt::PointingHandCursor);actions->addWidget(b);return b;};
    download_=button("下载","fileDownload");pause_=button("暂停","filePause");resume_=button("继续","fileResume");cancel_=button("取消","fileCancel");folder_=button("打开文件夹","fileFolder");actions->addStretch();layout->addLayout(actions);
    connect(download_,&QPushButton::clicked,this,[this]{const auto name=QFileInfo(record_["name"].toString()).fileName();const auto path=QFileDialog::getSaveFileName(this,"保存收到的文件",QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)+"/"+name);if(!path.isEmpty())manager_.download(record_,path);});
    connect(pause_,&QPushButton::clicked,this,[this]{manager_.pause(id_);});connect(resume_,&QPushButton::clicked,this,[this]{manager_.resume(id_);});connect(cancel_,&QPushButton::clicked,this,[this]{manager_.cancel(id_);});
    connect(folder_,&QPushButton::clicked,this,[this]{QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(manager_.task(id_)["path"].toString()).absolutePath()));});
    connect(&manager_,&net::FileTransferManagerHsx::taskChanged,this,[this](const QString& id){if(id==id_)refresh();});refresh();
}
void FileMessageCardHsx::refresh(){
    auto t=id_.isEmpty()?record_:manager_.task(id_);const auto s=t["status"].toString();const bool done=s=="complete",upload=t["kind"]=="upload",ready=t["state"]=="ready"||done;
    static const QHash<QString,QString> labels{{"waiting","等待连接后继续"},{"hashing","正在校验 SHA-256…"},{"uploading","正在上传"},{"downloading","正在下载"},{"verifying","服务器正在校验…"},{"paused","已暂停"},{"cancelled","已取消"},{"failed","传输失败"}};
    auto text=done?(upload?"已上传 · 对方可随时下载":"已下载 · SHA-256 校验通过"):labels.value(s,ready?"离线文件 · 点击下载":"文件不可用");if(!t["error"].toString().isEmpty())text+="\n"+t["error"].toString();status_->setText(text);
    const double total=t["size"].toDouble(),offset=t["offset"].toDouble(),progress=qMin(total,qMax(offset,t["progress"].toDouble())),speed=t["speed"].toDouble();const bool moving=s=="uploading"||s=="downloading";
    progress_->setValue(done?1000:total>0?int(progress/total*1000):0);progress_->setVisible(!id_.isEmpty()&&s!="cancelled");
    rate_->setVisible(!id_.isEmpty());rate_->setText(moving?bytes(progress)+" / "+bytes(total)+" · "+bytes(speed)+"/s · "+(speed>0?"约 "+QString::number(int((total-progress)/speed))+" 秒":"估算中"):QDateTime::fromSecsSinceEpoch(qint64(t["created"].toDouble())).toString("MM-dd HH:mm"));
    download_->setVisible(ready&&(id_.isEmpty()||upload||s=="cancelled"));download_->setEnabled(manager_.available());folder_->setVisible(done&&!upload);
    pause_->setVisible(!id_.isEmpty()&&(moving||s=="hashing"||s=="waiting"));resume_->setVisible(s=="paused"||s=="failed");resume_->setText(s=="failed"?"重试":"继续");
    cancel_->setVisible(!id_.isEmpty()&&!done&&s!="cancelled"&&s!="verifying");
}
}
