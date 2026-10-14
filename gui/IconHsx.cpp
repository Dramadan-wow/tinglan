#include "IconHsx.h"
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
namespace im {
namespace {
QPixmap draw(const QString& name,const QColor& color) {
    QPixmap image(48,48);image.setDevicePixelRatio(2);image.fill(Qt::transparent);
    QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(color,1.65,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.setBrush(Qt::NoBrush);
    QPainterPath path;
    if(name=="chat") {path.moveTo(5,17);path.lineTo(3,21);path.lineTo(9,18);path.lineTo(17,18);path.quadTo(21,18,21,14);path.lineTo(21,7);path.quadTo(21,3,17,3);path.lineTo(7,3);path.quadTo(3,3,3,7);path.lineTo(3,13);path.quadTo(3,16,5,17);p.drawPath(path);p.drawLine(8,9,16,9);p.drawLine(8,13,13,13);}
    else if(name=="user"||name=="users"||name=="group") {p.drawEllipse(QPointF(10,7),3.5,3.5);path.moveTo(3,21);path.lineTo(3,18);path.cubicTo(3,12,17,12,17,18);path.lineTo(17,21);p.drawPath(path);if(name!="user"){path=QPainterPath();path.moveTo(17,4);path.cubicTo(22,4,22,10,18,11);path.moveTo(19,14);path.quadTo(23,15,22,21);p.drawPath(path);}}
    else if(name=="bell") {path.moveTo(5,16);path.lineTo(6,13);path.lineTo(6,9);path.cubicTo(6,1,18,1,18,9);path.lineTo(18,13);path.lineTo(20,17);path.lineTo(4,17);path.closeSubpath();p.drawPath(path);p.drawArc(QRectF(9,17,6,5),180*16,180*16);}
    else if(name=="data") {p.drawRoundedRect(QRectF(3,3,18,18),3,3);p.drawLine(7,16,7,12);p.drawLine(12,16,12,7);p.drawLine(17,16,17,10);}
    else if(name=="network") {p.drawEllipse(QPointF(12,5),3,3);p.drawEllipse(QPointF(5,19),3,3);p.drawEllipse(QPointF(19,19),3,3);p.drawLine(10,8,6,16);p.drawLine(14,8,18,16);p.drawLine(8,19,16,19);}
    else if(name=="search") {p.drawEllipse(QRectF(3,3,12.5,12.5));p.drawLine(QPointF(13,13),QPointF(21,21));}
    else if(name=="exit") {path.moveTo(10,3);path.lineTo(4,3);path.lineTo(4,21);path.lineTo(10,21);path.moveTo(9,12);path.lineTo(22,12);path.moveTo(17,7);path.lineTo(22,12);path.lineTo(17,17);p.drawPath(path);}
    else if(name=="arrow") {p.drawLine(4,12,20,12);p.drawLine(14,6,20,12);p.drawLine(14,18,20,12);}
    else if(name=="eye"||name=="eye-off") {path.moveTo(2,12);path.cubicTo(7,3,17,3,22,12);path.cubicTo(17,21,7,21,2,12);p.drawPath(path);p.drawEllipse(QPointF(12,12),3,3);if(name=="eye-off")p.drawLine(3,3,21,21);}
    else if(name=="play") {p.setBrush(color);path.moveTo(8,5);path.lineTo(19,12);path.lineTo(8,19);path.closeSubpath();p.drawPath(path);}
    else if(name=="pause") {p.setBrush(color);p.drawRoundedRect(QRectF(6,5,3,14),1,1);p.drawRoundedRect(QRectF(15,5,3,14),1,1);}
    else if(name=="music") {p.drawEllipse(QRectF(3,15,6,5));p.drawEllipse(QRectF(15,12,6,5));path.moveTo(9,17);path.lineTo(9,5);path.lineTo(21,2);path.lineTo(21,14);path.moveTo(9,9);path.lineTo(21,6);p.drawPath(path);}
    else if(name=="folder") {path.moveTo(3,7);path.lineTo(3,4);path.lineTo(10,4);path.lineTo(12,7);path.lineTo(21,7);path.lineTo(19,20);path.lineTo(3,20);path.closeSubpath();p.drawPath(path);p.drawLine(3,10,20,10);}
    else if(name=="shield") {path.moveTo(12,2);path.lineTo(21,6);path.lineTo(20,14);path.quadTo(19,19,12,22);path.quadTo(5,19,4,14);path.lineTo(3,6);path.closeSubpath();p.drawPath(path);p.drawLine(8,12,11,15);p.drawLine(11,15,16,9);}
    else if(name=="check") {p.drawLine(5,12,10,17);p.drawLine(10,17,20,6);}
    else if(name=="download") {path.moveTo(12,3);path.lineTo(12,15);path.moveTo(7,10);path.lineTo(12,15);path.lineTo(17,10);path.moveTo(3,16);path.lineTo(3,21);path.lineTo(21,21);path.lineTo(21,16);p.drawPath(path);}
    else if(name=="plus") {p.drawLine(12,4,12,20);p.drawLine(4,12,20,12);}
    else if(name=="smile") {p.drawEllipse(QRectF(2,2,20,20));p.drawPoint(QPointF(8,9));p.drawPoint(QPointF(16,9));p.drawArc(QRectF(7,10,10,8),180*16,180*16);}
    else if(name=="send") {path.moveTo(3,10);path.lineTo(22,2);path.lineTo(15,22);path.lineTo(10,14);path.closeSubpath();p.drawPath(path);p.drawLine(10,14,22,2);}
    else if(name=="palette") {path.moveTo(13,22);path.cubicTo(0,23,0,4,10,2);path.cubicTo(23,-1,26,16,18,15);path.cubicTo(11,13,17,21,13,22);p.drawPath(path);for(const auto point:{QPointF(7,8),QPointF(12,6),QPointF(17,9)})p.drawEllipse(point,1,1);}
    else if(name=="brand") {path.moveTo(3,11);path.lineTo(9,4);path.lineTo(14,11);path.lineTo(18,7);path.lineTo(22,11);p.drawPath(path);for(int y:{15,20}){path=QPainterPath();path.moveTo(2,y);path.cubicTo(8,y-4,16,y+4,22,y);p.drawPath(path);}}
    return image;
}
}
QIcon IconHsx::get(const QString& name,const QString& color,const QString& selected){QIcon icon;icon.addPixmap(draw(name,QColor(color)),QIcon::Normal,QIcon::Off);icon.addPixmap(draw(name,QColor(selected)),QIcon::Normal,QIcon::On);icon.addPixmap(draw(name,QColor("#b5c0ba")),QIcon::Disabled);return icon;}
}
