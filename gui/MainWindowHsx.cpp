#include "MainWindowHsx.h"
#include "ChatPageHsx.h"
#include "FriendsPageHsx.h"
#include "GroupsPageHsx.h"
#include "RequestsPageHsx.h"
#include "AccountsPageHsx.h"
#include "DataPageHsx.h"
#include "NetworkPageHsx.h"
#include "ThemeHsx.h"
#include "IconHsx.h"
#include "UiHsx.h"
#include "StudioPageHsx.h"
#include "AvatarHsx.h"
#include "FeedPageHsx.h"
#include "WeiboThemeHsx.h"
#include "UpdateManagerHsx.h"
#include <QApplication>
#include <QTimer>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QFrame>
#include <QGuiApplication>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>
namespace im {
namespace {
const QStringList names{"会话","好友","群组","通知","账号","数据","朋友圈","装扮与会员","微博"};
const QStringList titles{"每一次相逢，都值得倾听","与你相伴的人","在这里，找到同频的人","相逢，正在发生","照顾好每一个自己","记录相逢的点滴","把生活分享给朋友","让你的每句话，都有自己的色彩","微博 · 发现新鲜事"};
const QStringList captions{"私语与群聊，让心意慢慢靠近","管理好友与联系，留住日常的温柔","一起交流、分享，也一起成长","好友邀请与群组申请，都在这里","个人资料与服务，在此从容打理","消息、好友与群组，汇成你的日常","文字与图片，仅你和好友可见","主题、气泡、头像与小火人，在这里相遇","公开分享、关注话题，与陌生人建立新的联系"};
const QStringList icons{"chat","users","group","bell","user","data","chat","shield","chat"};
}
MainWindowHsx::MainWindowHsx(AppSessionHsx& s,QWidget* parent):QMainWindow(parent),session_(s) {
    setWindowTitle(s.remote()?"听澜 · 山水有相逢":"听澜 · 离线课程演示");setWindowIcon(IconHsx::get("brand","#1689ff"));setMinimumSize(1180,740);
    const auto screen=QGuiApplication::primaryScreen()->availableGeometry();resize(qMin(1440,screen.width()-60),qMin(900,screen.height()-60));
    auto* central=new QWidget;setCentralWidget(central);auto* layout=new QHBoxLayout(central);layout->setContentsMargins(0,0,0,0);layout->setSpacing(0);
    auto* sidebar=new QWidget;sidebar->setObjectName("sidebar");sidebar->setFixedWidth(180);auto* side=new QVBoxLayout(sidebar);side->setContentsMargins(12,21,12,16);side->setSpacing(5);
    auto* brandRow=new QHBoxLayout;brandRow->setContentsMargins(12,0,0,0);auto* logo=new QLabel;logo->setPixmap(IconHsx::get("brand","#1689ff").pixmap(30,30));brandRow->addWidget(logo);auto* brand=new QLabel("听澜");brand_=brand;brand->setObjectName("brand");brandRow->addWidget(brand);brandRow->addStretch();side->addLayout(brandRow);
    auto* wordmark=new QLabel("       T I N G L A N");wordmark_=wordmark;wordmark->setStyleSheet("font-size:8px;color:#92acce;letter-spacing:3px;padding-top:8px;");side->addWidget(wordmark);side->addSpacing(12);
    auto* profile=new QFrame;profile->setObjectName("sidebarProfile");profile->setFixedHeight(64);auto* profileLayout=new QHBoxLayout(profile);profileLayout->setContentsMargins(8,8,6,8);avatar_=new QLabel;avatar_->setFixedSize(48,48);profileLayout->addWidget(avatar_);
    auto* profileText=new QVBoxLayout;profileText->setSpacing(4);user_=new QLabel;user_->setTextFormat(Qt::PlainText);user_->setStyleSheet("color:#233b5d;font-size:14px;font-weight:600;");profileText->addWidget(user_);auto* status=new QLabel("● 已登录");status->setStyleSheet("color:#25b57a;font-size:11px;");profileText->addWidget(status);profileLayout->addLayout(profileText,1);side->addWidget(profile);
    side->addSpacing(6);
    auto* navGroup=new QButtonGroup(this);navGroup->setExclusive(true);
    for(int i:{0,1,2,3,4,5,6,8,7}){
        if(i==4){if(i==4)side->addSpacing(10);auto* category=new QLabel(i==0?"   交流空间":"   我的听澜");sideCategory_=category;category->setStyleSheet("color:#95a6be;font-size:9px;letter-spacing:1px;padding-bottom:7px;");side->addWidget(category);}
        auto* button=new QPushButton("  "+names[i]);button->setObjectName("nav");button->setAccessibleName(names[i]);button->setCheckable(true);button->setIcon(IconHsx::get(icons[i],"#6580a6","#1689ff"));button->setIconSize(QSize(20,20));button->setCursor(Qt::PointingHandCursor);navGroup->addButton(button,i);navigation_[i]=button;
        if(i==0||i==3){auto* badge=new QLabel(button);badge->setFixedSize(20,20);badge->setAlignment(Qt::AlignCenter);badge->setStyleSheet("background:#ff515e;color:#ffffff;border-radius:10px;font-size:10px;font-weight:600;");badge->setAttribute(Qt::WA_TransparentForMouseEvents);auto* badgeRow=new QHBoxLayout(button);badgeRow->setContentsMargins(0,0,10,0);badgeRow->addStretch();badgeRow->addWidget(badge);badges_[i==0?0:1]=badge;}
        side->addWidget(button);connect(button,&QPushButton::clicked,this,[this,i]{navigate(i);if(i==8)weibo_->showHome();});
    }
    weiboFollowing_=new QPushButton("  ☆  我的关注");weiboFollowing_->setObjectName("weiboFollowingNav");side->addWidget(weiboFollowing_);
    weiboInbox_=new QPushButton("  ✉  微博私信");weiboInbox_->setObjectName("weiboInboxNav");side->addWidget(weiboInbox_);
    connect(weiboFollowing_,&QPushButton::clicked,this,[this]{navigate(8);weibo_->showFollowing();});
    connect(weiboInbox_,&QPushButton::clicked,this,[this]{navigate(8);weibo_->showInbox();});
    side->addStretch();
    layout->addWidget(sidebar);
    auto* area=new QWidget;auto* areaLayout=new QVBoxLayout(area);areaLayout->setContentsMargins(0,0,0,0);areaLayout->setSpacing(0);
    auto* header=new QFrame;header->setObjectName("header");header_=header;auto* h=new QHBoxLayout(header);h->setContentsMargins(29,20,29,18);
    pageIcon_=new QLabel;pageIcon_->setObjectName("pageIcon");pageIcon_->setFixedSize(48,48);pageIcon_->setAlignment(Qt::AlignCenter);h->addWidget(pageIcon_);h->addSpacing(14);auto* heading=new QVBoxLayout;heading->setSpacing(6);
    title_=new QLabel;title_->setObjectName("pageTitle");title_->setStyleSheet("font-size:23px;");caption_=new QLabel;caption_->setObjectName("muted");heading->addWidget(title_);heading->addWidget(caption_);h->addLayout(heading,1);
    // Current service and sign-out share one small card at the foot of the sidebar, above the closing line.
    auto* serviceCard=new QFrame;serviceCard->setObjectName("sidebarService");auto* serviceLayout=new QVBoxLayout(serviceCard);serviceLayout->setContentsMargins(10,8,8,8);serviceLayout->setSpacing(6);
    auto* serviceLabel=new QLabel("当前服务");serviceLayout->addWidget(serviceLabel);services_=new QComboBox;services_->setObjectName("activeService");services_->setMinimumWidth(0);auto* serviceRow=new QHBoxLayout;serviceRow->setSpacing(6);serviceRow->addWidget(services_,1);
    auto* switchUser=new QPushButton;switchUser->setObjectName("switchAccount");switchUser->setIcon(IconHsx::get("exit","#84987c"));switchUser->setIconSize(QSize(19,19));switchUser->setToolTip("切换账号 / 登出");switchUser->setAccessibleName("切换账号 / 登出");switchUser->setFixedSize(40,40);serviceRow->addWidget(switchUser);serviceLayout->addLayout(serviceRow);side->addWidget(serviceCard);
    auto* quote=new QLabel("山水有相逢。");quote_=quote;quote->setAlignment(Qt::AlignCenter);quote->setStyleSheet("font-family:'SimSun';font-size:12px;color:#8ba5c6;padding:10px 0 2px 0;letter-spacing:2px;");side->addWidget(quote);areaLayout->addWidget(header);
    pages_=new QStackedWidget;pages_->setObjectName("mainPages");chat_=new ChatPageHsx(s);friends_=new FriendsPageHsx(s);groups_=new GroupsPageHsx(s);requests_=new RequestsPageHsx(s);accounts_=new AccountsPageHsx(s);data_=new DataPageHsx(s);network_=new NetworkPageHsx(this);network_->hide();feed_=new FeedPageHsx(s,this);weibo_=new FeedPageHsx(s,this,"weibo");studio_=new StudioPageHsx(s);
    for(QWidget* page:std::initializer_list<QWidget*>{chat_,friends_,groups_,requests_,accounts_,data_,feed_,studio_,weibo_})pages_->addWidget(page);
    auto* body=new QWidget;auto* bodyLayout=new QVBoxLayout(body);bodyLayout_=bodyLayout;bodyLayout->setContentsMargins(26,24,26,22);bodyLayout->addWidget(pages_);areaLayout->addWidget(body,1);layout->addWidget(area,1);
#ifdef IM_USER_CLIENT
    auto* updates=new QPushButton("软件更新");updates->setObjectName("softwareUpdates");statusBar()->addPermanentWidget(updates);connect(updates,&QPushButton::clicked,this,[]{UpdateManagerHsx::showUpdates();});
#endif
    connection_=new QLabel(s.remote()?s.connectionStatus():"本机演示");connection_->setObjectName("connectionState");statusBar()->addPermanentWidget(connection_);connect(&s,&AppSessionHsx::connectionChanged,connection_,&QLabel::setText);
    statusBar()->setSizeGripEnabled(false);statusBar()->showMessage("  所有修改已保存");auto* footer=new QLabel("山水有相逢  ·  听澜     ");footer->setObjectName("muted");statusBar()->addPermanentWidget(footer);
    connect(services_,&QComboBox::currentIndexChanged,this,[this](int){if(services_->currentIndex()>=0)session_.switchService(static_cast<ServiceHsx>(services_->currentData().toInt()));});
    connect(switchUser,&QPushButton::clicked,this,[this]{emit returnToLogin();});connect(&s,&AppSessionHsx::sessionEnded,this,[this]{emit returnToLogin();});
    connect(friends_,&FriendsPageHsx::openChat,this,[this](int target){if(!session_.data().account(session_.user(),session_.service()).friends().count(target)){QMessageBox::information(this,"添加好友","先发送好友申请，等待对方同意后即可聊天。");return;}navigate(0);chat_->open(MessageKindHsx::Private,target);});
    connect(groups_,&GroupsPageHsx::openChat,this,[this](int target){if(!session_.data().group(target).contains(session_.user(),session_.service())){QMessageBox::information(this,"加入群组","请先通过当前服务加入此群。");return;}navigate(0);chat_->open(MessageKindHsx::Group,target);});
    connect(&s,&AppSessionHsx::changed,this,[this]{refresh();});connect(&s,&AppSessionHsx::countersChanged,this,[this]{counters();});
    connect(&s,&AppSessionHsx::failed,this,[this](const QString& message){statusBar()->showMessage(message,12000);QMessageBox::warning(this,"操作未完成",message);});connect(&s,&AppSessionHsx::succeeded,this,[this](const QString& message){statusBar()->showMessage(message,6000);});
    connect(chat_,&ChatPageHsx::openSection,this,&MainWindowHsx::navigate);
    connect(chat_,&ChatPageHsx::openStudio,this,[this]{navigate(7);});
    auto* expiry=new QTimer(this);expiry->setInterval(60000);connect(expiry,&QTimer::timeout,this,[this]{refresh();});expiry->start();
    refresh();navigate(session_.service()==ServiceHsx::Weibo?8:0);
}
void MainWindowHsx::refresh(){if(!session_.signedIn())return;const auto theme=session_.studio().choice(session_.user(),"theme");const bool blog=session_.service()==ServiceHsx::Weibo;const bool serviceChanged=displayedService_!=session_.service();
    if(currentTheme_!=theme||serviceChanged){currentTheme_=theme;ThemeHsx::install(*qApp,theme);setStyleSheet(ThemeHsx::workspace(theme)+(blog?weiboChrome():QString{}));qApp->setProperty("tinglanAccent",ThemeHsx::accent(theme));updatePageIcon();}
    for(int i=0;i<9;++i)navigation_[i]->setVisible(i==8?blog:(!blog||i==4||i==5||i==7));
    weiboFollowing_->setVisible(blog);weiboInbox_->setVisible(blog);navigation_[8]->setText("  微博首页");
    brand_->setText(blog?"绚色微博":"听澜");wordmark_->setText(blog?"    YVONNE / WEIBO":"       T I N G L A N");quote_->setText(blog?"灵感，正在连线。":"山水有相逢。");sideCategory_->setText(blog?"   频道与个人空间":"   我的听澜");
    user_->setStyleSheet(blog?"color:#f5d9eb;font-size:14px;font-weight:600;":"color:#233b5d;font-size:14px;font-weight:600;");
    setWindowTitle(blog?"听澜微博 · 绚色共振":"听澜 · 山水有相逢");
    if(serviceChanged){displayedService_=session_.service();navigate(blog?8:0);if(blog)weibo_->showHome();}
    AvatarHsx icon(session_.studio(),session_.user(),AppSessionHsx::q(session_.data().user(session_.user()).name()),48);avatar_->setPixmap(icon.grab());studio_->refresh();QSignalBlocker guard(services_);services_->clear();for(const auto& entry:session_.data().user(session_.user()).services())if(activeService(entry.first))services_->addItem(AppSessionHsx::q(serviceName(entry.first)),static_cast<int>(entry.first));services_->setCurrentIndex(services_->findData(static_cast<int>(session_.service())));user_->setText(AppSessionHsx::q(session_.data().user(session_.user()).name()));chat_->refresh();friends_->refresh();groups_->refresh();requests_->refresh();accounts_->refresh();data_->refresh();feed_->refresh();weibo_->refresh();counters();}
void MainWindowHsx::counters(){const int counts[]={session_.unreadCount(),session_.requestCount()};for(int i=0;i<2;++i){badges_[i]->setText(counts[i]>99?"99+":QString::number(counts[i]));badges_[i]->setVisible(counts[i]>0);}}
void MainWindowHsx::navigate(int page){if(page<0||page>=9)return;const bool blog=session_.service()==ServiceHsx::Weibo;if(page==8&&!blog)return;if(blog&&(page<=3||page==6))page=8;chat_->setActive(page==0);pages_->setCurrentIndex(page);header_->setVisible(page!=0);bodyLayout_->setContentsMargins(page==0?0:22,page==0?0:20,page==0?0:22,page==0?0:20);navigation_[page]->setChecked(true);title_->setText(page==8?"微博 / 绚色共振":titles[page]);caption_->setText(page==8?"发现  ·  关注  ·  与同频的人交换灵感":captions[page]);updatePageIcon();}
void MainWindowHsx::updatePageIcon(){
    const int page=pages_->currentIndex();if(page<0||page>=9)return;const bool blog=session_.service()==ServiceHsx::Weibo;
    pageIcon_->setStyleSheet(QString("background:%1;border-radius:15px;").arg(blog?"#f8e6f0":"#e6f1ff"));pageIcon_->setPixmap(IconHsx::get(icons[page],blog?"#c45c93":ThemeHsx::accent(currentTheme_)).pixmap(QSize(24,24),devicePixelRatioF()));
}
void MainWindowHsx::closeEvent(QCloseEvent* event){if(!session_.save()&&!UiHsx::confirm(this,"保存失败。确定关闭并丢弃尚未保存的改动？")){event->ignore();return;}network_->stop();event->accept();}
}
