#include "AvatarHsx.h"
#include "StudioStoreHsx.h"
#include "ThemeHsx.h"
#include <QPainter>
#include <QPainterPath>
namespace im {
AvatarHsx::AvatarHsx(const StudioStoreHsx& store,int user,const QString& name,int size,QWidget* parent):QLabel(parent),image_(store.media(user,"avatar")),name_(name),color_(ThemeHsx::color(user)),frame_(StudioStoreHsx::item("frame",store.choice(user,"frame"))) {setFixedSize(size,size);setAccessibleName(name+"的头像");}
void AvatarHsx::paintEvent(QPaintEvent*) {
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::SmoothPixmapTransform);QRectF box=rect().adjusted(3,3,-3,-3);QPainterPath path;path.addEllipse(box);p.setClipPath(path);p.fillPath(path,QColor(color_));
    if(!image_.isNull()){const auto scaled=image_.scaled(box.size().toSize(),Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);p.drawPixmap(box,scaled,QRectF((scaled.width()-box.width())/2,(scaled.height()-box.height())/2,box.width(),box.height()));}
    else{p.setPen(Qt::white);QFont font("Microsoft YaHei UI");font.setPixelSize(width()/2-3);font.setWeight(QFont::DemiBold);p.setFont(font);p.drawText(box,Qt::AlignCenter,name_.left(1));}
    p.setClipping(false);if(frame_.value("id")!="none"){QLinearGradient ring(rect().topLeft(),rect().bottomRight());ring.setColorAt(0,QColor(frame_.value("a").toString()));ring.setColorAt(1,QColor(frame_.value("b").toString()));p.setPen(QPen(QBrush(ring),2.5));p.setBrush(Qt::NoBrush);p.drawEllipse(QRectF(rect().adjusted(1,1,-1,-1)));}
}
}
