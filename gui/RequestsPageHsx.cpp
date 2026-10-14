#include "RequestsPageHsx.h"
#include "UiHsx.h"
#include <QFrame>
#include <QLabel>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
namespace im {
using S=AppSessionHsx;
RequestsPageHsx::RequestsPageHsx(AppSessionHsx& s,QWidget* parent):QWidget(parent),session_(s){auto* layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);auto* frame=new QFrame;frame->setObjectName("card");auto* c=new QVBoxLayout(frame);c->setContentsMargins(22,22,22,20);c->setSpacing(14);
    auto* title=new QLabel("申请中心");title->setObjectName("sectionTitle");c->addWidget(title);auto* hint=new QLabel("在当前服务处理好友申请、入群申请，也可撤销自己发出的申请。");hint->setObjectName("muted");c->addWidget(hint);
    tabs_=new QTabWidget;received_=UiHsx::table({"申请号","申请人","来源服务","申请说明","状态"});sent_=UiHsx::table({"申请号","目标用户","目标服务","申请说明","状态"});groups_=UiHsx::table({"群组","申请人","服务","类型","状态"});received_->setObjectName("receivedRequests");groups_->setObjectName("groupRequests");tabs_->addTab(received_,"收到的好友申请");tabs_->addTab(sent_,"发出的好友申请");tabs_->addTab(groups_,"入群申请");c->addWidget(tabs_,1);
    auto* actions=new QHBoxLayout;UiHsx::button(actions,"同意申请",[this]{respond(true);},true);UiHsx::button(actions,"拒绝申请",[this]{respond(false);});UiHsx::button(actions,"撤销自己的申请",[this]{cancel();});actions->addStretch();c->addLayout(actions);layout->addWidget(frame,1);
}
void RequestsPageHsx::refresh(){received_->setRowCount(0);sent_->setRowCount(0);groups_->setRowCount(0);if(!session_.signedIn())return;const int actor=session_.user();const auto service=session_.service();const QStringList status{"待处理","已同意","已拒绝","已撤销"};
    for(const auto& [id,r]:session_.data().requests()){
        auto add=[&](QTableWidget* t,int target,ServiceHsx type){UiHsx::row(t,{QString::number(id),S::q(session_.data().user(target).name()),S::q(serviceName(type)),S::q(r.reason()),status.at(static_cast<int>(r.status()))},id);};
        if(r.to()==actor&&r.destination()==service)add(received_,r.from(),r.source());if(r.from()==actor&&r.source()==service)add(sent_,r.to(),r.destination());
    }
    for(const auto& [id,g]:session_.data().groups())for(const auto& [user,type]:g->pending()){
        const bool own=user==actor&&type==service;const bool manages=g->contains(actor,service)&&g->manages(actor)&&g->policy().applications();if(!own&&!manages)continue;
        UiHsx::row(groups_,{QString("%1 · %2").arg(id).arg(S::q(g->name())),S::q(session_.data().user(user).name()),S::q(serviceName(type)),own?"我发出的":"等待我审批","待处理"},id,user);auto* item=groups_->item(groups_->rowCount()-1,0);item->setData(Qt::UserRole+2,static_cast<int>(type));item->setData(Qt::UserRole+3,own);
    }
}
void RequestsPageHsx::respond(bool accept){if(tabs_->currentIndex()==0){const auto id=UiHsx::selected(received_);if(id)session_.command("friend.respond",{{"message",QString::number(id)},{"enabled",accept}},[&]{session_.friends().respond(session_.user(),session_.service(),id,accept);},accept?"好友申请已同意":"好友申请已拒绝");}
    else if(tabs_->currentIndex()==2){const int gid=static_cast<int>(UiHsx::selected(groups_)),user=UiHsx::extra(groups_);if(!gid)return;const auto type=static_cast<ServiceHsx>(groups_->item(groups_->currentRow(),0)->data(Qt::UserRole+2).toInt());session_.command("group.approve",{{"group",gid},{"target",user},{"contact",int(type)},{"enabled",accept}},[&]{session_.groups().approve(session_.user(),session_.service(),gid,user,type,accept);},accept?"成员已加入":"入群申请已拒绝");}}
void RequestsPageHsx::cancel(){if(tabs_->currentIndex()==1){const auto id=UiHsx::selected(sent_);if(id)session_.command("friend.cancel",{{"message",QString::number(id)}},[&]{session_.friends().cancel(session_.user(),id);},"好友申请已撤销");}else if(tabs_->currentIndex()==2){const int gid=static_cast<int>(UiHsx::selected(groups_));if(!gid)return;const auto* item=groups_->item(groups_->currentRow(),0);if(!item->data(Qt::UserRole+3).toBool())return;session_.command("group.cancel",{{"group",gid}},[&]{session_.groups().cancelApplication(session_.user(),session_.service(),gid);},"入群申请已撤销");}}
}
