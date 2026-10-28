#include "WeiboThemeHsx.h"
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
namespace im {
WeiboHeroHsx::WeiboHeroHsx(QWidget* parent):QWidget(parent){setObjectName("yvonneHero");setFixedHeight(158);setAccessibleName("伊冯灵感主题 · 绚色共振");}
void WeiboHeroHsx::paintEvent(QPaintEvent*){
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);QPainterPath clip;clip.addRoundedRect(rect(),18,18);p.setClipPath(clip);p.fillRect(rect(),QColor("#221d2e"));
    static const QPixmap art(":/art/yvonne-social.png");
    if(!art.isNull()){const int w=430;const int h=w*art.height()/art.width();p.drawPixmap(QRect(width()-w,-14,w,h),art);}
    QLinearGradient fade(0,0,width(),0);fade.setColorAt(0,QColor("#221d2e"));fade.setColorAt(.43,QColor("#221d2e"));fade.setColorAt(.76,QColor(34,29,46,0));p.fillRect(rect(),fade);
    p.setPen(QColor("#e5f483"));QFont f("Segoe UI",9);f.setLetterSpacing(QFont::AbsoluteSpacing,2);p.setFont(f);p.drawText(24,30,"YVONNE  /  SOCIAL CHANNEL");
    f=QFont("Microsoft YaHei UI",24,QFont::Bold);p.setFont(f);p.setPen(QColor("#fff0f7"));p.drawText(22,79,QString::fromUtf8("绚色共振"));
    p.setFont(QFont("Microsoft YaHei UI",10));p.setPen(QColor("#d9bed0"));p.drawText(24,108,QString::fromUtf8("把日常的灵感，发送到同一频率。"));
    p.setPen(Qt::NoPen);p.setBrush(QColor("#ed78af"));p.drawRoundedRect(QRect(24,125,74,18),9,9);p.setPen(QColor("#271e30"));p.setFont(QFont("Microsoft YaHei UI",8,QFont::Bold));p.drawText(QRect(24,125,74,18),Qt::AlignCenter,QString::fromUtf8("伊冯灵感"));
}
QString weiboChrome(){return R"(
QMainWindow {background:#f6f1f8;}
QWidget#sidebar {background:#251f30;border-right:1px solid #403246;}
QWidget#sidebar QLabel#brand {color:#ffd5e9;font-size:21px;letter-spacing:0px;}
QPushButton#nav {color:#cebfd8;background:transparent;border:0;text-align:left;padding:12px;border-radius:12px;}
QPushButton#nav:hover {background:#3b3046;color:#ffebf5;}
QPushButton#nav:checked {background:#f5bad8;color:#382839;font-weight:600;}
QPushButton#weiboInboxNav,QPushButton#weiboFollowingNav {background:transparent;color:#e4cfe1;text-align:left;border:0;padding:13px;border-radius:12px;}
QPushButton#weiboInboxNav:hover,QPushButton#weiboFollowingNav:hover {background:#3b3046;}
QFrame#header {background:#faf5fb;border-bottom:1px solid #ebddeb;}
QLabel#pageTitle {color:#4b2e51;}
QComboBox#activeService {background:#3d3049;color:#faddf0;border:1px solid #65506f;}
QFrame#sidebarProfile,QFrame#sidebarService {background:rgba(255,255,255,14);border:1px solid #3d3049;}
QFrame#sidebarService QLabel {color:#b99bb8;}
QFrame#windowBar {background:#faf5fb;border:none;}
QLabel#windowNotice {color:#957e98;}
QLabel#connectionState {color:#8d4f75;background:#f6e3ee;}
)";}
QString weiboContentStyle(){return R"(
QWidget#weiboPage {background:#f6f1f8;}
QFrame#card {background:#fffafd;border:1px solid #ecdeeb;border-radius:16px;}
QLabel {color:#4b3653;}
QLabel#sectionTitle {color:#573757;}
QLabel#muted {color:#9b839c;}
QPlainTextEdit,QLineEdit,QComboBox {background:#f6eff8;color:#4b3653;border:1px solid #e6d7e9;border-radius:11px;padding:10px;selection-background-color:#f4bbd7;}
QPushButton {background:#fffafd;color:#91577e;border:1px solid #ead9e8;border-radius:10px;padding:8px 13px;}
QPushButton:hover {background:#ffedf6;border-color:#edacd0;}
QPushButton#publishPost,QPushButton#sendWeiboMessage {background:#e987b6;color:#271d2e;border:0;font-weight:600;}
QTabWidget::pane {border:0;background:transparent;}
QTabBar::tab {background:transparent;color:#a2829e;padding:10px 24px;border-bottom:2px solid transparent;}
QTabBar::tab:selected {color:#9d4981;border-bottom:2px solid #e987b6;font-weight:600;}
QListWidget {background:#fffafd;border:1px solid #ecdeeb;border-radius:14px;padding:8px;color:#543959;}
QListWidget::item {padding:14px;border-radius:8px;}
QListWidget::item:selected {background:#f6d5e8;color:#67385c;}
QScrollArea,QScrollArea>QWidget>QWidget {background:transparent;}
)";}
}
