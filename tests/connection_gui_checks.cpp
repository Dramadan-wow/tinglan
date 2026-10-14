#include "ConnectionDialogHsx.h"
#include "RuntimeHsx.h"
#include "AppSessionHsx.h"
#include "LoginDialogHsx.h"
#include "ThemeHsx.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTextStream>
#include <stdexcept>
namespace {
int checks=0;
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);++checks;QTextStream(stdout)<<"PASS "<<text<<Qt::endl;}
QByteArray read(const QString& name){QFile f(name);f.open(QIODevice::ReadOnly);return f.readAll();}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setApplicationName("connection-isolated-test");im::ThemeHsx::install(app);
    if(argc!=2)return 2;const auto root=QDir(app.arguments()[1]).absolutePath();QDir().mkpath(root);
    try{
        const QByteArray original=R"({"host":"127.0.0.1","port":56110,"tls":false,"autoStartLocalServer":true,"allowInsecureLoopback":true})";
        QFile f(root+"/client.json");f.open(QIODevice::WriteOnly);f.write(original);f.close();
        im::ConnectionDialogHsx d(root+"/client.json");d.show();app.processEvents();
        d.findChild<QRadioButton*>("modeCloud")->click();
        auto* host=d.findChild<QLineEdit*>("serverHost");auto* port=d.findChild<QSpinBox*>("serverPort");auto* save=d.findChild<QPushButton*>("saveConnection");
        host->clear();save->click();check(!QFile::exists(root+"/connection-active.json"),"Empty host cannot be saved");
        host->setText("0.0.0.0");save->click();check(!QFile::exists(root+"/connection-active.json"),"Wildcard is not a connection target");
        host->setText("chat.example.test");port->setValue(56110);d.findChild<QLabel*>("connectionError")->clear();app.processEvents();
        check(d.grab().save(root+"/connection-settings.png"),"Rendered real Qt connection settings screenshot");
        d.findChild<QLineEdit*>("serverCa")->setText(root+"/missing.pem");save->click();check(!QFile::exists(root+"/connection-active.json"),"Missing CA cannot be saved");
        d.findChild<QLineEdit*>("serverCa")->clear();save->click();
        if(d.result()!=QDialog::Accepted)QTextStream(stderr)<<d.findChild<QLabel*>("connectionError")->text()<<Qt::endl;
        auto c=QJsonDocument::fromJson(read(root+"/connection-active.json")).object();
        check(d.result()==QDialog::Accepted&&c["mode"]=="cloud"&&c["host"]=="chat.example.test","Cloud settings saved by actual button");
        check(c["tls"].toBool()&&!c["allowInsecureLoopback"].toBool()&&!c["autoStartLocalServer"].toBool(),"Remote settings enforce verified TLS and no local bootstrap");
        check(read(root+"/client.json")==original,"Original local configuration unchanged");
        im::ConnectionDialogHsx cancelled(root+"/connection-active.json");cancelled.findChild<QRadioButton*>("modeLan")->click();cancelled.reject();
        check(read(root+"/connection-active.json").contains("cloud"),"Cancel does not replace active profile");
        const auto runtime=im::prepareRuntime();check(runtime.config==root+"/connection-active.json","Normal bootstrap selects saved profile");
        check(!QFile::exists(root+"/server.json"),"LAN/cloud mode creates no local server config");
        im::AppSessionHsx session(im::AppSessionHsx::path(root+"/isolated-data.txt"));session.configureRemote(root+"/connection-active.json");
        bool opened=false;im::LoginDialogHsx login(session,nullptr,false,[&]{opened=true;return true;});login.show();app.processEvents();
        check(login.grab().save(root+"/login-settings-entry.png"),"Rendered login entry without changing existing visual style");
        login.findChild<QPushButton*>("loginSubmit")->setEnabled(false);
        login.findChild<QPushButton*>("connectionSettings")->click();
        check(opened&&login.result()==2&&login.findChild<QPushButton*>("loginSubmit")->isEnabled(),"Connection settings can cancel pending login and request clean session recreation");
        const auto explicitProfile=im::prepareRuntime(root+"/connection-active.json");check(explicitProfile.config==runtime.config,"Explicit profile accepted without replacing local config");
        im::ConnectionDialogHsx restore(root+"/connection-active.json");auto* local=restore.findChild<QRadioButton*>("modeLocal");
        if(local->isEnabled()){local->click();restore.findChild<QPushButton*>("saveConnection")->click();auto saved=QJsonDocument::fromJson(read(root+"/connection-active.json")).object();check(saved["host"]=="127.0.0.1"&&saved["autoStartLocalServer"].toBool()&&!saved["tls"].toBool(),"Local mode restores original development settings");}
        QTextStream(stdout)<<"PASS TOTAL "<<checks<<Qt::endl;return 0;
    }catch(const std::exception& e){QTextStream(stderr)<<e.what()<<Qt::endl;return 1;}
}
