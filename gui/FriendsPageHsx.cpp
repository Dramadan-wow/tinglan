#include "FriendsPageHsx.h"
#include "FormDialogHsx.h"
#include "UiHsx.h"
#include <QComboBox>
#include <QDialog>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QListWidget>
#include <QPushButton>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QTableWidget>
#include <QVBoxLayout>
namespace im {
using S=AppSessionHsx;
FriendsPageHsx::FriendsPageHsx(AppSessionHsx& s,QWidget* parent):QWidget(parent),session_(s){
    auto* layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);auto* card=new QFrame;card->setObjectName("card");auto* c=new QVBoxLayout(card);c->setContentsMargins(22,22,22,20);c->setSpacing(14);
    auto* top=new QHBoxLayout;auto* title=new QLabel("联系人");title->setObjectName("sectionTitle");top->addWidget(title);top->addStretch();UiHsx::button(top,"添加好友",[this]{addFriend();},true);c->addLayout(top);
    auto* filters=new QHBoxLayout;search_=new QLineEdit;search_->setPlaceholderText("搜索昵称、备注或分组");search_->setClearButtonEnabled(true);filter_=new QComboBox;filter_->addItems({"我的好友","黑名单"});filters->addWidget(search_,1);filters->addWidget(filter_);c->addLayout(filters);
    table_=UiHsx::table({"内部编号","昵称与备注","地区","分组 / 关系","服务"});table_->hideColumn(0);table_->setObjectName("friendsTable");c->addWidget(table_,1);
    auto* actions=new QHBoxLayout;UiHsx::button(actions,"发消息",[this]{const int id=static_cast<int>(UiHsx::selected(table_));if(id)emit openChat(id);},true);
    UiHsx::button(actions,"编辑备注",[this]{editFriend();});UiHsx::button(actions,"删除好友",[this]{const int id=static_cast<int>(UiHsx::selected(table_));if(id&&UiHsx::confirm(this,"删除好友关系？"))session_.command("friend.remove",{{"target",id}},[&]{session_.friends().remove(session_.user(),session_.service(),id);},"好友已删除");});
    UiHsx::button(actions,"拉黑",[this]{block(true);});UiHsx::button(actions,"取消拉黑",[this]{block(false);});actions->addStretch();c->addLayout(actions);layout->addWidget(card,1);
    auto* bottom=new QHBoxLayout;summary_=new QLabel;summary_->setObjectName("muted");bottom->addWidget(summary_,1);UiHsx::button(bottom,"跨服务共同好友",[this]{compare(true);});UiHsx::button(bottom,"跨服务好友推荐",[this]{compare(false);});layout->addLayout(bottom);
    connect(search_,&QLineEdit::textChanged,this,[this]{refresh();});connect(filter_,&QComboBox::currentIndexChanged,this,[this]{refresh();});
    connect(table_,&QTableWidget::cellDoubleClicked,this,[this](int,int){const int id=static_cast<int>(UiHsx::selected(table_));if(id)emit openChat(id);});
}
void FriendsPageHsx::refresh(){
    const auto selected=UiHsx::selected(table_);table_->setRowCount(0);if(!session_.signedIn())return;const auto& a=session_.data().account(session_.user(),session_.service());
    for(const auto& [id,u]:session_.data().users()){
        if(id==session_.user())continue;const auto found=a.friends().find(id);const bool friendNow=found!=a.friends().end();const bool blocked=a.blocked().count(id)!=0;
        if(filter_->currentIndex()==0&&!friendNow)continue;if(filter_->currentIndex()==1&&!blocked)continue;
        QStringList services;for(const auto& [type,acct]:u->services())services<<S::q(serviceName(type));
        QString nick=S::q(u->name()),category=blocked?"已拉黑":friendNow?"已添加":"未添加";
        if(friendNow){if(!found->second.remark().empty())nick+=" / "+S::q(found->second.remark());if(!found->second.category().empty())category=S::q(found->second.category());}
        QStringList values{QString::number(id),nick,S::q(u->location()),category,friendNow?S::q(serviceName(found->second.contact())):services.join(" · ")};
        if(!search_->text().isEmpty()&&!values.mid(1).join(" ").contains(search_->text(),Qt::CaseInsensitive))continue;UiHsx::row(table_,values,id);
    }
    UiHsx::select(table_,selected);summary_->setText(QString("%1 位好友 · %2 位黑名单用户 · 当前显示 %3 人").arg(a.friends().size()).arg(a.blocked().size()).arg(table_->rowCount()));
}
// Search-only: nobody is listed until the full nickname or service number is typed.
void FriendsPageHsx::addFriend(){
    QDialog dialog(this);dialog.setObjectName("addFriendDialog");dialog.setWindowTitle("听澜 · 添加好友");dialog.setMinimumWidth(460);auto* l=new QVBoxLayout(&dialog);l->setContentsMargins(24,22,24,20);l->setSpacing(12);
    auto* title=new QLabel("添加好友");title->setObjectName("sectionTitle");l->addWidget(title);
    auto* hint=new QLabel("输入对方完整的昵称或服务号码查找。为保护隐私，听澜不展示全部用户。");hint->setObjectName("muted");hint->setWordWrap(true);l->addWidget(hint);
    auto* row=new QHBoxLayout;auto* query=new QLineEdit;query->setObjectName("friendSearch");query->setPlaceholderText("完整昵称或服务号码");query->setClearButtonEnabled(true);row->addWidget(query,1);
    auto* find=new QPushButton("查找");find->setObjectName("searchFriend");find->setProperty("primary",true);row->addWidget(find);l->addLayout(row);
    auto* results=new QListWidget;results->setObjectName("friendResults");results->setMinimumHeight(120);l->addWidget(results);
    auto* status=new QLabel;status->setObjectName("muted");status->setWordWrap(true);l->addWidget(status);
    auto* form=new QFormLayout;auto* service=new QComboBox;service->setObjectName("friendService");form->addRow("对方服务",service);
    auto* reason=new QLineEdit("你好，希望添加你为好友");reason->setObjectName("friendReason");reason->setMaxLength(100);form->addRow("申请说明",reason);l->addLayout(form);
    auto* buttons=new QHBoxLayout;buttons->addStretch();auto* cancel=new QPushButton("取消");buttons->addWidget(cancel);auto* send=new QPushButton("发送申请");send->setObjectName("sendFriendRequest");send->setProperty("primary",true);send->setEnabled(false);buttons->addWidget(send);l->addLayout(buttons);
    QString request;
    auto pick=[results,service,send]{service->clear();const auto* item=results->currentItem();send->setEnabled(item);if(!item)return;
        for(const auto& v:item->data(Qt::UserRole+1).toJsonArray()){const auto s=v.toObject();service->addItem(S::q(serviceName(serviceType(s["type"].toInt())))+(s["nick"].toString().isEmpty()?QString{}:" · "+s["nick"].toString()),s["type"].toInt());}};
    connect(results,&QListWidget::currentRowChanged,&dialog,pick);
    connect(find,&QPushButton::clicked,&dialog,[&]{const auto term=query->text().trimmed();if(term.isEmpty()){status->setText("请输入完整的昵称或服务号码");return;}results->clear();status->setText("正在查找…");find->setEnabled(false);request=session_.searchUsers(term);if(request.isEmpty())find->setEnabled(true);});
    connect(query,&QLineEdit::returnPressed,find,&QPushButton::click);
    connect(&session_,&AppSessionHsx::searchFailed,&dialog,[&](const QString& id,const QString& detail){if(id!=request)return;find->setEnabled(true);status->setText(detail);});
    connect(&session_,&AppSessionHsx::usersFound,&dialog,[&](const QString& id,const QJsonArray& users){
        if(id!=request)return;find->setEnabled(true);results->clear();const auto& mine=session_.data().account(session_.user(),session_.service());
        for(const auto& v:users){const auto u=v.toObject();const int uid=u["id"].toInt();const bool already=mine.friends().count(uid)!=0;
            auto* item=new QListWidgetItem(u["name"].toString()+(already?"  ·  已是好友":""),results);item->setData(Qt::UserRole,uid);item->setData(Qt::UserRole+1,u["services"].toArray());if(already)item->setFlags(item->flags()&~Qt::ItemIsEnabled);}
        status->setText(users.isEmpty()?"没有找到。请确认对方的完整昵称或服务号码。":QString("找到 %1 位用户，选择后发送申请").arg(users.size()));
        for(int i=0;i<results->count();++i)if(results->item(i)->flags()&Qt::ItemIsEnabled){results->setCurrentRow(i);break;}});
    connect(cancel,&QPushButton::clicked,&dialog,&QDialog::reject);
    connect(send,&QPushButton::clicked,&dialog,[&]{const auto* item=results->currentItem();if(!item||service->currentIndex()<0)return;const int target=item->data(Qt::UserRole).toInt();const int contact=service->currentData().toInt();const auto text=reason->text();
        if(session_.command("friend.request",{{"target",target},{"contact",contact},{"reason",text}},[&]{session_.friends().request(session_.user(),session_.service(),target,static_cast<ServiceHsx>(contact),S::s(text));},"好友申请已发送，可在通知中查看"))dialog.accept();});
    dialog.exec();
}
void FriendsPageHsx::editFriend(){const int id=static_cast<int>(UiHsx::selected(table_));if(!id)return;const auto& a=session_.data().account(session_.user(),session_.service());auto it=a.friends().find(id);if(it==a.friends().end()){QMessageBox::information(this,"编辑备注","请先添加这位用户为好友。");return;}
    FormDialogHsx f("编辑好友",this);f.text("remark","备注",S::q(it->second.remark()));f.text("category","分组",S::q(it->second.category()));f.submit([&](const FormDialogHsx& form){return session_.command("friend.edit",{{"target",id},{"remark",form.textValue("remark")},{"category",form.textValue("category")}},[&]{session_.friends().edit(session_.user(),session_.service(),id,S::s(form.textValue("remark")),S::s(form.textValue("category")));},"好友信息已更新");});f.exec();
}
void FriendsPageHsx::block(bool on){const int id=static_cast<int>(UiHsx::selected(table_));if(id&&(!on||UiHsx::confirm(this,"将该用户加入黑名单？已有好友关系会被删除。")))session_.command("friend.block",{{"target",id},{"enabled",on}},[&]{session_.friends().block(session_.user(),session_.service(),id,on);},on?"用户已拉黑":"已取消拉黑");}
void FriendsPageHsx::compare(bool common){
    FormDialogHsx f(common?"跨服务共同好友":"跨服务好友推荐",this);const auto choices=UiHsx::services(session_.data().user(session_.user()));f.combo("from","来源服务",choices);f.combo("to",common?"比较服务":"推荐到服务",choices);QList<int> results;
    f.submit([&](const FormDialogHsx& form){try{const auto from=static_cast<ServiceHsx>(form.choice("from")),to=static_cast<ServiceHsx>(form.choice("to"));const auto ids=common?session_.friends().common(session_.user(),from,to):session_.friends().recommend(session_.user(),to,from);results.clear();for(int id:ids)results<<id;return true;}catch(const std::exception& e){QMessageBox::warning(&f,"操作失败",S::q(e.what()));return false;}});
    if(f.exec()!=QDialog::Accepted)return;QStringList names;for(int id:results)names<<S::q(session_.data().user(id).name());QMessageBox::information(this,common?"共同好友":"推荐好友",names.isEmpty()?"暂无符合条件的好友":names.join("\n"));
}
}
