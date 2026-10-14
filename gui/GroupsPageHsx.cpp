#include "GroupsPageHsx.h"
#include "FormDialogHsx.h"
#include "UiHsx.h"
#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
namespace im {
using S=AppSessionHsx;
GroupsPageHsx::GroupsPageHsx(AppSessionHsx& s,QWidget* parent):QWidget(parent),session_(s){
    auto* outer=new QHBoxLayout(this);outer->setContentsMargins(0,0,0,0);outer->setSpacing(18);auto* left=new QFrame;left->setObjectName("card");left->setFixedWidth(274);auto* l=new QVBoxLayout(left);l->setContentsMargins(16,22,16,18);l->setSpacing(12);
    auto* heading=new QLabel("群组空间");heading->setObjectName("sectionTitle");l->addWidget(heading);search_=new QLineEdit;search_->setPlaceholderText("搜索群名或群号");search_->setClearButtonEnabled(true);l->addWidget(search_);filter_=new QComboBox;filter_->addItems({"我的群组","全部群组"});l->addWidget(filter_);list_=new QListWidget;list_->setObjectName("groupsList");l->addWidget(list_,1);auto* createRow=new QHBoxLayout;UiHsx::button(createRow,"创建群组",[this]{create(false);},true);l->addLayout(createRow);outer->addWidget(left);
    auto* right=new QFrame;right->setObjectName("card");auto* r=new QVBoxLayout(right);r->setContentsMargins(22,22,22,20);r->setSpacing(12);title_=new QLabel("选择一个群组");title_->setObjectName("sectionTitle");r->addWidget(title_);description_=new QLabel;description_->setWordWrap(true);description_->setObjectName("muted");r->addWidget(description_);
    auto* quick=new QHBoxLayout;UiHsx::button(quick,"进入群聊",[this]{if(selected())emit openChat(selected());},true);
    UiHsx::button(quick,"申请加入",[this]{const int id=selected();if(id)session_.command("group.apply",{{"group",id}},[&]{session_.groups().apply(session_.user(),session_.service(),id);},"已提交入群申请");});
    UiHsx::button(quick,"邀请成员",[this]{invite();});UiHsx::button(quick,"编辑群信息",[this]{edit();});quick->addStretch();r->addLayout(quick);
    members_=UiHsx::table({"内部编号","成员","角色","加入服务","状态"});members_->hideColumn(0);members_->setObjectName("groupMembers");r->addWidget(members_,1);
    auto* memberRow=new QHBoxLayout;operations_=new QComboBox;operations_->addItems({"设置管理员","取消管理员","禁言成员","解除禁言","移出成员","转让群主"});memberRow->addWidget(operations_);UiHsx::button(memberRow,"执行成员操作",[this]{memberAction(operations_->currentIndex());});memberRow->addStretch();r->addLayout(memberRow);
    auto* more=new QHBoxLayout;UiHsx::button(more,"创建子群",[this]{create(true);});UiHsx::button(more,"切换管理模式",[this]{changeMode();});
    UiHsx::button(more,"撤销入群申请",[this]{const int id=selected();if(id)session_.command("group.cancel",{{"group",id}},[&]{session_.groups().cancelApplication(session_.user(),session_.service(),id);},"申请已撤销");});
    UiHsx::button(more,"退出群组",[this]{const int id=selected();if(id&&UiHsx::confirm(this,"退出当前服务加入的群组？"))session_.command("group.leave",{{"group",id}},[&]{session_.groups().leave(session_.user(),session_.service(),id);},"已退出群组");});
    UiHsx::button(more,"解散群组",[this]{const int id=selected();if(id&&UiHsx::confirm(this,"解散此群及其子群？此操作会清除群组与对应群消息。"))session_.command("group.disband",{{"group",id}},[&]{session_.groups().disband(session_.user(),session_.service(),id);},"群组已解散");});r->addLayout(more);outer->addWidget(right,1);
    connect(list_,&QListWidget::currentItemChanged,this,[this]{details();});connect(search_,&QLineEdit::textChanged,this,[this]{refresh();});connect(filter_,&QComboBox::currentIndexChanged,this,[this]{refresh();});
}
int GroupsPageHsx::selected()const{return list_->currentItem()?list_->currentItem()->data(Qt::UserRole).toInt():0;}
void GroupsPageHsx::refresh(){const int current=selected();QSignalBlocker guard(list_);list_->clear();if(!session_.signedIn())return;
    for(const auto& [id,g]:session_.data().groups()){
        if(filter_->currentIndex()==0&&!g->contains(session_.user(),session_.service()))continue;
        const QString name=S::q(g->name()),code=QString::number(id);if(!search_->text().isEmpty()&&!(name+code).contains(search_->text(),Qt::CaseInsensitive))continue;
        auto* item=new QListWidgetItem(name+QString("\n%1 · %2人 · %3").arg(id).arg(g->members().size()).arg(S::q(modeName(g->policy().mode()))),list_);item->setData(Qt::UserRole,id);item->setSizeHint(QSize(240,74));if(id==current)list_->setCurrentItem(item);
    }
    if(!list_->currentItem()&&list_->count())list_->setCurrentRow(0);details();
}
void GroupsPageHsx::details(){members_->setRowCount(0);const int id=selected();if(!id){title_->setText("选择一个群组");description_->setText("群聊、邀请、管理员与子群都在这里");return;}const auto& g=session_.data().group(id);title_->setText(S::q(g.name()));description_->setText(QString("群号 %1 · 群主 %2 · %3/%4 人 · %5\n公告：%6%7").arg(id).arg(S::q(session_.data().user(g.owner()).name())).arg(g.members().size()).arg(g.capacity()).arg(S::q(g.policy().description())).arg(g.announcement().empty()?"暂未设置":S::q(g.announcement())).arg(g.parent()?QString("\n父群：%1").arg(g.parent()):QString{}));
    for(const auto& [uid,services]:g.members()){QStringList names;for(auto type:services)names<<S::q(serviceName(type));UiHsx::row(members_,{QString::number(uid),S::q(session_.data().user(uid).name()),uid==g.owner()?"群主":g.admins().count(uid)?"管理员":"成员",names.join(" · "),g.muted(uid)?"禁言中":"正常"},uid);}
}
void GroupsPageHsx::create(bool subgroup){const int parent=subgroup?selected():0;if(subgroup&&!parent)return;FormDialogHsx f(subgroup?"创建子群":"创建群组",this);f.text("name","群名称");f.combo("mode","管理模式",{{"QQ · 申请审核 / 管理员 / 子群",0},{"微信 · 邀请入群 / 群主管理",1}});f.submit([&](const FormDialogHsx& form){return session_.command("group.create",{{"name",form.textValue("name")},{"mode",form.choice("mode")},{"parent",parent}},[&]{session_.groups().create(session_.user(),session_.service(),S::s(form.textValue("name")),static_cast<GroupModeHsx>(form.choice("mode")),parent);},"群组已创建");});f.exec();}
void GroupsPageHsx::invite(){const int gid=selected();if(!gid)return;FormDialogHsx f("邀请成员",this);f.combo("target","成员昵称",UiHsx::users(session_.data(),session_.user()));f.combo("service","成员服务",UiHsx::allServices(),static_cast<int>(session_.service()));f.submit([&](const FormDialogHsx& form){return session_.command("group.invite",{{"group",gid},{"target",form.choice("target")},{"contact",form.choice("service")}},[&]{session_.groups().invite(session_.user(),session_.service(),gid,form.choice("target"),static_cast<ServiceHsx>(form.choice("service")));},"成员已加入群组");});f.exec();}
void GroupsPageHsx::edit(){const int gid=selected();if(!gid)return;const auto& g=session_.data().group(gid);FormDialogHsx f("编辑群组",this);f.text("name","群名称",S::q(g.name()));f.text("notice","群公告",S::q(g.announcement()));f.number("capacity","人数上限",1,5000,g.capacity());f.submit([&](const FormDialogHsx& form){return session_.command("group.edit",{{"group",gid},{"name",form.textValue("name")},{"notice",form.textValue("notice")},{"capacity",form.numberValue("capacity")}},[&]{session_.groups().edit(session_.user(),session_.service(),gid,S::s(form.textValue("name")),S::s(form.textValue("notice")),form.numberValue("capacity"));},"群信息已更新");});f.exec();}
void GroupsPageHsx::memberAction(int action){const int gid=selected(),target=static_cast<int>(UiHsx::selected(members_));if(!gid||!target)return;
    if(action==2){FormDialogHsx f("禁言成员",this);f.number("minutes","禁言分钟",1,1440,10);f.submit([&](const FormDialogHsx& form){return session_.command("group.mute",{{"group",gid},{"target",target},{"minutes",form.numberValue("minutes")}},[&]{session_.groups().mute(session_.user(),session_.service(),gid,target,form.numberValue("minutes"));},"成员已禁言");});f.exec();return;}
    if((action==4||action==5)&&!UiHsx::confirm(this,action==4?"将此成员移出群组？":"将群主权限转让给此成员？"))return;
    if(session_.remote()){const QStringList operations{"group.admin","group.admin","group.mute","group.mute","group.kick","group.transfer"};session_.command(operations.at(action),{{"group",gid},{"target",target},{"enabled",action==0},{"minutes",0}},{},"成员操作已完成");return;}
    session_.mutate([&]{switch(action){case 0:case 1:session_.groups().admin(session_.user(),session_.service(),gid,target,action==0);break;case 3:session_.groups().mute(session_.user(),session_.service(),gid,target,0);break;case 4:session_.groups().kick(session_.user(),session_.service(),gid,target);break;case 5:session_.groups().transfer(session_.user(),session_.service(),gid,target);break;}},"成员操作已完成");
}
void GroupsPageHsx::changeMode(){const int gid=selected();if(!gid)return;FormDialogHsx f("切换群管理模式",this);f.combo("mode","目标模式",{{"QQ 管理模式",0},{"微信管理模式",1}},static_cast<int>(session_.data().group(gid).policy().mode()));f.submit([&](const FormDialogHsx& form){return session_.command("group.mode",{{"group",gid},{"mode",form.choice("mode")}},[&]{session_.groups().mode(session_.user(),session_.service(),gid,static_cast<GroupModeHsx>(form.choice("mode")));},"管理模式已切换，原成员已保留");});f.exec();}
}
