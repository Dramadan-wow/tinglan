#include "BubbleHsx.h"
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <cmath>
namespace im {
BubbleHsx::BubbleHsx(const QString& text,const QJsonObject& style,bool mine,QWidget* parent):QLabel(text,parent),style_(style),mine_(mine),timer_(new QTimer(this)) {
    if(style_.value("id")=="jade"){style_["a"]="#b9dfff";style_["b"]="#cee8ff";style_["ink"]="#233650";}
    setTextFormat(Qt::PlainText);setWordWrap(true);setTextInteractionFlags(Qt::TextSelectableByMouse);setContentsMargins(15,12,15,12);
    setStyleSheet("background:transparent;border:none;font-size:13px;color:"+style_.value("ink").toString("#304d35")+";");
    timer_->setInterval(80);connect(timer_,&QTimer::timeout,this,[this]{phase_+=.1;update();});setProperty("bubbleStyle",style.value("id").toString());
}
void BubbleHsx::showEvent(QShowEvent* event){QLabel::showEvent(event);const auto effect=style_.value("effect").toString();if(effect=="aurora"||effect=="prism"||effect=="gold")timer_->start();}
void BubbleHsx::hideEvent(QHideEvent* event){timer_->stop();QLabel::hideEvent(event);}
void BubbleHsx::paintEvent(QPaintEvent* event) {
    {QPainter p(this);p.setRenderHint(QPainter::Antialiasing);const QRectF box=rect().adjusted(1,1,-1,-1);const auto effect=style_.value("effect").toString();
        QLinearGradient gradient(box.topLeft(),box.bottomRight());const QColor a(style_.value("a").toString("#dcebd5")),b(style_.value("b").toString("#dcebd5"));gradient.setColorAt(0,a);gradient.setColorAt(1,b);if(effect=="aurora"||effect=="prism")gradient.setColorAt(.5+.25*std::sin(phase_),QColor("#f4e9ff"));
        p.setPen(QPen(a.darker(108),1));p.setBrush(gradient);QPainterPath path;const double left=mine_?16:5,right=mine_?5:16;
        path.moveTo(box.left()+left,box.top());path.lineTo(box.right()-right,box.top());path.quadTo(box.topRight(),QPointF(box.right(),box.top()+right));path.lineTo(box.right(),box.bottom()-16);path.quadTo(box.bottomRight(),QPointF(box.right()-16,box.bottom()));path.lineTo(box.left()+16,box.bottom());path.quadTo(box.bottomLeft(),QPointF(box.left(),box.bottom()-16));path.lineTo(box.left(),box.top()+left);path.quadTo(box.topLeft(),QPointF(box.left()+left,box.top()));path.closeSubpath();p.drawPath(path);p.setClipPath(path);p.setPen(Qt::NoPen);
        if(effect=="dots"||effect=="flowers"){p.setBrush(QColor(255,255,255,100));for(int i=0;i<5;++i){const auto x=box.right()-12-i*14;const auto y=box.top()+8+(i%2)*7;p.drawEllipse(QPointF(x,y),effect=="flowers"?4:2,3);}}
        if(effect=="stars"||effect=="gold"||effect=="prism"){p.setPen(QPen(QColor(255,255,255,140),1.3));for(int i=0;i<4;++i){const double x=box.right()-12-i*19,y=box.top()+8+(i%2)*7,r=2+.7*std::sin(phase_+i);p.drawLine(QPointF(x-r,y),QPointF(x+r,y));p.drawLine(QPointF(x,y-r),QPointF(x,y+r));}}
        if(effect=="waves"){p.setPen(QPen(QColor(255,255,255,100),1.5));for(int i=0;i<2;++i){QPainterPath wave;wave.moveTo(box.left(),box.bottom()-5-i*5);wave.cubicTo(box.width()*.3,box.bottom()-18-i*5,box.width()*.6,box.bottom()+7-i*5,box.right(),box.bottom()-5-i*5);p.drawPath(wave);}}
    }QLabel::paintEvent(event);
}
}
