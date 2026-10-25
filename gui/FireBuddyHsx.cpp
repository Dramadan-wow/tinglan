#include "FireBuddyHsx.h"
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <algorithm>
#include <cmath>
namespace im {
namespace {
constexpr int Compact=92,Tall=104,Speech=200;
const char* Tip="双方每天各发送至少一条私聊消息，点亮当天火苗；连续满 7 天提升一级。\n今日只有单方发送时保留昨天的连续天数；漏掉一个完整自然日后重新开始。\n按当前服务统计，已撤回的消息不计入。点击小火人可以打招呼。";
}
FireBuddyHsx::FireBuddyHsx(QWidget* parent,Mode mode):QWidget(parent),mode_(mode),timer_(new QTimer(this)){
    setObjectName(mode==Mode::Floating?"floatingBuddy":mode==Mode::Swatch?"buddySwatch":"fireBuddy");timer_->setInterval(80);
    connect(timer_,&QTimer::timeout,this,[this]{phase_+=.15;if(greeting_>0&&--greeting_==0&&mode_==Mode::Floating)place();update();});
    if(mode==Mode::Card){setMinimumSize(238,61);setMaximumHeight(65);setCursor(Qt::PointingHandCursor);}
    else if(mode==Mode::Swatch){setFixedSize(72,78);setAttribute(Qt::WA_TransparentForMouseEvents);}
    else{setAttribute(Qt::WA_TranslucentBackground);setCursor(Qt::OpenHandCursor);setFixedSize(Compact,Tall);if(parent)parent->installEventFilter(this);}
}
void FireBuddyHsx::setState(const QJsonObject& state,const QJsonObject& skin){
    state_=state;skin_=skin;if(mode_!=Mode::Swatch)setToolTip(Tip);
    setAccessibleName(state.value("preview").toBool()?"小火人外观预览":QString("小火人，连续聊天 %1 天，等级 %2").arg(state.value("days").toInt()).arg(state.value("level").toInt(1)));update();
}
void FireBuddyHsx::setOutfit(const QJsonObject& outfit){outfit_=outfit;update();}
void FireBuddyHsx::showEvent(QShowEvent* event){QWidget::showEvent(event);if(mode_!=Mode::Swatch)timer_->start();if(mode_==Mode::Floating)place();}
void FireBuddyHsx::hideEvent(QHideEvent* event){timer_->stop();QWidget::hideEvent(event);}
bool FireBuddyHsx::eventFilter(QObject* watched,QEvent* event){if(watched==parentWidget()&&event->type()==QEvent::Resize)place();return QWidget::eventFilter(watched,event);}
void FireBuddyHsx::place(){
    auto* host=parentWidget();if(!host)return;const int w=greeting_?Compact+Speech:Compact;setFixedSize(w,Tall);
    margin_.setX(std::clamp(margin_.x(),4,std::max(4,host->width()-w-4)));margin_.setY(std::clamp(margin_.y(),4,std::max(4,host->height()-Tall-4)));
    move(host->width()-margin_.x()-w,host->height()-margin_.y()-Tall);raise();
}
void FireBuddyHsx::greet(){
    greeting_=mode_==Mode::Floating?45:30;++pokes_;if(mode_==Mode::Floating)place();update();
}
void FireBuddyHsx::mousePressEvent(QMouseEvent* event){
    if(mode_!=Mode::Floating){greet();return;}
    pressed_=event->globalPosition().toPoint();dragMargin_=margin_;dragging_=false;
}
void FireBuddyHsx::mouseMoveEvent(QMouseEvent* event){
    if(mode_!=Mode::Floating||!(event->buttons()&Qt::LeftButton))return;const auto delta=event->globalPosition().toPoint()-pressed_;
    if(!dragging_&&delta.manhattanLength()<5)return;if(!dragging_){dragging_=true;setCursor(Qt::ClosedHandCursor);}
    margin_=dragMargin_-delta;place();
}
void FireBuddyHsx::mouseReleaseEvent(QMouseEvent*){
    if(mode_!=Mode::Floating)return;if(!dragging_)greet();dragging_=false;setCursor(Qt::OpenHandCursor);
}
QString FireBuddyHsx::statusLine() const{
    const int today=state_.value("today").toInt();
    return today==3?"今日已点亮":today==1?"今天还差对方的一条消息":today==2?"今天还差你的一条消息":"双方今天各发一条消息，即可点亮";
}
// The flame is drawn in a 48×58 design box; outfits use the same coordinates so they follow the bobbing.
void FireBuddyHsx::drawBuddy(QPainter& p,const QRectF& box,bool active) const{
    p.save();const double s=std::min(box.width()/48,box.height()/58);p.translate(box.center().x()-24*s,box.center().y()-29*s);p.scale(s,s);p.setPen(Qt::NoPen);
    QPainterPath flame;flame.moveTo(24,1);flame.cubicTo(29,16,43,17,43,32);flame.cubicTo(44,55,3,57,4,32);flame.cubicTo(5,23,9,18,14,16);flame.cubicTo(11,27,20,23,24,1);
    QLinearGradient fill(0,0,0,52);fill.setColorAt(0,active?QColor(skin_.value("a").toString("#ff963b")):QColor("#aab5c6"));fill.setColorAt(1,active?QColor(skin_.value("b").toString("#ffd56e")):QColor("#d9e0eb"));p.setBrush(fill);p.drawPath(flame);
    p.setBrush(QColor(255,255,255,115));p.drawEllipse(QRectF(10,27,27,22));p.setPen(QPen(QColor("#6b4d43"),2,Qt::SolidLine,Qt::RoundCap));
    const bool blink=mode_!=Mode::Swatch&&std::fmod(phase_,6)>5.7;if(blink){p.drawLine(QPointF(16,34),QPointF(19,34));p.drawLine(QPointF(29,34),QPointF(32,34));}else{p.drawPoint(QPointF(18,34));p.drawPoint(QPointF(30,34));}
    p.drawArc(QRectF(21,35,7,6),180*16,180*16);p.setPen(Qt::NoPen);
    const auto id=outfit_.value("id").toString();const QColor a(outfit_.value("a").toString("#ffffff")),b(outfit_.value("b").toString("#ffffff")),ink(outfit_.value("ink").toString("#ffffff"));
    if(id=="scarf"){QPainterPath band;band.addRoundedRect(QRectF(7,42,34,7),3.5,3.5);p.setBrush(a);p.drawPath(band);p.save();p.setClipPath(band);p.setBrush(b);for(int i=0;i<5;++i)p.drawRect(QRectF(9+i*7,42,2.6,7));p.restore();
        QPainterPath tail;tail.addRoundedRect(QRectF(29,46,7,12),3,3);p.setBrush(a);p.drawPath(tail);p.setBrush(ink);p.drawRect(QRectF(29.5,54.5,6,1.6));}
    else if(id=="bow"){QPainterPath left,right;left.moveTo(33,15);left.cubicTo(27,9,24,12,25,16);left.cubicTo(25,20,29,20,33,15);right.moveTo(33,15);right.cubicTo(39,9,42,12,41,16);right.cubicTo(41,20,37,20,33,15);
        p.setBrush(a);p.drawPath(left);p.drawPath(right);p.setBrush(b);p.drawEllipse(QPointF(27.5,14.8),1.4,1.4);p.drawEllipse(QPointF(38.5,14.8),1.4,1.4);p.setBrush(ink);p.drawEllipse(QPointF(33,15),2.2,2.2);}
    else if(id=="straw"){p.setBrush(b);p.drawEllipse(QRectF(5,14,38,7.5));QPainterPath top;top.addRoundedRect(QRectF(14.5,6.5,19,11),5,5);p.setBrush(a);p.drawPath(top);p.setBrush(ink);p.drawRect(QRectF(14.5,13,19,2.6));p.setBrush(a.lighter(106));p.drawEllipse(QRectF(9,16.5,30,3.6));}
    else if(id=="glasses"){p.setBrush(QColor(b.red(),b.green(),b.blue(),110));p.setPen(QPen(a,1.6));p.drawEllipse(QPointF(18,34),4.6,4.6);p.drawEllipse(QPointF(30,34),4.6,4.6);
        p.drawLine(QPointF(22.6,33.2),QPointF(25.4,33.2));p.drawLine(QPointF(13.4,33),QPointF(10.2,31.4));p.drawLine(QPointF(34.6,33),QPointF(37.8,31.4));p.setPen(Qt::NoPen);}
    else if(id=="headphones"){p.setBrush(Qt::NoBrush);p.setPen(QPen(a,3,Qt::SolidLine,Qt::RoundCap));p.drawArc(QRectF(6,10,36,40),20*16,140*16);p.setPen(Qt::NoPen);p.setBrush(a);
        p.drawRoundedRect(QRectF(2.5,25,8,13),3.5,3.5);p.drawRoundedRect(QRectF(37.5,25,8,13),3.5,3.5);p.setBrush(b);p.drawRoundedRect(QRectF(4.5,27.5,4,8),2,2);p.drawRoundedRect(QRectF(39.5,27.5,4,8),2,2);p.setBrush(ink);p.drawEllipse(QPointF(6.5,37.5),1.2,1.2);}
    else if(id=="crown"){QPainterPath c;c.moveTo(15,13);c.lineTo(14,4);c.lineTo(19.5,8.5);c.lineTo(24,1.5);c.lineTo(28.5,8.5);c.lineTo(34,4);c.lineTo(33,13);c.closeSubpath();
        QLinearGradient gold(0,1,0,13);gold.setColorAt(0,b);gold.setColorAt(1,a);p.setBrush(gold);p.setPen(QPen(a.darker(115),.8));p.drawPath(c);p.setPen(Qt::NoPen);p.setBrush(ink);p.drawEllipse(QPointF(24,9.5),1.6,1.6);p.setBrush(QColor("#7bc6ff"));p.drawEllipse(QPointF(18.5,10.5),1.1,1.1);p.drawEllipse(QPointF(29.5,10.5),1.1,1.1);}
    p.restore();
}
void FireBuddyHsx::paintEvent(QPaintEvent*){
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);const bool active=state_.value("active").toBool()||mode_==Mode::Swatch;const double bob=std::sin(phase_)*(greeting_?3:1.2);
    if(mode_==Mode::Swatch){drawBuddy(p,QRectF(8,4,56,68),true);return;}
    if(mode_==Mode::Floating){
        const double right=width()-Compact;
        if(greeting_){
            // Speech bubble to the left of the flame, with a small tail pointing at it.
            const QRectF bubble(4,16,Speech-14,62);QPainterPath shape;shape.addRoundedRect(bubble,16,16);QPainterPath tail;tail.moveTo(bubble.right()-2,40);tail.lineTo(bubble.right()+9,47);tail.lineTo(bubble.right()-2,53);tail.closeSubpath();shape=shape.united(tail);
            p.setPen(QPen(QColor("#e2ebf7"),1));p.setBrush(QColor(255,255,255,245));p.drawPath(shape);
            static const QStringList lines{"在呢，今天也要好好聊天！","被你戳到啦～","一起把今天的火苗点亮吧"};
            QFont title("Microsoft YaHei UI");title.setPixelSize(12);title.setWeight(QFont::DemiBold);p.setFont(title);p.setPen(QColor("#5b4334"));p.drawText(bubble.adjusted(14,10,-10,-30),Qt::AlignLeft|Qt::AlignVCenter,lines[(pokes_-1+lines.size())%lines.size()]);
            title.setPixelSize(10);title.setWeight(QFont::Normal);p.setFont(title);p.setPen(QColor("#9a897b"));
            const QString detail=active?QString("连续 %1 天 · Lv.%2 · %3").arg(state_.value("days").toInt()).arg(state_.value("level").toInt(1)).arg(statusLine()):statusLine();
            p.drawText(bubble.adjusted(14,32,-10,-8),Qt::AlignLeft|Qt::AlignVCenter,p.fontMetrics().elidedText(detail,Qt::ElideRight,int(bubble.width())-24));
        }
        p.setPen(Qt::NoPen);p.setBrush(QColor(40,80,140,active?38:26));const double shadow=40-bob*2;p.drawEllipse(QPointF(right+Compact/2.0,Tall-9),shadow/2,4.5);
        drawBuddy(p,QRectF(right+12,8+bob,68,82),active);
        QFont badge("Microsoft YaHei UI");badge.setPixelSize(9);badge.setWeight(QFont::DemiBold);p.setFont(badge);
        const QString label=active?QString("Lv.%1").arg(state_.value("level").toInt(1)):"zZ";const QRectF pill(right+Compact-34,6,30,15);
        p.setBrush(active?QColor("#ff8a3d"):QColor("#c3cbd8"));p.drawRoundedRect(pill,7.5,7.5);p.setPen(Qt::white);p.drawText(pill,Qt::AlignCenter,label);return;
    }
    p.setPen(Qt::NoPen);p.setBrush(active?QColor("#fff5e8"):QColor("#f2f4f8"));p.drawRoundedRect(rect().adjusted(0,0,-1,-1),12,12);
    drawBuddy(p,QRectF(9,5+bob,48,58),active);
    const auto outfit=outfit_.value("id").toString();const QString dressed=outfit.isEmpty()||outfit=="none"?QString{}:" · "+outfit_.value("name").toString();
    p.setPen(QColor("#684c3a"));QFont title("Microsoft YaHei UI");title.setPixelSize(12);title.setWeight(QFont::DemiBold);p.setFont(title);
    p.drawText(QRect(64,9,width()-69,23),Qt::AlignVCenter,greeting_?"在呢，今天也要好好聊天！":state_.value("preview").toBool()?"外观预览 · "+skin_.value("name").toString()+dressed:active?QString("小火人  Lv.%1 · 连续 %2 天").arg(state_.value("level").toInt(1)).arg(state_.value("days").toInt()):"小火人 · 等一句双向的问候");
    title.setPixelSize(10);title.setWeight(QFont::Normal);p.setFont(title);p.setPen(QColor("#9a897b"));const int today=state_.value("today").toInt();
    p.drawText(QRect(64,32,width()-69,20),Qt::AlignVCenter,state_.value("preview").toBool()?"真实连续天数请在好友私聊中查看":today==3?"今日已点亮  ·  点击和它打招呼":statusLine());
}
}
