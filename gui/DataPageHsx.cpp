#include "DataPageHsx.h"
#include "UiHsx.h"
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QLabel>
#include <QMessageBox>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>
namespace im {
using S=AppSessionHsx;
DataPageHsx::DataPageHsx(AppSessionHsx& s,QWidget* parent):QWidget(parent),session_(s){auto* l=new QVBoxLayout(this);l->setContentsMargins(0,0,0,0);l->setSpacing(18);auto* cards=new QHBoxLayout;const QStringList titles{"注册用户","服务账号","群组空间","历史消息"};for(int i=0;i<4;++i){auto* f=new QFrame;f->setObjectName("card");auto* c=new QVBoxLayout(f);c->setContentsMargins(22,22,22,22);auto* label=new QLabel(titles[i]);label->setObjectName("muted");c->addWidget(label);counts_[i]=new QLabel("0");counts_[i]->setStyleSheet("font-size:30px;font-weight:700;color:#4f7553;");c->addWidget(counts_[i]);cards->addWidget(f);}l->addLayout(cards);
    auto* card=new QFrame;card->setObjectName("card");auto* c=new QVBoxLayout(card);c->setContentsMargins(22,22,22,20);c->setSpacing(14);auto* heading=new QLabel("服务分布");heading->setObjectName("sectionTitle");c->addWidget(heading);services_=UiHsx::table({"服务","账号数","好友关系数","群成员关系数"});c->addWidget(services_,1);l->addWidget(card,1);
    auto* fileCard=new QFrame;fileCard->setObjectName("card");auto* fc=new QVBoxLayout(fileCard);fc->setContentsMargins(22,20,22,20);fc->setSpacing(10);auto* title=new QLabel(session_.remote()?"服务器数据":"本地数据");title->setObjectName("sectionTitle");fc->addWidget(title);const auto path=QString::fromStdWString(session_.file().wstring());auto* file=new QLabel(session_.remote()?"账号、会话和微博统一保存于服务器；本机仅保存个性装扮。":path);file->setWordWrap(true);file->setTextInteractionFlags(Qt::TextSelectableByMouse);fc->addWidget(file);state_=new QLabel;state_->setObjectName("muted");fc->addWidget(state_);auto* actions=new QHBoxLayout;
    if(!session_.remote()){UiHsx::button(actions,"立即保存",[this]{if(session_.save())QMessageBox::information(this,"保存完成","数据已写入本地文件。");refresh();},true);UiHsx::button(actions,"打开数据目录",[this,path]{QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));});
    UiHsx::button(actions,"导出数据副本",[this,path]{if(!session_.save())return;const auto target=QFileDialog::getSaveFileName(this,"导出数据副本","即时通信数据.txt","数据文件 (*.txt)");if(target.isEmpty())return;if(QFile::exists(target)){QMessageBox::information(this,"请选择新文件","为避免覆盖已有数据，请使用新的文件名。 ");return;}if(!QFile::copy(path,target))QMessageBox::warning(this,"导出失败","请检查目标目录是否可写。 ");else QMessageBox::information(this,"导出完成","数据副本已导出。 ");});}else UiHsx::button(actions,"刷新服务器数据",[this]{session_.command("sync",{});},true);actions->addStretch();fc->addLayout(actions);l->addWidget(fileCard);
    auto* note=new QLabel(session_.remote()?"课程版 · 此处仅统计当前账号可见的数据；服务器提交成功后自动保存。":"课程模拟系统 · 数据保存在本机 · 当前为离线演示模式");note->setWordWrap(true);note->setObjectName("muted");l->addWidget(note);
}
void DataPageHsx::refresh(){size_t accounts=0;for(const auto& [id,u]:session_.data().users())accounts+=u->services().size();counts_[0]->setText(QString::number(session_.data().users().size()));counts_[1]->setText(QString::number(accounts));counts_[2]->setText(QString::number(session_.data().groups().size()));counts_[3]->setText(QString::number(session_.data().messages().size()));services_->setRowCount(0);
    for(int i=0;i<ActiveServiceCountHsx;++i){const auto type=static_cast<ServiceHsx>(i);int ac=0,fr=0,gr=0;for(const auto& [id,u]:session_.data().users()){const auto a=u->services().find(type);if(a!=u->services().end()){++ac;fr+=static_cast<int>(a->second.friends().size());}}for(const auto& [id,g]:session_.data().groups())for(const auto& [uid,types]:g->members())if(types.count(type))++gr;UiHsx::row(services_,{S::q(serviceName(type)),QString::number(ac),QString::number(fr),QString::number(gr)},i+1);}state_->setText(session_.remote()?"服务器统一存储 · 备份需由服务器管理员执行":session_.data().dirty()?"有未保存的改动，请重试保存。":"已保存 · 每次业务操作自动写入，上一版数据保留为 .bak");
}
}
