#include "ConversationDelegateHsx.h"
#include "ThemeHsx.h"
#include "IconHsx.h"
#include <QPainter>
#include <QPainterPath>
namespace im {
ConversationDelegateHsx::ConversationDelegateHsx(QObject* parent):QStyledItemDelegate(parent){}
QSize ConversationDelegateHsx::sizeHint(const QStyleOptionViewItem&,const QModelIndex&) const{return QSize(0,86);}
void ConversationDelegateHsx::paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index) const {
    painter->save();painter->setRenderHint(QPainter::Antialiasing);
    const QRect box=option.rect.adjusted(0,1,0,-1);const bool selected=option.state&QStyle::State_Selected;
    painter->setPen(Qt::NoPen);painter->setBrush(selected?QColor("#dceeff"):(option.state&QStyle::State_MouseOver)?QColor("#eaf4ff"):QColor("#f5f9ff"));painter->drawRoundedRect(box,12,12);
    const QString name=index.data(Qt::UserRole+2).toString(),preview=index.data(Qt::UserRole+3).toString(),time=index.data(Qt::UserRole+5).toString();
    const int seed=index.data(Qt::UserRole+1).toInt(),unread=index.data(Qt::UserRole+4).toInt();
    const QColor color(ThemeHsx::color(seed));QColor tint=color.lighter(185);tint.setAlpha(105);const QRect avatar(box.left()+12,box.top()+16,52,52);
    painter->setBrush(tint);painter->drawEllipse(avatar);painter->setPen(color);
    if(index.data(Qt::UserRole).toInt()==1)IconHsx::get("group",color.name()).paint(painter,avatar.adjusted(10,10,-10,-10));
    else if(index.data(Qt::UserRole+6).canConvert<QPixmap>())painter->drawPixmap(avatar,index.data(Qt::UserRole+6).value<QPixmap>());
    else{QFont font("Microsoft YaHei UI");font.setPixelSize(17);font.setWeight(QFont::DemiBold);painter->setFont(font);painter->drawText(avatar,Qt::AlignCenter,name.left(1));}
    const int left=avatar.right()+13,right=box.right()-14;QFont title("Microsoft YaHei UI");title.setPixelSize(14);title.setWeight(selected?QFont::DemiBold:QFont::Medium);painter->setFont(title);painter->setPen(QColor("#233650"));
    painter->drawText(QRect(left,box.top()+17,right-left-43,23),Qt::AlignVCenter,painter->fontMetrics().elidedText(name,Qt::ElideRight,right-left-43));
    QFont small("Microsoft YaHei UI");small.setPixelSize(10);painter->setFont(small);painter->setPen(QColor("#8197b5"));painter->drawText(QRect(right-40,box.top()+18,40,20),Qt::AlignRight|Qt::AlignVCenter,time);
    small.setPixelSize(12);painter->setFont(small);painter->setPen(QColor("#7b90ad"));const int previewWidth=right-left-(unread?28:0);
    painter->drawText(QRect(left,box.top()+42,previewWidth,21),Qt::AlignVCenter,painter->fontMetrics().elidedText(preview,Qt::ElideRight,previewWidth));
    if(unread){const QString count=unread>99?"99+":QString::number(unread);const int width=unread>9?25:18;const QRect badge(right-width,box.top()+43,width,18);painter->setPen(Qt::NoPen);painter->setBrush(QColor("#ff515e"));painter->drawRoundedRect(badge,9,9);painter->setPen(Qt::white);small.setPixelSize(9);painter->setFont(small);painter->drawText(badge,Qt::AlignCenter,count);}
    painter->restore();
}
}
