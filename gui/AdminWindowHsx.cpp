#include "AdminWindowHsx.h"
#include "FormDialogHsx.h"
#include "ThemeHsx.h"
#include "UiHsx.h"
#include "MembershipAdminHsx.h"
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>
namespace im {
using S = AppSessionHsx;
AdminWindowHsx::AdminWindowHsx(AppSessionHsx& session, QWidget* parent) : QMainWindow(parent), session_(session) {
    require(session_.adminSignedIn(),"需要服务器管理员登录");
    ThemeHsx::install(*qApp);
    setWindowTitle("听澜 · 系统管理后台"); resize(1260,820); setMinimumSize(1100,720);
    auto* area = new QWidget; setCentralWidget(area); auto* outer = new QVBoxLayout(area); outer->setContentsMargins(0,0,0,0); outer->setSpacing(0);
    auto* header = new QFrame; header->setObjectName("header"); auto* h = new QHBoxLayout(header); h->setContentsMargins(28,20,28,20);
    h->addWidget(ThemeHsx::avatar("管",0,44)); auto* heading = new QVBoxLayout;
    auto* title = new QLabel("系统管理后台"); title->setObjectName("pageTitle"); heading->addWidget(title);
    auto* caption = new QLabel("账号权限、会话状态与密码管理"); caption->setObjectName("muted"); heading->addWidget(caption); h->addLayout(heading,1);
    h->addWidget(new QLabel("管理员  " + S::q(session_.admins().name())));
    auto* password = UiHsx::button(h,"修改管理员密码",[this]{changePassword();}); password->setObjectName("changeAdminPassword");
    auto* exit = UiHsx::button(h,"退出管理 / 返回登录",[this]{emit returnToLogin();}); exit->setObjectName("adminLogout"); outer->addWidget(header);
    auto* body = new QWidget; auto* b = new QVBoxLayout(body); b->setContentsMargins(24,22,24,22); b->setSpacing(18);
    auto* cards = new QHBoxLayout; const QStringList labels{"注册用户","允许登录","禁止登录","当前在线"};
    for (int i=0;i<4;++i) { auto* frame = new QFrame; frame->setObjectName("card"); auto* c = new QVBoxLayout(frame); c->setContentsMargins(22,18,22,18);
        auto* name = new QLabel(labels[i]); name->setObjectName("muted"); c->addWidget(name); counts_[i] = new QLabel;counts_[i]->setObjectName(QString("adminCount%1").arg(i));
        counts_[i]->setStyleSheet("font-size:30px;font-weight:700;color:#4f7553;"); c->addWidget(counts_[i]); cards->addWidget(frame); }
    b->addLayout(cards);
    auto* tools = new QHBoxLayout; search_ = new QLineEdit; search_->setObjectName("adminSearch"); search_->setPlaceholderText("搜索用户 ID、姓名、服务号或日志操作"); search_->setClearButtonEnabled(true); tools->addWidget(search_,1);
    filter_ = new QComboBox; filter_->addItems({"全部用户","允许登录","禁止登录","当前在线"}); tools->addWidget(filter_); b->addLayout(tools);
    auto* frame = new QFrame; frame->setObjectName("card"); auto* c = new QVBoxLayout(frame);
    auto* tabs = new QTabWidget; tabs->setObjectName("adminTabs"); auto* userPage = new QWidget; auto* u = new QVBoxLayout(userPage);
    users_ = UiHsx::table({"用户 ID","姓名","地区","已开通服务","登录权限","会话状态"}); users_->setObjectName("adminUsers"); u->addWidget(users_,1);
    details_ = new QLabel("选择用户查看服务信息"); details_->setWordWrap(true); details_->setObjectName("muted"); u->addWidget(details_);
    auto* actions = new QHBoxLayout;
    UiHsx::button(actions,"重置用户密码",[this]{resetPassword();},true)->setObjectName("resetUserPassword");
    UiHsx::button(actions,"允许登录",[this]{setAllowed(true);})->setObjectName("allowUserLogin");
    UiHsx::button(actions,"禁止登录",[this]{setAllowed(false);})->setObjectName("denyUserLogin");
    UiHsx::button(actions,"强制登出",[this]{const int id=static_cast<int>(UiHsx::selected(users_)); if(id&&UiHsx::confirm(this,QString("强制登出用户 %1？").arg(id))) session_.command("admin.logout",{{"target",id}},[&]{session_.admins().forceLogout(id);},"用户已登出");})->setObjectName("forceUserLogout");
    actions->addStretch(); u->addLayout(actions); tabs->addTab(userPage,"用户管理");
    auto* logPage = new QWidget; auto* l = new QVBoxLayout(logPage);logFilter_=new QComboBox;logFilter_->setObjectName("adminLogFilter");logFilter_->addItems({"全部记录","用户登录 / 登出","管理员操作"});l->addWidget(logFilter_);
    logs_ = UiHsx::table({"时间","昵称 / 操作人","登录状态 / 操作","用户 ID","服务"}); logs_->setObjectName("adminLogs"); l->addWidget(logs_,1);
    logInfo_ = new QLabel; logInfo_->setObjectName("muted"); l->addWidget(logInfo_); tabs->addTab(logPage,"用户登录与操作记录");membership_=new MembershipAdminHsx(session_);tabs->addTab(membership_,"充值订单核验"); c->addWidget(tabs); b->addWidget(frame,1);
    auto* note = new QLabel(session_.remote()?"正在管理中心服务器：注册、登录／断线及会员申请实时更新。禁止登录、重置密码或强制登出会结束用户的真实连接。":"当前管理本机离线存档。"); note->setObjectName("muted"); note->setWordWrap(true); b->addWidget(note); outer->addWidget(body,1);
    statusBar()->showMessage("管理员模式 · 操作经服务器确认后保存");auto* connection=new QLabel(session_.remote()?session_.connectionStatus():"离线管理");connection->setObjectName("adminConnectionState");statusBar()->addPermanentWidget(connection);connect(&session_,&S::connectionChanged,connection,&QLabel::setText);
    connect(users_,&QTableWidget::itemSelectionChanged,this,[this]{details();});
    connect(search_,&QLineEdit::textChanged,this,[this]{refresh();}); connect(filter_,&QComboBox::currentIndexChanged,this,[this]{refresh();});
    connect(logFilter_,&QComboBox::currentIndexChanged,this,[this]{refresh();});
    connect(&session_,&S::changed,this,[this]{refresh();});
    connect(&session_,&S::failed,this,[this](const QString& message){QMessageBox::warning(this,"操作未完成",message);});
    connect(&session_,&S::succeeded,this,[this](const QString& message){statusBar()->showMessage(message,6000);});
    refresh();
}
void AdminWindowHsx::refresh() {
    if (!session_.adminSignedIn()) { centralWidget()->setEnabled(false); return; }
    membership_->refresh();
    const auto selected = UiHsx::selected(users_); QSignalBlocker guard(users_); users_->setRowCount(0); int allowed=0,online=0;
    for (const auto& [id,user] : session_.data().users()) {
        if (user->loginAllowed()) ++allowed; if (user->online()) ++online;
        if ((filter_->currentIndex()==1&&!user->loginAllowed()) || (filter_->currentIndex()==2&&user->loginAllowed()) || (filter_->currentIndex()==3&&!user->online())) continue;
        QStringList names,numbers; for (const auto& [type,account] : user->services()) { names<<S::q(serviceName(type)); numbers<<QString::number(account.number()); }
        const QString searchable=QString::number(id)+" "+S::q(user->name())+" "+S::q(user->location())+" "+names.join(" ")+" "+numbers.join(" ");
        if (!search_->text().isEmpty()&&!searchable.contains(search_->text(),Qt::CaseInsensitive)) continue;
        UiHsx::row(users_,{QString::number(id),S::q(user->name()),S::q(user->location()),names.join(" · "),user->loginAllowed()?"允许登录":"禁止登录",user->online()?"在线":"离线"},id);
    }
    UiHsx::select(users_,selected); if(users_->currentRow()<0&&users_->rowCount())users_->selectRow(0); details();
    const int total=static_cast<int>(session_.data().users().size()); const int values[]{total,allowed,total-allowed,online}; for(int i=0;i<4;++i)counts_[i]->setText(QString::number(values[i]));
    logs_->setRowCount(0); int shown=0;
    for(auto it=session_.data().adminLogs().rbegin();it!=session_.data().adminLogs().rend()&&shown<500;++it) {
        const auto& log=it->second;const bool userLog=log.actor().rfind("用户:",0)==0;
        if((logFilter_->currentIndex()==1&&!userLog)||(logFilter_->currentIndex()==2&&userLog))continue;
        const QString actor=userLog&&log.target()?S::q(session_.data().user(log.target()).name()):S::q(log.actor());
        const QStringList rowValues{S::q(timeText(log.created())),actor,S::q(log.action()),log.target()?QString::number(log.target()):"—",log.service()==ServiceHsx::Count?"全部 / —":S::q(serviceName(log.service()))};
        if(!search_->text().isEmpty()&&!rowValues.join(" ").contains(search_->text(),Qt::CaseInsensitive))continue;
        UiHsx::row(logs_,rowValues,log.id()); ++shown;
    }
    logInfo_->setText(QString("当前显示 %1 条 · 最多显示最近500条 · 存档保留最近2000条 · 日志不记录密码").arg(shown));
}
void AdminWindowHsx::details() {
    const int id=static_cast<int>(UiHsx::selected(users_)); if(!id){details_->setText("选择用户查看服务信息");return;}
    QStringList services; for(const auto& [type,account]:session_.data().user(id).services()) services<<QString("%1：%2").arg(S::q(serviceName(type))).arg(account.number());
    details_->setText(QString("用户 %1 · ").arg(id)+services.join("  /  "));
}
void AdminWindowHsx::setAllowed(bool allowed) {
    const int id=static_cast<int>(UiHsx::selected(users_)); if(!id)return;
    if(!allowed&&!UiHsx::confirm(this,QString("禁止用户 %1 登录，并立即登出其当前会话？").arg(id)))return;
    session_.command("admin.allow",{{"target",id},{"enabled",allowed}},[&]{session_.admins().setLoginAllowed(id,allowed);},allowed?"已允许该用户登录":"已禁止该用户登录并登出");
}
void AdminWindowHsx::resetPassword() {
    const int id=static_cast<int>(UiHsx::selected(users_)); if(!id)return;
    FormDialogHsx form(QString("重置用户 %1 的密码").arg(id),this);
    QList<QPair<QString,QVariant>> services{{"全部已开通服务",static_cast<int>(ServiceHsx::Count)}}; services.append(UiHsx::services(session_.data().user(id)));
    form.combo("service","重置范围",services); form.text("new","新密码",{},true); form.text("repeat","确认新密码",{},true);
    form.submit([&](const FormDialogHsx& f){require(f.textValue("new")==f.textValue("repeat"),"两次新密码不一致");if(session_.remote())require(f.textValue("new").toUtf8().size()>=8,"新密码至少 8 字节");return session_.command("admin.reset",{{"target",id},{"scope",f.choice("service")},{"password",f.textValue("new")}},[&]{session_.admins().resetPassword(id,static_cast<ServiceHsx>(f.choice("service")),S::s(f.textValue("new")));},"密码已重置，该用户需重新登录");}); form.exec();
}
void AdminWindowHsx::changePassword() {
    FormDialogHsx form("修改管理员密码",this); form.text("old","原密码",{},true); form.text("new","新密码（至少8个字符）",{},true); form.text("repeat","确认新密码",{},true);
    form.submit([&](const FormDialogHsx& f){require(f.textValue("new").size()>=8,"管理员密码至少8个字符");require(f.textValue("new")==f.textValue("repeat"),"两次新密码不一致");return session_.command("admin.password",{{"old",f.textValue("old")},{"password",f.textValue("new")}},[&]{session_.admins().changePassword(S::s(f.textValue("old")),S::s(f.textValue("new")));},"管理员密码已更新");}); form.exec();
}
void AdminWindowHsx::closeEvent(QCloseEvent* event) {
    if(!session_.save()&&!UiHsx::confirm(this,"保存失败，仍要关闭并放弃尚未保存的修改？")){event->ignore();return;} event->accept();
}
}
