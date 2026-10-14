#include "ChatPageHsx.h"
#include "ThemeHsx.h"
#include "IconHsx.h"
#include "ConversationDelegateHsx.h"
#include "BubbleHsx.h"
#include "AvatarHsx.h"
#include "ChatBackdropHsx.h"
#include "FireBuddyHsx.h"
#include <QCheckBox>
#include <QClipboard>
#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFrame>
#include <QFontMetrics>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
namespace im {
using S=AppSessionHsx;
namespace {
// The cover is decorative; it is not presented as media sent by a contact.
class ContactArtworkHsx final : public QWidget {
public:
    explicit ContactArtworkHsx(QWidget* parent=nullptr):QWidget(parent),cover_(":/art/mist-river.png") {setFixedHeight(196);}
    void setFace(const QPixmap& face) {face_=face;update();}
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::SmoothPixmapTransform);
        const QRectF cover(0,0,width(),148);QPainterPath clip;clip.addRoundedRect(cover,14,14);
        p.save();p.setClipPath(clip);p.fillRect(cover,QColor("#d8eaff"));
        if(!cover_.isNull()){const auto scaled=cover_.scaled(cover.size().toSize(),Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);p.drawPixmap(cover,scaled,QRectF((scaled.width()-cover.width())/2,(scaled.height()-cover.height())/2,cover.width(),cover.height()));}
        p.fillRect(cover,QColor(200,225,255,35));p.restore();
        p.setPen(Qt::NoPen);p.setBrush(Qt::white);p.drawEllipse(QRectF(13,101,92,92));
        if(!face_.isNull())p.drawPixmap(QRect(16,104,86,86),face_);
    }
private:
    QPixmap cover_,face_;
};
}

