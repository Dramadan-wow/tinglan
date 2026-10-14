#pragma once
#include "AppSessionHsx.h"
#include <QWidget>
class QPlainTextEdit;class QVBoxLayout;class QLabel;class QLineEdit;class QComboBox;class QTabWidget;class QListWidget;class QScrollArea;
namespace im {
class FeedPageHsx final:public QWidget{
public:
    explicit FeedPageHsx(AppSessionHsx&,QWidget* parent=nullptr,const QString& channel="moments");
    void refresh();
    bool selectImage(const QString& path);
    void showInbox();
    void showFollowing();
    void showHome();
private:
    QString name(int id)const;
    void refreshMessages();
    void openMessage(int target,const QString& post={});
    AppSessionHsx& session_;
    QString channel_,image_,originPost_;
    QPlainTextEdit* composer_;
    QVBoxLayout* posts_;
    QLabel *hint_,*photo_,*trends_=nullptr,*dmTitle_=nullptr;
    QLineEdit *search_,*dmInput_=nullptr;
    QComboBox* filter_;
    QTabWidget* tabs_;
    QListWidget* contacts_=nullptr;
    QPlainTextEdit* history_=nullptr;
    QScrollArea* scroll_;
    int dmTarget_=0;
};
}
