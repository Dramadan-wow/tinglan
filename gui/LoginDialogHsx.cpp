#include "LoginDialogHsx.h"
#include "AppSessionHsx.h"
#include "FormDialogHsx.h"
#include "LandscapeHsx.h"
#include "PasswordHsx.h"
#include "LoginMusicHsx.h"
#include "IconHsx.h"
#include "ThemeHsx.h"
#include "UpdateManagerHsx.h"
#include <QApplication>
#include <QAction>
#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
namespace im {
LoginDialogHsx::LoginDialogHsx(AppSessionHsx& session,QWidget* parent,bool admin,std::function<bool()> settings):QDialog(parent),session_(session) {
    ThemeHsx::install(*qApp);
    setWindowTitle("听澜 · 山水有相逢");setFixedSize(1120,760);setObjectName("loginDialog");
    setWindowIcon(IconHsx::get("brand","#356552"));
    setStyleSheet(R"(
        QDialog#loginDialog {background:#fbfcf7;}
        QDialog#loginDialog QLineEdit,QDialog#loginDialog QComboBox {background:#f6f8f1;border:1px solid #e2e8dc;border-radius:10px;min-height:23px;padding:12px;color:#334d3c;font-size:13px;}
        QDialog#loginDialog QLineEdit:focus,QDialog#loginDialog QComboBox:focus {background:#ffffff;border-color:#87a88d;}
        QFrame#roleSelector {background:#eff3e9;border:1px solid #e6ecdd;border-radius:11px;}
        QPushButton#roleButton {background:transparent;border:1px solid transparent;border-radius:8px;color:#8d9b83;padding:10px 14px;}
        QPushButton#roleButton:checked {background:#ffffff;border:1px solid #dce5d2;color:#355c3d;font-weight:600;}
        QPushButton#loginSubmit {background:#345f44;border:1px solid #345f44;border-radius:11px;color:#ffffff;font-size:14px;padding:13px;font-weight:600;}
        QPushButton#loginSubmit:hover {background:#264b33;}
        QPushButton#registerAccount {background:transparent;border:none;color:#718769;padding:9px;font-size:12px;}
        QLabel#fieldLabel {color:#566b4e;font-size:11px;font-weight:600;}
    )");
    auto* outer=new QHBoxLayout(this);outer->setContentsMargins(16,16,16,16);outer->setSpacing(0);
    auto* cover=new LandscapeHsx;cover->setFixedWidth(512);auto* left=new QVBoxLayout(cover);left->setContentsMargins(36,34,36,26);left->setSpacing(12);
    auto* brandRow=new QHBoxLayout;auto* emblem=new QLabel;emblem->setPixmap(IconHsx::get("brand","#38543f").pixmap(32,32));brandRow->addWidget(emblem);
    auto* brand=new QLabel("听澜");brand->setObjectName("loginBrand");brand->setStyleSheet("font-family:'SimSun';color:#314933;font-size:30px;letter-spacing:4px;");brandRow->addWidget(brand);brandRow->addStretch();
    auto* english=new QLabel("T I N G L A N");english->setStyleSheet("color:#7f8d73;font-size:9px;letter-spacing:2px;");brandRow->addWidget(english);left->addLayout(brandRow);left->addSpacing(21);
    auto* poem=new QLabel("听风起，\n等你来。");poem->setStyleSheet("font-family:'SimSun';color:#314b37;font-size:39px;letter-spacing:3px;");poem->setMinimumHeight(105);left->addWidget(poem);
    auto* words=new QLabel("山水有相逢，言语有回响。");words->setStyleSheet("color:#7c876c;font-size:12px;letter-spacing:1px;");left->addWidget(words);left->addStretch();
    auto* music=new LoginMusicHsx;left->addWidget(music);connect(this,&QDialog::finished,music,[music]{music->stop();});outer->addWidget(cover);
    auto* body=new QWidget;auto* right=new QVBoxLayout(body);right->setContentsMargins(58,34,48,24);right->setSpacing(8);
    auto* overline=new QLabel("有 人 倾 听  /  有 处 心 安");overline->setStyleSheet("color:#9ca98b;font-size:10px;letter-spacing:1px;");right->addWidget(overline);right->addSpacing(6);
    title_=new QLabel;title_->setStyleSheet("font-size:29px;color:#2e472f;font-weight:600;");right->addWidget(title_);
    intro_=new QLabel;intro_->setWordWrap(true);intro_->setStyleSheet("font-size:12px;color:#8c9a7d;");right->addWidget(intro_);right->addSpacing(17);
    role_=new QComboBox(this);role_->setObjectName("loginRole");
#ifdef IM_USER_CLIENT
    role_->addItem("普通用户");
#else
    role_->addItems({"普通用户","系统管理员"});
#endif
    role_->hide();
    auto* selector=new QFrame;selector->setObjectName("roleSelector");auto* tabs=new QHBoxLayout(selector);tabs->setContentsMargins(4,4,4,4);tabs->setSpacing(4);
    auto* roles=new QButtonGroup(this);userRole_=new QPushButton("用户登录");
#ifdef IM_USER_CLIENT
    adminRole_=nullptr;
#else
    adminRole_=new QPushButton("管理空间");
#endif
    for(auto* button:{userRole_,adminRole_}){if(!button)continue;button->setCheckable(true);button->setObjectName("roleButton");roles->addButton(button);tabs->addWidget(button);}
    userRole_->setIcon(IconHsx::get("user","#8d9b83","#355c3d"));if(adminRole_)adminRole_->setIcon(IconHsx::get("shield","#8d9b83","#355c3d"));right->addWidget(selector);right->addSpacing(13);
    userLabel_=new QLabel;userLabel_->setObjectName("fieldLabel");right->addWidget(userLabel_);
    user_=new QLineEdit;user_->setObjectName("loginUser");user_->setMaxLength(60);user_->setClearButtonEnabled(true);user_->addAction(IconHsx::get("user","#9aa78c"),QLineEdit::LeadingPosition);right->addWidget(user_);right->addSpacing(7);
    auto* credentials=new QHBoxLayout;credentials->setSpacing(14);serviceBox_=new QWidget;serviceBox_->setFixedWidth(120);auto* svc=new QVBoxLayout(serviceBox_);svc->setContentsMargins(0,0,0,0);svc->setSpacing(8);
    auto* serviceLabel=new QLabel("登录服务");serviceLabel->setObjectName("fieldLabel");svc->addWidget(serviceLabel);service_=new QComboBox;service_->setObjectName("loginService");service_->setAccessibleName("登录服务");svc->addWidget(service_);credentials->addWidget(serviceBox_);
    auto* passwordBox=new QWidget;auto* secret=new QVBoxLayout(passwordBox);secret->setContentsMargins(0,0,0,0);secret->setSpacing(8);auto* passwordLabel=new QLabel("密码");passwordLabel->setObjectName("fieldLabel");secret->addWidget(passwordLabel);
    password_=new QLineEdit;password_->setObjectName("loginPassword");password_->setEchoMode(QLineEdit::Password);password_->setPlaceholderText("输入密码");password_->setAccessibleName("密码");
    reveal_=password_->addAction(IconHsx::get("eye","#98a68a"),QLineEdit::TrailingPosition);reveal_->setObjectName("revealPassword");reveal_->setCheckable(true);reveal_->setToolTip("显示密码");secret->addWidget(password_);credentials->addWidget(passwordBox,1);right->addLayout(credentials);right->addSpacing(8);
    linked_=new QCheckBox("联动登录本人已开通的其它服务");linked_->setChecked(true);right->addWidget(linked_);
    error_=new QLabel;error_->setObjectName("loginError");error_->setWordWrap(true);error_->setMinimumHeight(20);error_->setStyleSheet("color:#b46653;font-size:11px;");right->addWidget(error_);
    auto* submit=new QPushButton("登 录 听 澜");submit->setObjectName("loginSubmit");submit->setIcon(IconHsx::get("arrow","#ffffff"));submit->setLayoutDirection(Qt::RightToLeft);submit->setMinimumHeight(22);right->addWidget(submit);
    register_=new QPushButton("还未相识？  创建你的账号");register_->setObjectName("registerAccount");right->addWidget(register_);right->addStretch();
    if(settings){auto* connection=new QPushButton("服务器连接设置");connection->setObjectName("connectionSettings");connection->setIcon(IconHsx::get("settings","#718769"));right->addWidget(connection);connect(connection,&QPushButton::clicked,this,[this,settings,submit]{session_.logout();submit->setEnabled(true);register_->setEnabled(true);error_->clear();if(settings())done(2);});}
#ifdef IM_USER_CLIENT
    auto* updates=new QPushButton("软件更新");updates->setObjectName("softwareUpdates");right->addWidget(updates);connect(updates,&QPushButton::clicked,this,[]{UpdateManagerHsx::showUpdates();});
#endif
    hint_=new QLabel;hint_->setWordWrap(true);hint_->setStyleSheet("background:#f2f5ec;color:#8c9b7a;border:1px solid #e8edde;border-radius:10px;padding:12px;font-size:10px;");right->addWidget(hint_);
    auto* signature=new QLabel("听澜  ·  让每一次相逢都有回响");signature->setAlignment(Qt::AlignCenter);signature->setStyleSheet("color:#abb59b;font-size:9px;padding-top:8px;");right->addWidget(signature);outer->addWidget(body,1);
    connect(userRole_,&QPushButton::clicked,this,[this]{role_->setCurrentIndex(0);});if(adminRole_)connect(adminRole_,&QPushButton::clicked,this,[this]{role_->setCurrentIndex(1);});
    connect(reveal_,&QAction::toggled,this,[this](bool show){password_->setEchoMode(show?QLineEdit::Normal:QLineEdit::Password);reveal_->setIcon(IconHsx::get(show?"eye-off":"eye","#98a68a"));reveal_->setToolTip(show?"隐藏密码":"显示密码");});
    connect(user_,&QLineEdit::textChanged,this,[this]{services();error_->clear();});connect(submit,&QPushButton::clicked,this,[this]{login();});connect(password_,&QLineEdit::returnPressed,this,[this]{login();});
    connect(register_,&QPushButton::clicked,this,[this]{registerUser();});connect(role_,&QComboBox::currentIndexChanged,this,[this]{roleChanged();});connect(&session_,&AppSessionHsx::failed,this,[this](const QString& message){error_->setText(message);});
    connect(&session_,&AppSessionHsx::remoteLoginFinished,this,[this,submit](bool ok){submit->setEnabled(true);register_->setEnabled(true);if(ok)accept();});
    role_->setCurrentIndex(admin?1:0);roleChanged();password_->setFocus();
#ifdef IM_USER_CLIENT
    role_->removeItem(1);role_->setCurrentIndex(0);selector->hide();
#endif
}
void LoginDialogHsx::roleChanged() {
    const bool admin=role_->currentIndex()==1;userRole_->setChecked(!admin);if(adminRole_)adminRole_->setChecked(admin);serviceBox_->setVisible(!admin);linked_->setVisible(!admin);register_->setVisible(!admin);
    title_->setText(admin?"管理空间":"欢迎归来");intro_->setText(admin?"用心守护每一位用户的相逢。":"用熟悉的昵称，开启今天的故事。");userLabel_->setText(admin?"管理员账号":"你的昵称");
    user_->setPlaceholderText(admin?"管理员账号":"输入你的昵称");user_->setAccessibleName(admin?"管理员账号":"昵称");user_->setText(admin?AppSessionHsx::q(session_.admins().name()):session_.lastNickname());
    reveal_->setChecked(false);password_->clear();error_->clear();hint_->setText(admin?"初始账号  admin  /  admin123456\n进入后台后可修改管理员密码。":"使用已注册的昵称登录；新用户请先注册。");if(session_.remote()){hint_->setText(admin?"在线管理：查看同一服务器的账号、登录记录与会员订单。":"账号、私聊、群聊和微博广场由同一服务器同步。");linked_->hide();}services();
}
void LoginDialogHsx::services() {
    service_->clear();if(role_->currentIndex()==1)return;
    if(session_.remote()){for(int i=0;i<ActiveServiceCountHsx;++i)service_->addItem(AppSessionHsx::q(serviceName(serviceType(i))),i);return;}
    try{const int id=session_.accounts().findByNickname(AppSessionHsx::s(user_->text()));service_->setPlaceholderText({});for(const auto& entry:session_.data().user(id).services())if(activeService(entry.first))service_->addItem(AppSessionHsx::q(serviceName(entry.first)),static_cast<int>(entry.first));if(service_->count())service_->setCurrentIndex(0);}
    catch(const std::exception&){service_->setPlaceholderText("选择服务");}
}
void LoginDialogHsx::login() {
    if(role_->currentIndex()==1){if(session_.remote()){findChild<QPushButton*>("loginSubmit")->setEnabled(false);error_->setText("正在验证服务器管理员…");session_.loginRemoteAdmin(user_->text().trimmed(),password_->text());}else if(session_.loginAdmin(user_->text().trimmed(),password_->text()))accept();return;}
    if(session_.remote()){findChild<QPushButton*>("loginSubmit")->setEnabled(false);register_->setEnabled(false);error_->setText("正在登录并同步资料…");session_.loginRemote(user_->text(),serviceType(service_->currentData().toInt()),password_->text());return;}
    try{const int id=session_.accounts().findByNickname(AppSessionHsx::s(user_->text()));require(service_->currentIndex()>=0,"请选择已开通的服务");require(session_.data().user(id).services().size()<=1||linked_->isChecked(),"请确认联动登录已开通服务");if(session_.loginNickname(user_->text(),serviceType(service_->currentData().toInt()),password_->text()))accept();}
    catch(const std::exception& e){error_->setText(AppSessionHsx::q(e.what()));}
}
void LoginDialogHsx::registerUser() {
    if(session_.remote()){
        FormDialogHsx form("创建听澜账号",this);form.text("name","登录昵称");form.text("password","密码（至少 8 字节）",{},true);form.text("repeat","确认密码",{},true);
        form.submit([&](const FormDialogHsx& f){if(f.textValue("name").trimmed().isEmpty()||f.textValue("password").toUtf8().size()<8||f.textValue("password")!=f.textValue("repeat")){error_->setText("昵称不能为空；密码至少 8 字节且两次相同");return false;}return true;});
        if(form.exec()==QDialog::Accepted){user_->setText(form.textValue("name"));findChild<QPushButton*>("loginSubmit")->setEnabled(false);register_->setEnabled(false);session_.loginRemote(form.textValue("name"),ServiceHsx::QQ,form.textValue("password"),true);}return;
    }
    FormDialogHsx form("初次相逢 · 注册账号",this);form.text("name","登录昵称");form.text("birth","出生日期","2000-01-01");form.text("location","所在地","长春");
    QList<QPair<QString,QVariant>> types;for(int i=0;i<ActiveServiceCountHsx;++i)types.append({AppSessionHsx::q(serviceName(serviceType(i))),i});
    form.combo("service","首个服务",types);form.text("number","服务号码");form.text("password","密码（至少 8 字节）",{},true);form.text("repeat","确认密码",{},true);
    form.submit([this](const FormDialogHsx& f){return session_.mutate([&]{require(f.textValue("password")==f.textValue("repeat"),"两次密码不一致");PasswordHsx::check(AppSessionHsx::s(f.textValue("password")));bool ok=false;const auto number=f.textValue("number").toLongLong(&ok);require(ok,"服务号码应为整数");session_.accounts().registerUser(session_.accounts().availableUserId(),AppSessionHsx::s(f.textValue("name").trimmed()),AppSessionHsx::s(f.textValue("birth")),AppSessionHsx::s(f.textValue("location").trimmed()),serviceType(f.choice("service")),number,AppSessionHsx::s(f.textValue("password")),false);});});
    if(form.exec()==QDialog::Accepted){user_->setText(form.textValue("name").trimmed());service_->setCurrentIndex(service_->findData(form.choice("service")));password_->setText(form.textValue("password"));login();}
}
}
