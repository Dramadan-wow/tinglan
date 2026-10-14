#include "FireBuddyHsx.h"
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <cmath>
namespace im {
FireBuddyHsx::FireBuddyHsx(QWidget* parent):QWidget(parent),timer_(new QTimer(this)){setMinimumSize(238,61);setMaximumHeight(65);setCursor(Qt::PointingHandCursor);setObjectName("fireBuddy");timer_->setInterval(80);connect(timer_,&QTimer::timeout,this,[this]{phase_+=.15;if(greeting_>0)--greeting_;update();});}
void FireBuddyHsx::setState(const QJsonObject& state,const QJsonObject& skin){state_=state;skin_=skin;setToolTip("双方每天各发送至少一条私聊消息，点亮当天火苗；连续满 7 天提升一级。\n今日只有单方发送时保留昨天的连续天数；漏掉一个完整自然日后重新开始。\n按当前服务统计，已撤回的消息不计入。点击小火人可以打招呼。");setAccessibleName(state.value("preview").toBool()?"小火人外观预览":QString("小火人，连续聊天 %1 天，等级 %2").arg(state.value("days").toInt()).arg(state.value("level").toInt(1)));update();}
void FireBuddyHsx::showEvent(QShowEvent* event){QWidget::showEvent(event);timer_->start();}
void FireBuddyHsx::hideEvent(QHideEvent* event){timer_->stop();QWidget::hideEvent(event);}
void FireBuddyHsx::mousePressEvent(QMouseEvent*){greeting_=30;update();}
void FireBuddyHsx::paintEvent(QPaintEvent*){
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);const bool active=state_.value("active").toBool();p.setPen(Qt::NoPen);p.setBrush(active?QColor("#fff5e8"):QColor("#f2f4f8"));p.drawRoundedRect(rect().adjusted(0,0,-1,-1),12,12);
    p.save();p.translate(9,5+std::sin(phase_)*(greeting_?3:1.2));QPainterPath flame;flame.moveTo(24,1);flame.cubicTo(29,16,43,17,43,32);flame.cubicTo(44,55,3,57,4,32);flame.cubicTo(5,23,9,18,14,16);flame.cubicTo(11,27,20,23,24,1);QLinearGradient fill(0,0,0,52);fill.setColorAt(0,active?QColor(skin_.value("a").toString("#ff963b")):QColor("#aab5c6"));fill.setColorAt(1,active?QColor(skin_.value("b").toString("#ffd56e")):QColor("#d9e0eb"));p.setBrush(fill);p.drawPath(flame);
    p.setBrush(QColor(255,255,255,115));p.drawEllipse(QRectF(10,27,27,22));p.setPen(QPen(QColor("#6b4d43"),2,Qt::SolidLine,Qt::RoundCap));const bool blink=std::fmod(phase_,6)>5.7;if(blink){p.drawLine(QPointF(16,34),QPointF(19,34));p.drawLine(QPointF(29,34),QPointF(32,34));}else{p.drawPoint(QPointF(18,34));p.drawPoint(QPointF(30,34));}p.drawArc(QRectF(21,35,7,6),180*16,180*16);p.restore();
    p.setPen(QColor("#684c3a"));QFont title("Microsoft YaHei UI");title.setPixelSize(12);title.setWeight(QFont::DemiBold);p.setFont(title);p.drawText(QRect(64,9,width()-69,23),Qt::AlignVCenter,greeting_?"在呢，今天也要好好聊天！":state_.value("preview").toBool()?"外观预览 · "+skin_.value("name").toString():active?QString("小火人  Lv.%1 · 连续 %2 天").arg(state_.value("level").toInt(1)).arg(state_.value("days").toInt()):"小火人 · 等一句双向的问候");
    title.setPixelSize(10);title.setWeight(QFont::Normal);p.setFont(title);p.setPen(QColor("#9a897b"));const int today=state_.value("today").toInt();p.drawText(QRect(64,32,width()-69,20),Qt::AlignVCenter,state_.value("preview").toBool()?"真实连续天数请在好友私聊中查看":today==3?"今日已点亮  ·  点击和它打招呼":today==1?"今天还差对方的一条消息":today==2?"今天还差你的一条消息":"双方今天各发一条消息，即可点亮");
}
}
