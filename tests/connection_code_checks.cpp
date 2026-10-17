#include "ConnectionCodeHsx.h"
#include "ConnectionDialogHsx.h"
#include "AppSessionHsx.h"
#include "LoginDialogHsx.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QTextStream>
#include <stdexcept>
static int count=0;
static void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);++count;QTextStream(stdout)<<"PASS "<<text<<Qt::endl;}
static QByteArray read(const QString& p){QFile f(p);f.open(QIODevice::ReadOnly);return f.readAll();}
template<typename F>void rejects(F f,const char* text){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected,text);}
int main(int argc,char** argv){
    QApplication app(argc,argv);if(argc!=3)return 2;const auto root=app.arguments()[1];QDir().mkpath(root);
    try{
        const auto ca=QString::fromUtf8(read(app.arguments()[2]));QJsonObject c{{"version",1},{"mode","lan"},{"host","127.0.0.1"},{"port",56110},{"ca",ca}};
        const auto code=im::net::ConnectionCodeHsx::encode(c);check(im::net::ConnectionCodeHsx::decode(code)==c,"Public CA connection code round trip");
        rejects([&]{im::net::ConnectionCodeHsx::decode(code+"x");},"Damaged code rejected");
        rejects([&]{auto v=c;v["tls"]=false;im::net::ConnectionCodeHsx::encode(v);},"TLS bypass fields rejected");
        rejects([&]{auto v=c;v["password"]="secret";im::net::ConnectionCodeHsx::encode(v);},"Credentials rejected");
        rejects([&]{auto v=c;v["ca"]="-----BEGIN PRIVATE KEY-----";im::net::ConnectionCodeHsx::encode(v);},"Private keys rejected");
        rejects([&]{auto v=c;v["ca"]="not a certificate";im::net::ConnectionCodeHsx::encode(v);},"Invalid certificate rejected");
        rejects([&]{auto v=c;v["host"]="0.0.0.0";im::net::ConnectionCodeHsx::encode(v);},"Wildcard rejected");
        rejects([&]{auto v=c;v["port"]=56110.1;im::net::ConnectionCodeHsx::encode(v);},"Fractional port rejected");
        rejects([&]{im::net::ConnectionCodeHsx::decode(QString(16385,'a'));},"Oversized text rejected");
        im::ConnectionDialogHsx settings(root+"/not-created.json");settings.show();app.processEvents();
        settings.findChild<QPlainTextEdit*>("connectionCode")->setPlainText(code);app.processEvents();
        check(settings.findChild<QLineEdit*>("serverHost")->text()=="127.0.0.1","Pasting text automatically recognizes host");
        check(!QFile::exists(root+"/connection-active.json"),"Paste alone does not activate trust");
        check(!settings.findChild<QRadioButton*>("modeLocal")->isEnabled(),"Ordinary client cannot start local server");
        settings.findChild<QPushButton*>("saveConnection")->click();const auto saved=QJsonDocument::fromJson(read(root+"/connection-active.json")).object();
        check(settings.result()==QDialog::Accepted&&saved["tls"].toBool()&&!saved["allowInsecureLoopback"].toBool()&&!saved["autoStartLocalServer"].toBool(),"Accepted code saves strict TLS profile");
        check(read(saved["caFile"].toString())==ca.toUtf8(),"Public CA saved without manual file copying");
        im::AppSessionHsx session(im::AppSessionHsx::path(root+"/data.txt"));session.configureRemote(root+"/connection-active.json");
        check(!QFile::exists(root+"/data.txt"),"Fresh ordinary client has no local account archive");
        int settingsOpened=0;
        im::LoginDialogHsx login(session,nullptr,false,[&]{++settingsOpened;return true;});login.show();app.processEvents();check(login.findChild<QComboBox*>("loginRole")->count()==1,"Only ordinary login role exists");
        login.findChild<QPushButton*>("connectionSettings")->click();
        check(settingsOpened==1&&login.result()==2,"Fresh client opens connection settings before first login");
        session.logout();session.logout();
        check(!QFile::exists(root+"/data.txt")&&!session.data().dirty()&&!session.signedIn(),"Repeated remote logout needs no archive and creates no local accounts");
        im::LoginDialogHsx cancelled(session,nullptr,false,[&]{++settingsOpened;return false;});cancelled.show();app.processEvents();
        cancelled.findChild<QPushButton*>("connectionSettings")->click();
        check(settingsOpened==2&&cancelled.isVisible()&&cancelled.findChild<QPushButton*>("loginSubmit")->isEnabled(),"Cancelling fresh connection settings keeps login usable");
        const auto existingPath=root+"/existing-data.txt";const QByteArray original="TENCENT 3\nEND 0 0 0 0\n";
        {QFile archive(existingPath);check(archive.open(QIODevice::WriteOnly)&&archive.write(original)==original.size(),"Prepared isolated existing archive");}
        im::AppSessionHsx existing(im::AppSessionHsx::path(existingPath));existing.configureRemote(root+"/connection-active.json");existing.logout();
        check(read(existingPath)==original&&!existing.data().dirty(),"Existing archive stays byte-identical and no default admin is inserted");
        {QFile archive(existingPath);archive.open(QIODevice::WriteOnly|QIODevice::Truncate);archive.write("not-an-archive");}
        rejects([&]{existing.logout();},"Existing corrupt archive is reported instead of reset");
        check(read(existingPath)=="not-an-archive","Corrupt archive is preserved without repair writes");
        bool adminButton=false;for(auto* b:login.findChildren<QPushButton*>())if(b->text()=="管理空间")adminButton=true;
        check(!adminButton&&!session.loginAdmin("admin","test-password")&&!session.adminSignedIn(),"Admin UI and authentication disabled in client build");
        auto unsafe=saved;unsafe["tls"]=false;QFile f(root+"/unsafe.json");f.open(QIODevice::WriteOnly);f.write(QJsonDocument(unsafe).toJson());f.close();
        im::AppSessionHsx other(im::AppSessionHsx::path(root+"/other.txt"));rejects([&]{other.configureRemote(f.fileName());},"Editing JSON cannot disable client TLS");
        check(settings.grab().save(root+"/connection-code.png"),"Actual connection-code UI rendered");QTextStream(stdout)<<"TOTAL "<<count<<Qt::endl;return 0;
    }catch(const std::exception& e){QTextStream(stderr)<<e.what()<<Qt::endl;return 1;}
}
