#include "ChatBackdropHsx.h"
#include <QPainter>
#include <QPainterPath>
namespace im {
ChatBackdropHsx::ChatBackdropHsx(QWidget* parent):QWidget(parent){}
void ChatBackdropHsx::setBackground(const QString& path){if(path==path_)return;path_=path;image_=QPixmap(path);cacheSize_={};update();}
void ChatBackdropHsx::paintEvent(QPaintEvent*){QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::SmoothPixmapTransform);QPainterPath clip;clip.addRoundedRect(QRectF(rect()),12,12);p.setClipPath(clip);QLinearGradient wash(0,0,0,height());wash.setColorAt(0,QColor("#f7faff"));wash.setColorAt(1,QColor("#edf4fd"));p.fillRect(rect(),wash);if(!image_.isNull()){if(cacheSize_!=size()){scaled_=image_.scaled(size(),Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);cacheSize_=size();}p.drawPixmap(rect(),scaled_,QRect((scaled_.width()-width())/2,(scaled_.height()-height())/2,width(),height()));p.fillRect(rect(),QColor(255,255,255,115));}}
}