ChatPageHsx::ChatPageHsx(AppSessionHsx& s,QWidget* parent):QWidget(parent),session_(s) {
    auto* layout=new QHBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(0);
    auto* left=new QFrame;left->setObjectName("conversationPane");left->setFixedWidth(264);
    auto* ll=new QVBoxLayout(left);ll->setContentsMargins(0,18,0,12);ll->setSpacing(12);
    auto* searchRow=new QHBoxLayout;searchRow->setContentsMargins(14,0,14,0);searchRow->setSpacing(8);
    conversationSearch_=new QLineEdit;conversationSearch_->setObjectName("conversationSearch");conversationSearch_->setPlaceholderText("搜索联系人或群聊");
    conversationSearch_->setClearButtonEnabled(true);conversationSearch_->addAction(IconHsx::get("search","#8198b8"),QLineEdit::LeadingPosition);conversationSearch_->setMinimumWidth(0);searchRow->addWidget(conversationSearch_,1);
    auto* add=new QPushButton;add->setObjectName("chatIcon");add->setIcon(IconHsx::get("plus","#6686af"));add->setFixedSize(32,36);add->setToolTip("查找 / 添加好友");add->setAccessibleName("查找或添加好友");connect(add,&QPushButton::clicked,this,[this]{emit openSection(1);});searchRow->addWidget(add);ll->addLayout(searchRow);
    conversations_=new QListWidget;conversations_->setObjectName("conversations");conversations_->setItemDelegate(new ConversationDelegateHsx(conversations_));conversations_->setMouseTracking(true);conversations_->setSpacing(0);conversations_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);ll->addWidget(conversations_,1);
    auto* note=new QLabel("每一条消息，都是一次靠近");note->setAlignment(Qt::AlignCenter);note->setStyleSheet("color:#9aacc3;font-size:10px;padding:8px;");ll->addWidget(note);layout->addWidget(left);
    auto* right=new QFrame;right->setObjectName("chatPane");right->setMinimumWidth(350);auto* rl=new QVBoxLayout(right);rl->setContentsMargins(0,0,0,0);rl->setSpacing(0);
    auto* header=new QFrame;header->setObjectName("chatHeader");auto* top=new QHBoxLayout(header);top->setContentsMargins(18,14,18,14);top->setSpacing(10);
    headerAvatar_=new QLabel;headerAvatar_->setFixedSize(48,48);top->addWidget(headerAvatar_);
    auto* names=new QVBoxLayout;names->setSpacing(4);title_=new QLabel("选择一个会话");title_->setObjectName("chatTitle");title_->setTextFormat(Qt::PlainText);title_->setWordWrap(true);names->addWidget(title_);
    subtitle_=new QLabel("从左侧选择好友或群组");subtitle_->setTextFormat(Qt::PlainText);subtitle_->setWordWrap(true);subtitle_->setObjectName("chatSubtitle");subtitle_->setMaximumHeight(36);names->addWidget(subtitle_);top->addLayout(names,1);
    auto iconButton=[&](const QString& icon,const QString& tooltip){auto* button=new QPushButton;button->setObjectName("chatIcon");button->setFixedSize(32,34);button->setIcon(IconHsx::get(icon,"#6382aa","#1689ff"));button->setIconSize(QSize(22,22));button->setToolTip(tooltip);button->setAccessibleName(tooltip);button->setCursor(Qt::PointingHandCursor);return button;};
    auto* searchButton=iconButton("search","搜索聊天记录");top->addWidget(searchButton);
    auto* detailsButton=iconButton("user","显示或收起会话资料");detailsButton->setObjectName("toggleContactDetails");detailsButton->setStyleSheet("padding:0;border:none;background:transparent;");detailsButton->setCheckable(true);detailsButton->setChecked(true);top->addWidget(detailsButton);rl->addWidget(header);
    filters_=new QWidget;auto* filters=new QHBoxLayout(filters_);filters->setContentsMargins(18,9,18,9);
    search_=new QLineEdit;search_->setPlaceholderText("搜索当前会话的聊天记录");search_->setAccessibleName("搜索当前会话消息");search_->setClearButtonEnabled(true);search_->addAction(IconHsx::get("search","#8ca1be"),QLineEdit::LeadingPosition);filters->addWidget(search_);rl->addWidget(filters_);filters_->hide();
    connect(searchButton,&QPushButton::clicked,this,[this]{filters_->setVisible(filters_->isHidden());if(filters_->isVisible())search_->setFocus();else search_->clear();});
    scroll_=new QScrollArea;scroll_->setWidgetResizable(true);scroll_->setFrameShape(QFrame::NoFrame);scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);content_=new ChatBackdropHsx;bubbles_=new QVBoxLayout(content_);bubbles_->setContentsMargins(16,20,16,20);bubbles_->setSpacing(13);scroll_->setWidget(content_);rl->addWidget(scroll_,1);
    older_=new QPushButton("加载更早的消息");older_->setObjectName("quietButton");rl->addWidget(older_);
    auto* composerArea=new QWidget;auto* composerLayout=new QVBoxLayout(composerArea);composerLayout->setContentsMargins(14,9,14,10);composerLayout->setSpacing(6);
    auto* composerFrame=new QFrame;composerFrame->setObjectName("composerFrame");auto* bottom=new QHBoxLayout(composerFrame);bottom->setContentsMargins(9,8,9,8);bottom->setSpacing(5);
    auto* emoji=iconButton("smile","插入表情");emoji->setObjectName("emojiButton");emoji->setStyleSheet("padding:0;border:none;background:transparent;");bottom->addWidget(emoji);
    auto* dress=iconButton("palette","气泡、头像与背景装扮");dress->setObjectName("chatDress");dress->setStyleSheet("padding:0;border:none;background:transparent;");connect(dress,&QPushButton::clicked,this,&ChatPageHsx::openStudio);bottom->addWidget(dress);
    mentions_=new QComboBox;mentions_->setMaximumWidth(100);mentions_->hide();bottom->addWidget(mentions_);
    composer_=new QLineEdit;composer_->setObjectName("messageInput");composer_->setPlaceholderText("输入消息…");composer_->setMaxLength(5500);composer_->setMinimumWidth(60);composer_->setStyleSheet("background:#f0f5fb;border:none;border-radius:17px;padding:9px 12px;font-size:13px;");bottom->addWidget(composer_,1);
    send_=new QPushButton;send_->setObjectName("sendMessage");send_->setProperty("primary",true);send_->setIcon(IconHsx::get("send","#ffffff"));send_->setIconSize(QSize(23,23));send_->setFixedSize(44,44);send_->setToolTip("发送消息（Enter）");send_->setAccessibleName("发送消息");bottom->addWidget(send_);composerLayout->addWidget(composerFrame);
    auto* foot=new QLabel("Enter 发送 · 右键消息可收藏、复制或撤回");foot->setStyleSheet("color:#96a9c1;font-size:9px;");foot->setAlignment(Qt::AlignCenter);composerLayout->addWidget(foot);rl->addWidget(composerArea);layout->addWidget(right,1);
    connect(emoji,&QPushButton::clicked,this,[this,emoji]{QMenu menu;for(const auto& symbol:QStringList{"😊","🎉","❤️","👍","🌷","🌙","✨","🌈"}){auto* action=menu.addAction(symbol);connect(action,&QAction::triggered,this,[this,symbol]{composer_->insert(symbol);composer_->setFocus();});}menu.exec(emoji->mapToGlobal(QPoint(0,-menu.sizeHint().height())));});
    auto* panel=new QFrame;panel->setObjectName("contactPanel");panel->setFixedWidth(280);contactPanel_=panel;
    auto* panelLayout=new QVBoxLayout(panel);panelLayout->setContentsMargins(0,0,0,0);
    auto* panelScroll=new QScrollArea;panelScroll->setWidgetResizable(true);panelScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);panelScroll->setFrameShape(QFrame::NoFrame);panelLayout->addWidget(panelScroll);
    auto* panelContent=new QWidget;panelContent->setObjectName("contactContent");panelContent->setStyleSheet("QWidget#contactContent {background:#f8fbff;}");panelScroll->setWidget(panelContent);
    auto* pl=new QVBoxLayout(panelContent);pl->setContentsMargins(14,12,14,16);pl->setSpacing(11);
    contactArtwork_=new ContactArtworkHsx;pl->addWidget(contactArtwork_);
    contactName_=new QLabel;contactName_->setObjectName("contactName");contactName_->setTextFormat(Qt::PlainText);contactName_->setWordWrap(true);pl->addWidget(contactName_);
    contactDetails_=new QLabel;contactDetails_->setObjectName("contactDetails");contactDetails_->setTextFormat(Qt::PlainText);contactDetails_->setWordWrap(true);pl->addWidget(contactDetails_);
    auto* quote=new QLabel("山不见我，我自去见山。");quote->setObjectName("contactQuote");quote->setWordWrap(true);pl->addWidget(quote);
    auto* quick=new QHBoxLayout;quick->setSpacing(7);profileButton_=new QPushButton("查看资料");profileButton_->setIcon(IconHsx::get("user","#1689ff"));profileButton_->setStyleSheet("padding:13px 7px;");quick->addWidget(profileButton_);
    auto* manage=new QPushButton("管理会话");manage->setIcon(IconHsx::get("users","#1689ff"));manage->setStyleSheet("padding:13px 7px;");connect(manage,&QPushButton::clicked,this,[this]{emit openSection(target_&&kind_==MessageKindHsx::Group?2:1);});quick->addWidget(manage);pl->addLayout(quick);
    auto* section=new QLabel("相逢的记录");section->setObjectName("contactSection");pl->addWidget(section);
    contactStats_=new QLabel;contactStats_->setObjectName("contactStats");contactStats_->setWordWrap(true);pl->addWidget(contactStats_);
    buddy_=new FireBuddyHsx;buddy_->hide();pl->addWidget(buddy_);
    auto* historyButton=new QPushButton("聊天记录");historyButton->setObjectName("contactAction");historyButton->setIcon(IconHsx::get("chat","#6585ad"));pl->addWidget(historyButton);connect(historyButton,&QPushButton::clicked,this,[this]{filters_->show();search_->setFocus();});
    auto* exportButton=new QPushButton("导出记录");exportButton->setObjectName("contactAction");exportButton->setIcon(IconHsx::get("download","#6585ad"));pl->addWidget(exportButton);
    auto* studioButton=new QPushButton("气泡与背景装扮");studioButton->setObjectName("contactAction");studioButton->setIcon(IconHsx::get("palette","#6585ad"));pl->addWidget(studioButton);connect(studioButton,&QPushButton::clicked,this,&ChatPageHsx::openStudio);
    favorites_=new QCheckBox("仅看收藏");pl->addWidget(favorites_);pl->addStretch();layout->addWidget(panel);
    reflow_=new QTimer(this);reflow_->setSingleShot(true);reflow_->setInterval(80);
    connect(reflow_,&QTimer::timeout,this,[this]{if(!active_||renderedWidth_==scroll_->viewport()->width())return;const auto* bar=scroll_->verticalScrollBar();const double position=bar->maximum()?double(bar->value())/bar->maximum():1;showHistory();QTimer::singleShot(0,this,[this,position]{auto* bar=scroll_->verticalScrollBar();bar->setValue(qRound(position*bar->maximum()));});});
    connect(&session_,&AppSessionHsx::operationRejected,this,[this](const QJsonObject& request){if(request["op"]=="chat.send"&&request["target"].toInt()==target_&&request["kind"].toInt()==int(kind_)&&request["service"].toInt()==int(session_.service())&&composer_->text().isEmpty())composer_->setText(request["text"].toString());});
    connect(detailsButton,&QPushButton::toggled,panel,&QWidget::setVisible);
    connect(detailsButton,&QPushButton::toggled,this,[this]{reflow_->start();});
    connect(profileButton_,&QPushButton::clicked,this,[this]{if(target_)QMessageBox::information(this,"会话资料",contactName_->text()+"\n\n"+contactDetails_->text()+"\n\n"+contactStats_->text());});
    connect(conversations_,&QListWidget::currentItemChanged,this,[this](QListWidgetItem* item){if(!item)return;kind_=static_cast<MessageKindHsx>(item->data(Qt::UserRole).toInt());target_=item->data(Qt::UserRole+1).toInt();limit_=40;showHistory();});
    connect(conversationSearch_,&QLineEdit::textChanged,this,[this](const QString& term){for(int i=0;i<conversations_->count();++i){auto* item=conversations_->item(i);item->setHidden(!(item->data(Qt::UserRole+2).toString()+item->data(Qt::UserRole+3).toString()).contains(term,Qt::CaseInsensitive));}});
    connect(search_,&QLineEdit::textChanged,this,[this]{showHistory();});connect(favorites_,&QCheckBox::toggled,this,[this]{showHistory();});connect(send_,&QPushButton::clicked,this,[this]{send();});connect(composer_,&QLineEdit::returnPressed,this,[this]{send();});connect(older_,&QPushButton::clicked,this,[this]{limit_+=40;showHistory();});
    connect(exportButton,&QPushButton::clicked,this,[this]{if(!target_)return;const auto file=QFileDialog::getSaveFileName(this,"导出聊天记录","聊天记录.txt","文本文件 (*.txt)");if(file.isEmpty())return;try{session_.messages().exportHistory(session_.user(),session_.service(),kind_,target_,S::path(file));QMessageBox::information(this,"导出完成","聊天记录已保存");}catch(const std::exception& e){QMessageBox::warning(this,"导出失败",S::q(e.what()));}});
}
void ChatPageHsx::resizeEvent(QResizeEvent* event){QWidget::resizeEvent(event);if(reflow_)reflow_->start();}
void ChatPageHsx::setActive(bool active){active_=active;if(active)showHistory();}
void ChatPageHsx::refresh() {
    if(user_!=session_.user()||service_!=session_.service()){target_=0;user_=session_.user();service_=session_.service();search_->clear();conversationSearch_->clear();}
    QSignalBlocker guard(conversations_);conversations_->clear();if(!session_.signedIn())return;const auto& account=session_.data().account(user_,service_);
    auto add=[&](const QString& name,MessageKindHsx kind,int target,const QString& detail){
        int unread=0;const auto history=session_.messages().history(user_,service_,kind,target);for(auto* message:history)if(message->sender()!=user_&&!message->read(user_))++unread;
        const QString preview=history.empty()?detail:history.back()->recalled()?"消息已撤回":S::q(history.back()->text());
        auto* item=new QListWidgetItem(name+"\n"+preview,conversations_);item->setData(Qt::UserRole,static_cast<int>(kind));item->setData(Qt::UserRole+1,target);item->setData(Qt::UserRole+2,name);item->setData(Qt::UserRole+3,preview);item->setData(Qt::UserRole+4,unread);item->setData(Qt::UserRole+5,history.empty()?QString{}:QDateTime::fromSecsSinceEpoch(history.back()->created()).toString("HH:mm"));item->setSizeHint(QSize(0,86));if(kind==MessageKindHsx::Private){AvatarHsx icon(session_.studio(),target,name,52);item->setData(Qt::UserRole+6,icon.grab());}
        item->setHidden(!(name+preview).contains(conversationSearch_->text(),Qt::CaseInsensitive));if(kind==kind_&&target==target_)conversations_->setCurrentItem(item);
    };
    for(const auto& [id,friendship]:account.friends()){const auto& user=session_.data().user(id);add(S::q(friendship.remark().empty()?user.name():friendship.remark()),MessageKindHsx::Private,id,"好友 · "+S::q(serviceName(friendship.contact())));}
    for(const auto& [id,group]:session_.data().groups())if(group->contains(user_,service_))add(S::q(group->name()),MessageKindHsx::Group,id,QString("群聊 · %1 人").arg(group->members().size()));
    if(!conversations_->currentItem())target_=0;showHistory();
}
void ChatPageHsx::open(MessageKindHsx kind,int target){kind_=kind;target_=target;refresh();}
void ChatPageHsx::showHistory() {
    // A service switch can trigger layout/navigation before refresh updates this view.
    // Never read or acknowledge an old conversation using the new session identity.
    if(user_!=session_.user()||service_!=session_.service())return;
    renderedWidth_=scroll_->viewport()->width();
    while(auto* item=bubbles_->takeAt(0)){if(item->widget())delete item->widget();delete item;}
    composer_->setEnabled(target_!=0);send_->setEnabled(target_!=0);mentions_->setVisible(target_&&kind_==MessageKindHsx::Group);older_->hide();refreshDetails();
    static_cast<ChatBackdropHsx*>(content_)->setBackground(session_.studio().media(session_.user(),"background"));buddy_->setVisible(target_&&kind_==MessageKindHsx::Private);if(target_&&kind_==MessageKindHsx::Private)buddy_->setState(session_.studio().streak(target_),StudioStoreHsx::item("pet",session_.studio().choice(session_.user(),"pet")));
    if(!target_){
        title_->setText("山水有相逢");subtitle_->setText("选择一位好友，让故事从一句问候开始");
        auto* empty=new QWidget;auto* layout=new QVBoxLayout(empty);layout->setAlignment(Qt::AlignCenter);layout->setSpacing(18);
        auto* symbol=new QLabel;symbol->setAlignment(Qt::AlignCenter);symbol->setPixmap(IconHsx::get("brand","#9ac5ee").pixmap(76,76));layout->addWidget(symbol);
        auto* title=new QLabel("心意，在这里相遇");title->setAlignment(Qt::AlignCenter);title->setStyleSheet("font-family:'SimSun';font-size:25px;color:#729ac3;letter-spacing:2px;");layout->addWidget(title);
        auto* detail=new QLabel("不必匆忙，一句问候也很好。");detail->setAlignment(Qt::AlignCenter);detail->setStyleSheet("font-size:12px;color:#a2b4ca;");layout->addWidget(detail);bubbles_->addWidget(empty,1);return;
    }
    if(kind_==MessageKindHsx::Private){const auto& user=session_.data().user(target_);title_->setText(S::q(user.name()));subtitle_->setText((session_.remote()?"服务器会话 · ":"本地会话 · ")+S::q(serviceName(service_))+" · "+S::q(user.location()));}
    else{const auto& group=session_.data().group(target_);title_->setText(S::q(group.name()));subtitle_->setText(QString("%1 位成员 · %2%3").arg(group.members().size()).arg(S::q(modeName(group.policy().mode()))).arg(group.announcement().empty()?"":" · "+S::q(group.announcement())));mentions_->clear();mentions_->addItem("不 @",0);if(group.manages(user_))mentions_->addItem("@ 所有人",-1);for(const auto& entry:group.members())if(entry.first!=user_)mentions_->addItem("@ "+S::q(session_.data().user(entry.first).name()),entry.first);}
    const auto all=session_.messages().history(user_,service_,kind_,target_,S::s(search_->text()));std::vector<const MessageHsx*> history;for(auto* message:all)if(!favorites_->isChecked()||message->starred(user_))history.push_back(message);
    const auto begin=history.size()>static_cast<size_t>(limit_)?history.size()-limit_:0;older_->setVisible(begin>0);std::vector<long long> displayed;QDate lastDate;
    for(size_t i=begin;i<history.size();++i){
        const auto* message=history[i];displayed.push_back(message->id());const bool mine=message->sender()==user_;const auto timestamp=QDateTime::fromSecsSinceEpoch(message->created());
        if(timestamp.date()!=lastDate){lastDate=timestamp.date();auto* stamp=new QLabel(lastDate==QDate::currentDate()?"今天":lastDate.toString("yyyy年M月d日"));stamp->setAlignment(Qt::AlignCenter);stamp->setStyleSheet("color:#6b7f91;background:rgba(255,255,255,185);border-radius:7px;font-size:10px;padding:4px 12px;");bubbles_->addWidget(stamp,0,Qt::AlignHCenter);}
        auto* row=new QWidget;auto* rowLayout=new QHBoxLayout(row);rowLayout->setContentsMargins(1,3,1,3);rowLayout->setSpacing(7);auto* column=new QVBoxLayout;column->setSpacing(4);
        auto* meta=new QLabel(S::q(session_.data().user(message->sender()).name())+"  "+timestamp.toString("HH:mm")+(message->starred(user_)?"  · 收藏":"")+(mine&&kind_==MessageKindHsx::Private?(message->read(target_)?"  · 已读":"  · 未读"):""));meta->setTextFormat(Qt::PlainText);meta->setStyleSheet("font-size:10px;color:#61728c;background:transparent;padding:1px 4px;");meta->setAlignment(mine?Qt::AlignRight:Qt::AlignLeft);
        auto* bubble=new BubbleHsx(message->recalled()?"消息已撤回":S::q(message->text()),StudioStoreHsx::item("bubble",session_.studio().choice(message->sender(),"bubble")),mine);
        bubble->ensurePolished();const int maxWidth=std::clamp(scroll_->viewport()->width()-110,150,470);bubble->setFixedWidth(std::clamp(bubble->fontMetrics().horizontalAdvance(bubble->text())+50,100,maxWidth));column->addWidget(bubble,0,mine?Qt::AlignRight:Qt::AlignLeft);column->addWidget(meta,0,mine?Qt::AlignRight:Qt::AlignLeft);
        bubble->setContextMenuPolicy(Qt::CustomContextMenu);const auto id=message->id();
        connect(bubble,&QWidget::customContextMenuRequested,this,[this,bubble,id,mine](QPoint pos){const auto& current=session_.data().message(id);QMenu menu;auto* copy=menu.addAction("复制消息");auto* star=menu.addAction(current.starred(user_)?"取消收藏":"收藏消息");star->setEnabled(!current.recalled());QAction* recall=nullptr;if(mine){recall=menu.addAction("撤回消息（2分钟内）");recall->setEnabled(!current.recalled());}auto* chosen=menu.exec(bubble->mapToGlobal(pos));if(chosen==copy)QApplication::clipboard()->setText(S::q(current.text()));else if(chosen==star)session_.command("chat.star",{{"message",QString::number(id)},{"enabled",!current.starred(user_)}},[&]{session_.messages().star(user_,service_,id,!current.starred(user_));});else if(recall&&chosen==recall)session_.command("chat.recall",{{"message",QString::number(id)}},[&]{session_.messages().recall(user_,id);});});
        auto* avatar=new AvatarHsx(session_.studio(),message->sender(),S::q(session_.data().user(message->sender()).name()),38);
        if(mine){rowLayout->addStretch();rowLayout->addLayout(column);rowLayout->addWidget(avatar,0,Qt::AlignTop);}else{rowLayout->addWidget(avatar,0,Qt::AlignTop);rowLayout->addLayout(column);rowLayout->addStretch();}bubbles_->addWidget(row);
    }
    if(history.empty()){auto* label=new QLabel("还没有消息，发送第一句问候吧。");label->setAlignment(Qt::AlignCenter);label->setObjectName("emptyState");bubbles_->addWidget(label);}
    bubbles_->addStretch();QTimer::singleShot(0,this,[this]{scroll_->verticalScrollBar()->setValue(scroll_->verticalScrollBar()->maximum());});
    if(active_){session_.markDisplayed(displayed);if(auto* item=conversations_->currentItem()){int unread=0;for(auto* message:session_.messages().history(user_,service_,kind_,target_))if(message->sender()!=user_&&!message->read(user_))++unread;item->setData(Qt::UserRole+4,unread);conversations_->viewport()->update();}}
}

