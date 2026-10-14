#include "ChatBackdropHsx.h"
#include <QPainter>
#include <QPainterPath>
namespace im {
ChatBackdropHsx::ChatBackdropHsx(QWidget* parent):QWidget(parent){}
void ChatBackdropHsx::setBackground(const QString& path){if(path==path_)return;path_=path;image_=QPixmap(path);cacheSize_={};update();}
void ChatBackdropHsx::paintEvent(QPaintEvent*){QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::SmoothPixmapTransform);QPainterPath clip;clip.addRoundedRect(QRectF(rect()),12,12);p.setClipPath(clip);p.fillRect(rect(),QColor("#f5f7fb"));if(!image_.isNull()){if(cacheSize_!=size()){scaled_=image_.scaled(size(),Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);cacheSize_=size();}p.drawPixmap(rect(),scaled_,QRect((scaled_.width()-width())/2,(scaled_.height()-height())/2,width(),height()));p.fillRect(rect(),QColor(255,255,255,115));}}
}
