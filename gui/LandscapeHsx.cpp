#include "LandscapeHsx.h"
#include <QPainter>
#include <QPainterPath>
namespace im {
LandscapeHsx::LandscapeHsx(QWidget* parent):QWidget(parent),artwork_(":/art/mist-river.png"){setObjectName("loginLandscape");setAttribute(Qt::WA_StyledBackground,false);}
void LandscapeHsx::paintEvent(QPaintEvent*) {
    QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);painter.setRenderHint(QPainter::SmoothPixmapTransform);
    QPainterPath clip;clip.addRoundedRect(QRectF(rect()),24,24);painter.setClipPath(clip);
    painter.fillRect(rect(),QColor("#e5ebe3"));
    if(!artwork_.isNull()) {
        const auto scaled=artwork_.scaled(size()*devicePixelRatioF(),Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);
        const QSizeF logical=QSizeF(scaled.size())/devicePixelRatioF();
        painter.drawPixmap(QRectF((width()-logical.width())/2,(height()-logical.height())/2,logical.width(),logical.height()),scaled,QRectF(scaled.rect()));
    }
    QLinearGradient veil(0,0,0,height());veil.setColorAt(0,QColor(252,249,239,110));veil.setColorAt(.45,QColor(252,249,239,0));veil.setColorAt(1,QColor(15,47,40,65));painter.fillRect(rect(),veil);
}
}
