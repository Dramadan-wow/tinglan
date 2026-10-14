// Test-only driver: actual login dialog, ordinary chat composer and shared feed.
#include "AppSessionHsx.h"
#include "DemoDataHsx.h"
#include "MainWindowHsx.h"
#include "AdminWindowHsx.h"
#include "UiHsx.h"
#include <QLabel>
#include <QCheckBox>
#include "LoginDialogHsx.h"
#include "ChatPageHsx.h"
#include "FeedPageHsx.h"
#include <QComboBox>
#include <QListWidget>
#include <QTabWidget>
#include <QStackedWidget>
#include "ThemeHsx.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QSettings>
#include <QFileInfo>
#include <QTableWidget>
#include <QDialogButtonBox>
#include "FormDialogHsx.h"
#include <QTextStream>
#include <QTimer>
#include <cstdio>
#include <memory>
static void output(QJsonObject value){auto b=QJsonDocument(value).toJson(QJsonDocument::Compact);fwrite(b.constData(),1,size_t(b.size()),stdout);fputc('\n',stdout);fflush(stdout);}
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);im::ThemeHsx::install(app);
    QTextStream in(stdin);in.setEncoding(QStringConverter::Utf8);const auto c=QJsonDocument::fromJson(in.readLine().toUtf8()).object();
    const auto dir=c["directory"].toString();QDir().mkpath(dir);
    QCoreApplication::setOrganizationName("TinglanIsolatedTests");QCoreApplication::setApplicationName(QFileInfo(dir).fileName());
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir);
    if(c["seed"].toBool())im::DemoDataHsx::fixture(im::AppSessionHsx::path(QDir(dir).filePath("data.txt")));
    im::AppSessionHsx session(im::AppSessionHsx::path(QDir(dir).filePath("data.txt")));
    if(c["seed"].toBool()){session.save();return 0;}
    session.configureRemote(c["config"].toString());
    std::unique_ptr<im::MainWindowHsx> window;std::unique_ptr<im::AdminWindowHsx> adminWindow;
    im::LoginDialogHsx login(session,nullptr,c["admin"].toBool());
    QObject::connect(&session,&im::AppSessionHsx::remoteLoginFinished,&app,[&](bool ok){
        if(ok&&session.adminSignedIn()&&!adminWindow){adminWindow=std::make_unique<im::AdminWindowHsx>(session);adminWindow->show();}
        if(ok&&!session.adminSignedIn()&&!window){window=std::make_unique<im::MainWindowHsx>(session);window->show();}
        output({{"event","login"},{"ok",ok},{"user",session.user()}});
    });
    QObject::connect(session.remoteClient(),&im::net::ChatClientHsx::sessionRevoked,&app,[]{output({{"event","revoked"}});});
    QObject::connect(&session,&im::AppSessionHsx::failed,&app,[](QString text){output({{"event","failure"},{"text",text}});});
    QObject::connect(&session,&im::AppSessionHsx::connectionChanged,&app,[](QString text){output({{"event","state"},{"text",text}});});
    QObject::connect(session.remoteClient(),&im::net::ChatClientHsx::workspaceSnapshot,&app,[](QJsonObject data){output({{"event","snapshot"},{"data",data}});});
    QObject::connect(session.remoteClient(),&im::net::ChatClientHsx::workspaceResult,&app,[](QString id,QJsonObject data){output({{"event","result"},{"id",id},{"data",data}});});
    QFile commands(QDir(dir).filePath("commands.jsonl"));qint64 offset=0;QByteArray pending;QTimer timer,dismiss;
    dismiss.setInterval(30);QObject::connect(&dismiss,&QTimer::timeout,&app,[]{if(auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())){if(box->standardButtons().testFlag(QMessageBox::Yes))box->button(QMessageBox::Yes)->click();else box->accept();}});dismiss.start();
    timer.setInterval(40);QObject::connect(&timer,&QTimer::timeout,&app,[&]{
        if(!commands.open(QIODevice::ReadOnly))return;commands.seek(offset);pending+=commands.readAll();offset=commands.pos();commands.close();
        while(pending.contains('\n')){
            auto end=pending.indexOf('\n');auto cmd=QJsonDocument::fromJson(pending.left(end)).object();pending.remove(0,end+1);auto action=cmd["action"].toString();
            if(action=="quit"){session.logout();app.quit();return;}
            if(action=="service"){auto* combo=window->findChild<QComboBox*>("activeService");const int index=combo->findData(cmd["value"].toInt());if(index>=0)combo->setCurrentIndex(index);output({{"event","service"},{"ok",index>=0&&int(session.service())==cmd["value"].toInt()}});}
            else if(action=="workspaceState"&&window){auto* combo=window->findChild<QComboBox*>("activeService");output({{"event","workspaceState"},{"service",int(session.service())},{"weiboAvailable",combo->findData(2)>=0},{"page",window->findChild<QStackedWidget*>("mainPages")->currentIndex()},{"weiboVisible",window->findChild<QWidget*>("weiboPage")->isVisible()},{"inboxVisible",window->findChild<QPushButton*>("weiboInboxNav")->isVisible()}});}
            else if(action=="openWeibo"&&window){window->navigate(4);QTimer::singleShot(50,&app,[&]{auto* form=dynamic_cast<im::FormDialogHsx*>(QApplication::activeModalWidget());if(!form)return;form->findChild<QComboBox*>("service")->setCurrentIndex(2);form->findChild<QLineEdit*>("number")->setText(QString::number(session.data().account(session.user(),im::ServiceHsx::QQ).number()));form->findChild<QLineEdit*>("password")->setText("123456");form->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();});for(auto* b:window->findChildren<QPushButton*>())if(b->text()=="开通服务"){b->click();break;}}
            else if(action=="rpc"){auto args=cmd["args"].toObject();session.command(cmd["op"].toString(),args);}
            else if(action=="send"&&window){window->navigate(0);auto* page=window->chat();page->open(im::MessageKindHsx(cmd["kind"].toInt()),cmd["target"].toInt());page->findChild<QLineEdit*>("messageInput")->setText(cmd["text"].toString());page->findChild<QPushButton*>("sendMessage")->click();}
            else if(action=="rename"&&window){
                window->navigate(4);window->findChild<QTableWidget*>("servicesTable")->selectRow(0);
                QTimer::singleShot(50,&app,[&,cmd]{auto* form=dynamic_cast<im::FormDialogHsx*>(QApplication::activeModalWidget());if(!form){output({{"event","failure"},{"text","Missing account form"}});return;}form->findChild<QLineEdit*>("nick")->setText(cmd["nick"].toString());form->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();});
                for(auto* button:window->findChildren<QPushButton*>())if(button->text()=="修改登录昵称 / 密码"){button->click();break;}
            }
            else if(action=="post"&&window){const bool blog=cmd["channel"]=="weibo";window->navigate(blog?8:6);auto* page=dynamic_cast<im::FeedPageHsx*>(window->findChild<QWidget*>(blog?"weiboPage":"momentsPage"));if(!cmd["image"].toString().isEmpty())page->selectImage(cmd["image"].toString());page->findChild<QPlainTextEdit*>("feedComposer")->setPlainText(cmd["text"].toString());page->findChild<QPushButton*>("publishPost")->click();}
            else if(action=="style"&&window){window->navigate(7);window->findChild<QPushButton*>(cmd["button"].toString())->click();output({{"event","style"},{"theme",session.studio().choice(session.user(),"theme")},{"bubble",session.studio().choice(session.user(),"bubble")}});}
            else if(action=="order"&&window){window->navigate(7);window->findChild<QComboBox*>("membershipPlan")->setCurrentIndex(cmd["tier"].toInt()-1);window->findChild<QLineEdit*>("paymentReference")->setText(cmd["reference"].toString());window->findChild<QPushButton*>("submitPaymentOrder")->click();}
            else if(action=="dm"&&window){window->navigate(8);auto* page=window->findChild<QWidget*>("weiboPage");const int target=cmd["target"].toInt();bool found=false;if(cmd["fromPost"].toBool()){for(auto* button:page->findChildren<QPushButton*>("messageAuthor"))if(button->property("author").toInt()==target){button->click();found=true;break;}}else{page->findChild<QTabWidget*>("socialTabs")->setCurrentIndex(1);auto* list=page->findChild<QListWidget*>("weiboContacts");for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole).toInt()==target){list->setCurrentRow(i);found=true;break;}}if(found){page->findChild<QLineEdit*>("weiboMessageInput")->setText(cmd["text"].toString());page->findChild<QPushButton*>("sendWeiboMessage")->click();}else output({{"event","failure"},{"text","DM entry missing"}});}
            else if(action=="shot"&&window){window->navigate(cmd["page"].toInt());if(cmd.contains("tab")){auto* widget=window->findChild<QWidget*>(cmd["page"].toInt()==8?"weiboPage":"momentsPage");widget->findChild<QTabWidget*>("socialTabs")->setCurrentIndex(cmd["tab"].toInt());}QTimer::singleShot(250,&app,[&,cmd]{output({{"event","screenshot"},{"ok",window->grab().save(QDir(dir).filePath(cmd["name"].toString("ui.png")))}});});}
            else if(action=="adminState"&&adminWindow){
                QJsonArray counts,users,logs,orders;
                for(int i=0;i<4;++i)counts.append(adminWindow->findChild<QLabel*>(QString("adminCount%1").arg(i))->text().toInt());
                auto rows=[](QTableWidget* table){QJsonArray out;for(int r=0;r<table->rowCount();++r){QJsonArray row;for(int c=0;c<table->columnCount();++c)row.append(table->item(r,c)->text());out.append(row);}return out;};
                output({{"event","adminState"},{"counts",counts},{"users",rows(adminWindow->findChild<QTableWidget*>("adminUsers"))},{"logs",rows(adminWindow->findChild<QTableWidget*>("adminLogs"))},{"orders",rows(adminWindow->findChild<QTableWidget*>("adminPaymentOrders"))}});
            }
            else if(action=="adminClick"&&adminWindow){im::UiHsx::select(adminWindow->findChild<QTableWidget*>("adminUsers"),cmd["target"].toInt());adminWindow->findChild<QPushButton*>(cmd["button"].toString())->click();}
            else if(action=="adminForm"&&adminWindow){
                im::UiHsx::select(adminWindow->findChild<QTableWidget*>("adminUsers"),cmd["target"].toInt());
                if(cmd.contains("order")){adminWindow->findChild<QTabWidget*>("adminTabs")->setCurrentIndex(2);auto* table=adminWindow->findChild<QTableWidget*>("adminPaymentOrders");for(int r=0;r<table->rowCount();++r)if(table->item(r,0)->data(Qt::UserRole+3).toString()==cmd["order"].toString())table->selectRow(r);}
                QTimer::singleShot(50,&app,[&,cmd]{auto* form=dynamic_cast<im::FormDialogHsx*>(QApplication::activeModalWidget());if(!form)return;const auto fields=cmd["fields"].toObject();for(auto it=fields.begin();it!=fields.end();++it){if(auto* line=form->findChild<QLineEdit*>(it.key()))line->setText(it.value().toString());else if(auto* box=form->findChild<QCheckBox*>(it.key()))box->setChecked(it.value().toBool());}form->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();});
                adminWindow->findChild<QPushButton*>(cmd["button"].toString())->click();
            }
            else if(action=="adminShot"&&adminWindow){adminWindow->findChild<QTabWidget*>("adminTabs")->setCurrentIndex(cmd["tab"].toInt());QTimer::singleShot(100,&app,[&,cmd]{output({{"event","screenshot"},{"ok",adminWindow->grab().save(QDir(dir).filePath(cmd["name"].toString("admin.png")))}});});}
            else if(action=="pulse")output({{"event","pulse"}});
        }
    });timer.start();
    login.show();login.findChild<QLineEdit*>("loginUser")->setText(c["nickname"].toString());login.findChild<QLineEdit*>("loginPassword")->setText(c["password"].toString());
    QTimer::singleShot(0,&login,[&]{login.findChild<QPushButton*>("loginSubmit")->click();});
    return app.exec();
}