void ChatPageHsx::refreshDetails() {
    QString name="听澜",details="选择好友或群组，查看会话资料。";
    QPixmap face;profileButton_->setEnabled(target_!=0);
    if(target_) {
        if(kind_==MessageKindHsx::Private) {
            const auto& user=session_.data().user(target_);name=S::q(user.name());
            const auto& friends=session_.data().account(user_,service_).friends();
            const auto it=friends.find(target_);
            details="地区   "+S::q(user.location())+"\n服务   "+S::q(serviceName(service_));
            if(it!=friends.end())details+="\n备注   "+(it->second.remark().empty()?QString("未设置"):S::q(it->second.remark()))+"\n分组   "+S::q(it->second.category());
            AvatarHsx avatar(session_.studio(),target_,name,96);face=avatar.grab();
        } else {
            const auto& group=session_.data().group(target_);name=S::q(group.name());
            details=QString("群成员   %1 人\n服务   %2\n公告   %3").arg(group.members().size()).arg(S::q(serviceName(service_))).arg(group.announcement().empty()?QString("暂无公告"):S::q(group.announcement()));
            face=IconHsx::get("group","#1689ff").pixmap(80,80);
        }
        int stars=0;const auto history=session_.messages().history(user_,service_,kind_,target_);for(const auto* message:history)if(message->starred(user_))++stars;
        contactStats_->setText(QString("%1 条消息    ·    %2 条收藏").arg(history.size()).arg(stars));
    } else {face=IconHsx::get("brand","#1689ff").pixmap(80,80);contactStats_->setText("从一句问候，开始新的故事。");}
    contactName_->setText(name);contactDetails_->setText(details);
    const auto dpr=devicePixelRatioF();auto headerFace=face.scaled(qRound(48*dpr),qRound(48*dpr),Qt::KeepAspectRatio,Qt::SmoothTransformation);headerFace.setDevicePixelRatio(dpr);headerAvatar_->setPixmap(headerFace);
    static_cast<ContactArtworkHsx*>(contactArtwork_)->setFace(face);
}

void ChatPageHsx::send(){if(!target_||composer_->text().trimmed().isEmpty())return;const auto text=S::s(composer_->text());if(session_.command("chat.send",{{"kind",int(kind_)},{"target",target_},{"text",composer_->text()},{"mention",mentions_->currentData().toInt()}},[&]{if(kind_==MessageKindHsx::Private)session_.messages().sendPrivate(user_,service_,target_,text);else session_.messages().sendGroup(user_,service_,target_,text,mentions_->currentData().toInt());}))composer_->clear();}
}
