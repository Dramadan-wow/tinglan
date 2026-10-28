#pragma once
#include "AppSessionHsx.h"
#include <QMainWindow>
#include <array>
class QVBoxLayout; class QStackedWidget; class QComboBox; class QLabel; class QPushButton; class QCloseEvent;
namespace im {
class ChatPageHsx; class FriendsPageHsx; class GroupsPageHsx; class RequestsPageHsx; class AccountsPageHsx; class DataPageHsx; class NetworkPageHsx;
class StudioPageHsx; class FeedPageHsx;
class MainWindowHsx final : public QMainWindow {
    Q_OBJECT
public:
    MainWindowHsx(AppSessionHsx&,QWidget* parent=nullptr);
    void navigate(int page);
    ChatPageHsx* chat() const { return chat_; }
    NetworkPageHsx* network() const { return network_; }
signals:
    void returnToLogin();
protected:
    void closeEvent(QCloseEvent*) override;
private:
    void refresh();
    void counters();
    void updatePageIcon();
    // Short messages shown in the title row, where the status bar used to be.
    void notice(const QString& text,int ms);
    AppSessionHsx& session_;
    QStackedWidget* pages_;
    QWidget* header_;
    QVBoxLayout* bodyLayout_;
    QComboBox* services_;
    QLabel *title_,*caption_,*user_,*avatar_,*pageIcon_;
    QString currentTheme_;
    ServiceHsx displayedService_=ServiceHsx::Count;
    QPushButton *weiboInbox_,*weiboFollowing_;
    QLabel *brand_,*wordmark_,*quote_,*sideCategory_;
    std::array<QPushButton*,9> navigation_;
    std::array<QLabel*,2> badges_;
    ChatPageHsx* chat_;
    FriendsPageHsx* friends_;
    GroupsPageHsx* groups_;
    RequestsPageHsx* requests_;
    AccountsPageHsx* accounts_;
    DataPageHsx* data_;
    NetworkPageHsx* network_;
    StudioPageHsx* studio_;
    FeedPageHsx* feed_;
    FeedPageHsx* weibo_;
    QLabel *connection_,*notice_;
    class QTimer* noticeTimer_;
};
}
