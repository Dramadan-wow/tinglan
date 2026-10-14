#include "DemoDataHsx.h"
#include "UiHsx.h"
#include "GuiChecksHsx.h"
#include "MainWindowHsx.h"
#include <QStackedWidget>
#include <QPlainTextEdit>
#include "AdminWindowHsx.h"
#include "ChatPageHsx.h"
#include "LoginDialogHsx.h"
#include "NetworkPageHsx.h"
#include "StorageHsx.h"
#include "FormDialogHsx.h"
#include "StudioChecksHsx.h"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTimer>
#include <QTcpSocket>
#include <QtEndian>
#include <QThread>
#include <stdexcept>
namespace im {
int GuiChecksHsx::run(const QString& output){
    QJsonArray passed;auto check=[&](bool condition,const QString& name){if(!condition)throw std::runtime_error(AppSessionHsx::s(name));passed.append(name);QFile progress(QDir(output).filePath("progress.txt"));if(progress.open(QIODevice::WriteOnly|QIODevice::Append))progress.write(name.toUtf8()+"\n");};
    QTimer diagnostics;QObject::connect(&diagnostics,&QTimer::timeout,[&]{if(auto* modal=QApplication::activeModalWidget()){QFile log(QDir(output).filePath("modal.txt"));if(log.open(QIODevice::WriteOnly)){log.write(modal->windowTitle().toUtf8()+"\n");for(auto* label:modal->findChildren<QLabel*>())log.write(label->text().toUtf8()+"\n");}}});diagnostics.start(10000);
    auto wait=[](const std::function<bool()>& ready){QElapsedTimer timer;timer.start();while(!ready()&&timer.elapsed()<3000){QApplication::processEvents();QThread::msleep(5);}return ready();};
    QJsonObject report;int code=0;
    try{
        QDir dir(output);dir.mkpath(".");const auto data=dir.filePath("data.txt");check(!QFile::exists(data),"测试使用新的独立数据文件");
        { AppSessionHsx fresh(AppSessionHsx::path(dir.filePath("fresh.txt")));check(fresh.data().users().empty(),"正式启动不自动创建任何演示人物"); }
        check(UiHsx::allServices().size()==3,"注册与开通仅提供 QQ、微信和微博");
        AppSessionHsx session(im::DemoDataHsx::fixture(AppSessionHsx::path(data)));int failures=0;QObject::connect(&session,&AppSessionHsx::failed,&session,[&](const QString&){++failures;});
        check(!session.login(10001,ServiceHsx::QQ,"wrong")&&failures==1,"错误密码被拒绝");
        LoginDialogHsx login(session);login.show();QApplication::processEvents();check(login.findChild<QLineEdit*>("loginUser")->text().isEmpty()&&login.findChild<QLineEdit*>("loginUser")->placeholderText().contains("昵称"),"普通登录只输入昵称");
        auto roleButtons=login.findChildren<QPushButton*>("roleButton");check(roleButtons.size()==2,"双角色入口存在");roleButtons[1]->click();check(login.findChild<QComboBox*>("loginRole")->currentIndex()==1&&login.findChild<QLineEdit*>("loginUser")->text()=="admin"&&!login.findChild<QComboBox*>("loginService")->isVisible(),"角色按钮切换管理员表单");roleButtons[0]->click();check(login.findChild<QComboBox*>("loginRole")->currentIndex()==0&&login.findChild<QComboBox*>("loginService")->isVisible(),"用户角色恢复昵称与服务选择");
        auto* reveal=login.findChild<QAction*>("revealPassword");reveal->trigger();check(login.findChild<QLineEdit*>("loginPassword")->echoMode()==QLineEdit::Normal,"密码显隐按钮展示输入");reveal->trigger();check(login.findChild<QLineEdit*>("loginPassword")->echoMode()==QLineEdit::Password,"密码显隐按钮恢复遮挡");
        login.findChild<QLineEdit*>("loginUser")->setText("10001");login.findChild<QLineEdit*>("loginPassword")->setText("123456");login.findChild<QPushButton*>("loginSubmit")->click();check(!session.signedIn()&&!login.findChild<QLabel*>("loginError")->text().isEmpty(),"数字用户ID不能替代昵称登录");login.findChild<QLineEdit*>("loginUser")->setText("张三");
        auto* music=login.findChild<QMediaPlayer*>("loginMusicPlayer");auto* audio=login.findChild<QAudioOutput*>("loginAudioOutput");audio->setMuted(true);
        check(wait([&]{return music->duration()>0;})&&music->hasAudio()&&music->error()==QMediaPlayer::NoError,"内置音乐资源可被实际解码");
        login.findChild<QSlider*>("musicVolume")->setValue(65);check(qAbs(audio->volume()-.65f)<.01f,"音量滑块控制实际音频输出");
        login.findChild<QPushButton*>("musicPlay")->click();check(wait([&]{return music->playbackState()==QMediaPlayer::PlayingState&&music->position()>0;}),"播放按钮启动实际音乐播放");login.findChild<QPushButton*>("musicPlay")->click();check(music->playbackState()==QMediaPlayer::PausedState,"暂停按钮停止播放进度");
        login.findChild<QLabel*>("loginError")->clear();login.findChild<QLineEdit*>("loginPassword")->clear();check(login.grab().save(dir.filePath("page-login.png")),"听澜昵称登录页渲染截图");
        login.findChild<QLineEdit*>("loginPassword")->setText("123456");login.findChild<QPushButton*>("loginSubmit")->click();check(login.result()==QDialog::Accepted&&session.user()==10001,"昵称登录按钮建立真实会话");check(music->playbackState()==QMediaPlayer::StoppedState,"登录成功后音乐停止");
        MainWindowHsx window(session);window.show();QApplication::processEvents();
        auto* conversationSearch=window.findChild<QLineEdit*>("conversationSearch");auto* conversations=window.findChild<QListWidget*>("conversations");conversationSearch->setText("李四");int matches=0;for(int i=0;i<conversations->count();++i)if(!conversations->item(i)->isHidden())++matches;check(matches==1,"会话搜索筛选联系人");conversationSearch->setText("不存在的会话");matches=0;for(int i=0;i<conversations->count();++i)if(!conversations->item(i)->isHidden())++matches;check(matches==0,"会话搜索不匹配时不显示无关联系人");conversationSearch->clear();
        FormDialogHsx selectedFriend("好友选择回归");selectedFriend.combo("target","好友",{{"李四",10002},{"赵六",10004}},10004);check(selectedFriend.choice("target")==10004,"好友表单按真实用户值预选而非按行号预选");selectedFriend.combo("scope","重置范围",{{"全部服务",-1},{"QQ",0},{"微信",1}});check(selectedFriend.choice("scope")==-1,"未指定预选值的表单默认首项全部服务");
        check(session.unreadCount()==2,"加载演示消息未自动标记已读");window.navigate(3);window.chat()->open(MessageKindHsx::Private,10002);check(session.unreadCount()==2,"隐藏的聊天页不标记消息已读");window.navigate(0);check(session.unreadCount()==1,"显示当前聊天只标记对应消息");
        window.findChild<QLineEdit*>("messageInput")->setText("Qt 按钮发送测试：你好，世界！");window.findChild<QPushButton*>("sendMessage")->click();
        const auto privateHistory=session.messages().history(10001,ServiceHsx::QQ,MessageKindHsx::Private,10002);check(privateHistory.size()==2&&privateHistory.back()->text()=="Qt 按钮发送测试：你好，世界！","发送按钮写入真实消息库");check(window.findChild<QLineEdit*>("messageInput")->text().isEmpty(),"发送成功清空输入框");
        window.chat()->open(MessageKindHsx::Group,1001);check(session.unreadCount()==0,"打开群聊标记群消息");window.findChild<QLineEdit*>("messageInput")->setText("来自 Qt 前端的群消息");window.findChild<QPushButton*>("sendMessage")->click();check(session.messages().history(10001,ServiceHsx::QQ,MessageKindHsx::Group,1001).size()==2,"群聊发送调用群消息业务");
        const int beforeSwitchFailures=failures;auto* combo=window.findChild<QComboBox*>("activeService");combo->setCurrentIndex(combo->findData(static_cast<int>(ServiceHsx::WeChat)));check(session.service()==ServiceHsx::WeChat&&window.chat()->target()==0,"切换服务清除旧聊天并隔离数据");combo->setCurrentIndex(combo->findData(0));check(failures==beforeSwitchFailures,"切换服务不按新身份读取或标记旧会话消息");
        DataStoreHsx disk;StorageHsx::load(disk,AppSessionHsx::path(data),false);check(disk.messages().size()==4,"GUI 消息自动保存并可重新加载");
        auto button=[&](const QString& text){for(auto* b:window.findChildren<QPushButton*>())if(b->text()==text)return b;throw std::runtime_error(AppSessionHsx::s("找不到按钮："+text));};
        auto fillDialog=[&](const std::function<void(QDialog*)>& fill){QTimer::singleShot(0,&window,[fill]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;fill(dialog);dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();});};
        window.navigate(2);fillDialog([](QDialog* dialog){dialog->findChild<QLineEdit*>("name")->setText("Qt 创建的测试群");});button("创建群组")->click();check(session.data().groups().size()==8,"创建群组表单写入真实群组");
        check(window.findChild<QTableWidget*>("friendsTable")->isColumnHidden(0)&&window.findChild<QTableWidget*>("groupMembers")->isColumnHidden(0),"普通好友和群成员界面隐藏用户编号");
        auto* groupList=window.findChild<QListWidget*>("groupsList");for(int i=0;i<groupList->count();++i)if(groupList->item(i)->data(Qt::UserRole).toInt()==1008)groupList->setCurrentRow(i);
        fillDialog([](QDialog* dialog){dialog->findChild<QComboBox*>("target")->setCurrentIndex(dialog->findChild<QComboBox*>("target")->findData(10002));dialog->findChild<QComboBox*>("service")->setCurrentIndex(0);});button("邀请成员")->click();check(session.data().group(1008).contains(10002,ServiceHsx::QQ),"邀请成员表单接入入群操作");
        auto* members=window.findChild<QTableWidget*>("groupMembers");for(int row=0;row<members->rowCount();++row)if(members->item(row,0)->data(Qt::UserRole).toInt()==10002)members->selectRow(row);button("执行成员操作")->click();check(session.data().group(1008).admins().count(10002)==1,"成员操作按钮设置管理员");
        fillDialog([](QDialog* dialog){dialog->findChild<QComboBox*>("mode")->setCurrentIndex(1);});button("切换管理模式")->click();check(session.data().group(1008).policy().mode()==GroupModeHsx::WeChat&&session.data().group(1008).members().size()==2,"管理模式表单切换并保留成员");
        session.login(10002,ServiceHsx::QQ,"123456");window.navigate(3);auto* received=window.findChild<QTableWidget*>("receivedRequests");for(int row=0;row<received->rowCount();++row)if(received->item(row,0)->data(Qt::UserRole).toLongLong()==6)received->selectRow(row);button("同意申请")->click();check(session.data().account(10002,ServiceHsx::QQ).friends().count(10003)==1,"通知审批按钮建立跨服务好友关系");session.login(10001,ServiceHsx::QQ,"123456");
        window.chat()->open(MessageKindHsx::Private,10002);for(int i=0;i<7;++i){window.navigate(i);QApplication::processEvents();check(window.grab().save(dir.filePath(QString("page-%1.png").arg(i))),QString("页面 %1 渲染截图").arg(i));}
        window.navigate(6);QApplication::processEvents();auto* feed=window.findChild<QStackedWidget*>("mainPages")->currentWidget();
        check(feed->findChild<QPlainTextEdit*>("feedComposer")&&!feed->findChild<QLineEdit*>("tcpIp"),"离线微博入口显示广场页面且无 TCP 地址表单");
        check(!feed->findChild<QPushButton*>("publishPost")->isEnabled()&&!feed->findChild<QPushButton*>("refreshFeed")->isEnabled(),"离线广场禁止伪发布与刷新");
        bool startupHint=false;for(auto* label:feed->findChildren<QLabel*>())if(label->text().contains("启动图形界面.cmd"))startupHint=true;check(startupHint,"离线广场提供正式客户端启动步骤");
        NetworkPageHsx client;check(client.findChild<QLineEdit*>("tcpRecipient")&&client.findChild<QLineEdit*>("tcpPassword"),"中心服务器页面包含独立远程账号和收件人");check(!client.connected()&&!client.sendText("未登录消息"),"未登录不能发送远程消息");client.stop();window.close();
        bool returned=false;QObject::connect(&window,&MainWindowHsx::returnToLogin,&window,[&]{returned=window.close();});window.show();QTimer::singleShot(0,&window,[&]{window.findChild<QPushButton*>("switchAccount")->click();});QApplication::exec();session.logout();
        check(returned&&!session.signedIn()&&!session.data().user(10001).online(),"普通用户登出按钮结束会话并返回登录流程");
        const auto adminFile=dir.filePath("admin-data.txt");AppSessionHsx adminSession(im::DemoDataHsx::fixture(AppSessionHsx::path(adminFile)));
        check(!adminSession.loginAdmin("10001","123456")&&!adminSession.adminSignedIn(),"普通用户账号不能认证为系统管理员");
        bool denied=false;try{AdminWindowHsx forbidden(adminSession);}catch(const std::exception&){denied=true;}check(denied,"未经认证不能构造管理后台");
        LoginDialogHsx adminLogin(adminSession,nullptr,true);adminLogin.findChild<QLineEdit*>("loginPassword")->setText("admin123456");adminLogin.findChild<QPushButton*>("loginSubmit")->click();
        check(adminLogin.result()==QDialog::Accepted&&adminSession.adminSignedIn()&&!adminSession.signedIn(),"管理员登录入口创建独立权限会话");
        AdminWindowHsx dashboard(adminSession);dashboard.show();QApplication::processEvents();auto* users=dashboard.findChild<QTableWidget*>("adminUsers");check(users->rowCount()==4,"后台列出真实用户及登录状态");
        auto selectUser=[&](int id){for(int row=0;row<users->rowCount();++row)if(users->item(row,0)->data(Qt::UserRole).toInt()==id){users->selectRow(row);return;}throw std::runtime_error("admin user row missing");};selectUser(10002);
        auto fillAdmin=[&](const std::function<void(QDialog*)>& fill){QTimer::singleShot(0,&dashboard,[fill]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;fill(dialog);dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();});};
        fillAdmin([](QDialog* d){d->findChild<QLineEdit*>("new")->setText("gui-reset-2026");d->findChild<QLineEdit*>("repeat")->setText("gui-reset-2026");});dashboard.findChild<QPushButton*>("resetUserPassword")->click();
        check(adminSession.data().account(10002,ServiceHsx::QQ).verify("gui-reset-2026")&&adminSession.data().account(10002,ServiceHsx::WeChat).verify("gui-reset-2026")&&!adminSession.data().account(10002,ServiceHsx::QQ).verify("123456"),"重置表单更新全部服务密码并废除旧密码");
        auto confirmAdmin=[&]{QTimer::singleShot(0,&dashboard,[]{auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});};
        adminSession.accounts().login(10002,ServiceHsx::QQ,"gui-reset-2026");confirmAdmin();dashboard.findChild<QPushButton*>("denyUserLogin")->click();
        check(!adminSession.data().user(10002).loginAllowed()&&!adminSession.data().user(10002).online(),"禁止登录按钮禁用账号并登出");
        denied=false;try{adminSession.accounts().login(10002,ServiceHsx::QQ,"gui-reset-2026");}catch(const std::exception&){denied=true;}check(denied,"后台禁用状态拦截正确密码登录");
        DataStoreHsx disabledDisk;StorageHsx::load(disabledDisk,AppSessionHsx::path(adminFile),false);check(!disabledDisk.user(10002).loginAllowed()&&!disabledDisk.adminLogs().empty(),"管理操作和禁用状态自动写入存档");
        dashboard.findChild<QPushButton*>("allowUserLogin")->click();check(adminSession.data().user(10002).loginAllowed()&&!adminSession.data().user(10002).online(),"允许登录按钮恢复登录资格");
        adminSession.accounts().login(10002,ServiceHsx::QQ,"gui-reset-2026");adminSession.mutate([]{});confirmAdmin();dashboard.findChild<QPushButton*>("forceUserLogout")->click();check(!adminSession.data().user(10002).online(),"强制登出按钮使在线用户离线");
        dashboard.findChild<QLineEdit*>("adminSearch")->setText("李四");check(users->rowCount()==1,"后台搜索定位指定用户");dashboard.findChild<QLineEdit*>("adminSearch")->clear();
        fillAdmin([](QDialog* d){d->findChild<QLineEdit*>("old")->setText("admin123456");d->findChild<QLineEdit*>("new")->setText("gui-admin-2026");d->findChild<QLineEdit*>("repeat")->setText("gui-admin-2026");});dashboard.findChild<QPushButton*>("changeAdminPassword")->click();
        DataStoreHsx adminDisk;StorageHsx::load(adminDisk,AppSessionHsx::path(adminFile),false);AdminManagerHsx checkAdmin(adminDisk);checkAdmin.login("admin","gui-admin-2026");check(checkAdmin.active(),"管理员改密表单生效且重启可登录");
        check(dashboard.findChild<QTableWidget*>("adminLogs")->rowCount()>=8,"后台显示登录与管理操作记录");check(dashboard.grab().save(dir.filePath("page-admin.png")),"管理员后台渲染截图");
        auto* loginRecords=dashboard.findChild<QTableWidget*>("adminLogs");dashboard.findChild<QComboBox*>("adminLogFilter")->setCurrentIndex(1);bool linkedIdentity=false;for(int row=0;row<loginRecords->rowCount();++row)if(loginRecords->item(row,1)->text()=="李四"&&loginRecords->item(row,3)->text()=="10002")linkedIdentity=true;
        check(linkedIdentity,"管理员登录记录同时显示昵称与内部用户ID");dashboard.findChild<QComboBox*>("adminLogFilter")->setCurrentIndex(0);
        returned=false;QObject::connect(&dashboard,&AdminWindowHsx::returnToLogin,&dashboard,[&]{returned=dashboard.close();});QTimer::singleShot(0,&dashboard,[&]{dashboard.findChild<QPushButton*>("adminLogout")->click();});QApplication::exec();adminSession.logout();check(returned&&!adminSession.adminSignedIn(),"退出管理员按钮撤销管理权限");
        LoginDialogHsx afterLogout(adminSession);QTimer::singleShot(0,&afterLogout,[&]{afterLogout.findChild<QLineEdit*>("loginUser")->setText("张三");afterLogout.findChild<QLineEdit*>("loginPassword")->setText("123456");afterLogout.findChild<QPushButton*>("loginSubmit")->click();});
        check(afterLogout.exec()==QDialog::Accepted&&adminSession.signedIn()&&!adminSession.adminSignedIn(),"返回登录后可切换到普通用户");adminSession.logout();
        AppSessionHsx registerSession(AppSessionHsx::path(dir.filePath("register-data.txt")));LoginDialogHsx registration(registerSession);bool noUserId=false;
        QTimer::singleShot(0,&registration,[&]{auto* form=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!form)return;noUserId=form->findChild<QSpinBox*>("id")==nullptr;form->findChild<QLineEdit*>("name")->setText("清禾");form->findChild<QLineEdit*>("number")->setText("90901");form->findChild<QLineEdit*>("password")->setText("new-password");form->findChild<QLineEdit*>("repeat")->setText("new-password");form->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();});registration.findChild<QPushButton*>("registerAccount")->click();
        check(noUserId&&registerSession.data().users().size()==1&&registerSession.accounts().findByNickname("清禾")==10001,"注册表单自动分配内部编号而不显示ID");check(registerSession.signedIn()&&registerSession.data().user(registerSession.user()).name()=="清禾","新注册昵称自动登录");registerSession.logout();
        StudioChecksHsx::run(output,passed);
        report["status"]="passed";
    }catch(const std::exception& e){code=1;report["status"]="failed";report["error"]=AppSessionHsx::q(e.what());}
    report["checks"]=passed;report["count"]=passed.size();QFile results(QDir(output).filePath("results.json"));if(results.open(QIODevice::WriteOnly))results.write(QJsonDocument(report).toJson());return code;
}
}
