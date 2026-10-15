#include "UpdateManagerHsx.h"
#include "AppSessionHsx.h"
#include "LoginDialogHsx.h"
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QTextStream>
#include <QTimer>
int main(int argc,char** argv){
    QApplication app(argc,argv);if(app.arguments().size()!=3)return 2;
    app.setApplicationVersion("5.4.0");app.setApplicationName("IsolatedUpdateGuiChecks");app.setOrganizationName("TinglanTests");app.setQuitOnLastWindowClosed(false);
    const auto root=app.arguments()[2];QDir().mkpath(root);QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,root);
    QSettings().setValue("updates/automatic",true);
    im::AppSessionHsx session(im::AppSessionHsx::path(root+"/cache.txt"));
    auto* updater=new im::UpdateManagerHsx(&app);updater->configure(app.arguments()[1]);
    im::LoginDialogHsx login(session);login.show();int phase=0;QTimer poll;poll.setInterval(50);
    QObject::connect(&poll,&QTimer::timeout,&app,[&]{
        QDialog* dialog=nullptr;for(auto* w:app.topLevelWidgets())if(w->windowTitle()=="听澜 · 软件更新")dialog=qobject_cast<QDialog*>(w);
        if(!dialog)return;
        QString text;for(auto* label:dialog->findChildren<QLabel*>())text+=label->text();
        for(auto* edit:dialog->findChildren<QPlainTextEdit*>())text+=edit->toPlainText();
        if(phase==0&&text.contains("发现新版本")){
            if(!dialog->grab().save(root+"/available.png")){app.exit(3);return;}
            for(auto* b:dialog->findChildren<QPushButton*>())if(b->text()=="下载新版本"&&b->isEnabled()){phase=1;b->click();break;}
        }else if(phase==1&&text.contains("SHA-256 校验通过")){
            if(!dialog->grab().save(root+"/downloaded.png")){app.exit(4);return;}
            auto* button=login.findChild<QPushButton*>("softwareUpdates");if(!button){app.exit(5);return;}
            dialog->hide();button->click();
            if(!dialog->isVisible()){app.exit(6);return;}
            QTextStream(stdout)<<"PASS automatic notification, GUI download/progress, verified installer state, manual reopen, screenshots"<<Qt::endl;app.quit();
        }
    });poll.start();QTimer::singleShot(60000,&app,[&]{app.exit(7);});return app.exec();
}
