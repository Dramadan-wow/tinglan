#include "ConversationDelegateHsx.h"
#include "ThemeHsx.h"
#include "IconHsx.h"
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
namespace im {
ConversationDelegateHsx::ConversationDelegateHsx(QObject* parent):QStyledItemDelegate(parent){}
QSize ConversationDelegateHsx::sizeHint(const QStyleOptionViewItem&,const QModelIndex&) const{return QSize(0,80);}
void ConversationDelegateHsx::paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index) const {
    painter->save();painter->setRenderHint(QPainter::Antialiasing);
    const QRect box=option.rect.adjusted(8,3,-8,-3);const bool selected=option.state&QStyle::State_Selected,hover=option.state&QStyle::State_MouseOver;
    // The selected conversation is a white card with a slim accent bar, like the active tab of a notebook.
    const QColor accent(qApp->property("tinglanAccent").toString().isEmpty()?QString("#1689ff"):qApp->property("tinglanAccent").toString());
    if(selected){painter->setPen(QPen(QColor("#d6e6fa"),1));painter->setBrush(Qt::white);painter->drawRoundedRect(QRectF(box).adjusted(.5,.5,-.5,-.5),14,14);
        painter->setPen(Qt::NoPen);painter->setBrush(accent);painter->drawRoundedRect(QRectF(box.left()+1,box.top()+20,3.5,box.height()-40),1.75,1.75);}
    else if(hover){painter->setPen(Qt::NoPen);painter->setBrush(QColor("#eaf3ff"));painter->drawRoundedRect(box,14,14);}
    const QString name=index.data(Qt::UserRole+2).toString(),preview=index.data(Qt::UserRole+3).toString(),time=index.data(Qt::UserRole+5).toString();
    const int seed=index.data(Qt::UserRole+1).toInt(),unread=index.data(Qt::UserRole+4).toInt();
    const QColor color(ThemeHsx::color(seed));QColor tint=color.lighter(185);tint.setAlpha(105);const QRect avatar(box.left()+12,box.top()+(box.height()-50)/2,50,50);
    painter->setPen(Qt::NoPen);painter->setBrush(tint);painter->drawEllipse(avatar);painter->setPen(color);
    if(index.data(Qt::UserRole).toInt()==1)IconHsx::get("group",color.name()).paint(painter,avatar.adjusted(11,11,-11,-11));
    else if(index.data(Qt::UserRole+6).canConvert<QPixmap>())painter->drawPixmap(avatar,index.data(Qt::UserRole+6).value<QPixmap>());
    else{QFont font("Microsoft YaHei UI");font.setPixelSize(17);font.setWeight(QFont::DemiBold);painter->setFont(font);painter->drawText(avatar,Qt::AlignCenter,name.left(1));}
    const int left=avatar.right()+13,right=box.right()-12;QFont title("Microsoft YaHei UI");title.setPixelSize(14);title.setWeight(selected||unread?QFont::DemiBold:QFont::Medium);painter->setFont(title);painter->setPen(QColor("#1f3250"));
    painter->drawText(QRect(left,box.top()+15,right-left-43,22),Qt::AlignVCenter,painter->fontMetrics().elidedText(name,Qt::ElideRight,right-left-43));
    QFont small("Microsoft YaHei UI");small.setPixelSize(10);painter->setFont(small);painter->setPen(QColor(unread?"#5f86b8":"#94a7c0"));painter->drawText(QRect(right-40,box.top()+16,40,20),Qt::AlignRight|Qt::AlignVCenter,time);
    small.setPixelSize(12);painter->setFont(small);painter->setPen(QColor(unread?"#5d7393":"#8597af"));const int previewWidth=right-left-(unread?28:0);
    painter->drawText(QRect(left,box.top()+39,previewWidth,21),Qt::AlignVCenter,painter->fontMetrics().elidedText(preview,Qt::ElideRight,previewWidth));
    if(unread){const QString count=unread>99?"99+":QString::number(unread);const int width=unread>9?25:18;const QRect badge(right-width,box.top()+41,width,18);painter->setPen(Qt::NoPen);painter->setBrush(QColor("#ff515e"));painter->drawRoundedRect(badge,9,9);painter->setPen(Qt::white);small.setPixelSize(9);small.setWeight(QFont::DemiBold);painter->setFont(small);painter->drawText(badge,Qt::AlignCenter,count);}
    painter->restore();
}
}
