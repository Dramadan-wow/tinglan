#include "ThemeHsx.h"
#include <QApplication>
#include <QFont>
#include <QLabel>
#include <QMap>
#include <QColor>
namespace im {
QString ThemeHsx::color(int seed) {
    const QString colors[]={"#578fcb","#8c9fda","#c5a17d","#79afaa","#b691c4","#669bac"};
    return colors[static_cast<unsigned>(seed)%6];
}
QLabel* ThemeHsx::avatar(const QString& name,int seed,int size,QWidget* parent) {
    auto* label=new QLabel(name.isEmpty()?"澜":name.left(1),parent);
    label->setFixedSize(size,size);label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(QString("background:%1;color:#ffffff;border:1px solid rgba(255,255,255,65);border-radius:%2px;font-size:%3px;font-weight:600;").arg(color(seed)).arg(size/3).arg(size/2-2));
    return label;
}
void ThemeHsx::install(QApplication& app,const QString& theme) {
    app.setStyle("Fusion");app.setFont(QFont("Microsoft YaHei UI",10));
    QString sheet=QString::fromUtf8(R"(
        QWidget {color:#243e35;font-family:'Microsoft YaHei UI';font-size:13px;}
        QMainWindow,QDialog {background:#f4f6f1;}
        QLabel {background:transparent;}
        QWidget#sidebar {background:#173e34;border:none;}
        QWidget#sidebar QLabel {color:#91aaa0;}
        QWidget#sidebar QLabel#brand {color:#f6f5e9;font-size:29px;font-family:'SimSun';font-weight:500;letter-spacing:3px;}
        QLabel#pageTitle {font-size:25px;font-weight:600;color:#243e35;}
        QLabel#muted {color:#87968c;font-size:11px;}
        QLabel#sectionTitle {font-size:17px;font-weight:600;color:#2e493d;}
        QLabel#emptyState {color:#90a096;font-size:14px;}
        QLabel#kicker {color:#839387;font-size:10px;letter-spacing:2px;}
        QFrame#card,QWidget#card {background:#ffffff;border:1px solid #e3e9df;border-radius:16px;}
        QFrame#header {background:#fbfcf8;border:none;border-bottom:1px solid #e5eade;}
        QFrame#sidebarProfile {background:#234b3f;border:1px solid #36584b;border-radius:14px;}
        QPushButton {background:#ffffff;border:1px solid #dfe6db;border-radius:9px;padding:9px 15px;color:#526a5c;font-size:12px;}
        QPushButton:hover {background:#f2f6ef;border-color:#b7cbb8;color:#315847;}
        QPushButton:pressed {background:#e6eee2;}
        QPushButton:focus {border-color:#8bac91;}
        QPushButton:disabled {color:#adb8ae;background:#f7f9f4;border-color:#ebeee7;}
        QPushButton[primary="true"] {color:#ffffff;background:#356552;border-color:#356552;font-weight:600;}
        QPushButton[primary="true"]:hover {background:#284f40;border-color:#284f40;}
        QPushButton[primary="true"]:pressed {background:#1c4234;}
        QPushButton[primary="true"]:disabled {background:#c2d0c1;border-color:#c2d0c1;}
        QPushButton[danger="true"] {color:#b5665c;}
        QPushButton#nav {color:#b8c9bb;text-align:left;background:transparent;border:1px solid transparent;border-radius:11px;padding:13px 16px;font-size:13px;}
        QPushButton#nav:hover {background:#234c3e;color:#edf3e8;}
        QPushButton#nav:checked {background:#39614f;border-color:#456d5a;color:#fffdf4;font-weight:600;}
        QPushButton#quietButton {background:transparent;border-color:transparent;padding:7px 10px;color:#7d9285;}
        QPushButton#quietButton:hover {background:#eef3e9;color:#345c47;}
        QLineEdit,QPlainTextEdit,QTextEdit,QComboBox,QSpinBox {background:#fcfdf9;border:1px solid #dfe7db;border-radius:9px;padding:10px 12px;selection-background-color:#d8e8d3;selection-color:#294e3a;}
        QLineEdit:focus,QPlainTextEdit:focus,QComboBox:focus,QSpinBox:focus {background:#ffffff;border:1px solid #7ca489;}
        QLineEdit:disabled,QComboBox:disabled {background:#f4f6f1;color:#a7b2a6;}
        QComboBox::drop-down {border:none;width:28px;}
        QComboBox::down-arrow {image:url(:/icons/chevron-down.xpm);width:12px;height:8px;}
        QSpinBox {padding-right:30px;}
        QSpinBox::up-button {subcontrol-origin:border;subcontrol-position:top right;width:26px;border:none;background:transparent;}
        QSpinBox::down-button {subcontrol-origin:border;subcontrol-position:bottom right;width:26px;border:none;background:transparent;}
        QSpinBox::up-arrow {image:url(:/icons/chevron-up.xpm);width:12px;height:8px;}
        QSpinBox::down-arrow {image:url(:/icons/chevron-down.xpm);width:12px;height:8px;}
        QComboBox QAbstractItemView {background:#ffffff;border:1px solid #dce5d5;selection-background-color:#e7efe1;selection-color:#31553f;padding:5px;outline:none;}
        QTableWidget {background:#ffffff;alternate-background-color:#fafbf8;border:none;gridline-color:#edf0e8;selection-background-color:#edf3e7;selection-color:#284d37;outline:none;}
        QTableWidget::item {padding:9px;border-bottom:1px solid #edf0e8;}
        QTableWidget::item:selected {background:#eaf2e5;}
        QHeaderView::section {background:#f6f8f2;color:#899787;border:none;padding:12px;font-size:11px;}
        QListWidget {background:transparent;border:none;outline:none;}
        QListWidget::item {border:none;padding:7px;margin:3px 5px;border-radius:11px;}
        QListWidget::item:selected {background:#eaf2e5;color:#31583f;}
        QListWidget::item:hover {background:#f3f7ef;}
        QScrollArea {border:none;background:transparent;}
        QScrollBar:vertical {width:6px;background:transparent;margin:4px 0;}
        QScrollBar::handle:vertical {background:#d3ded0;border-radius:3px;min-height:32px;}
        QScrollBar::handle:vertical:hover {background:#a6b9a3;}
        QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical {height:0;}
        QScrollBar:horizontal {height:6px;background:transparent;}
        QScrollBar::handle:horizontal {background:#d3ded0;border-radius:3px;min-width:32px;}
        QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal {width:0;}
        QTabWidget::pane {border:none;background:#ffffff;}
        QTabBar::tab {color:#8b998a;padding:13px 19px;background:transparent;border-bottom:2px solid transparent;}
        QTabBar::tab:selected {color:#3f7253;border-bottom:2px solid #507e5c;font-weight:600;}
        QTabBar::tab:hover {color:#456d4b;background:#f7faf4;}
        QStatusBar {background:#fbfcf8;color:#8c9b87;border-top:1px solid #e6ebdf;font-size:10px;}
        QStatusBar::item {border:none;}
        QToolTip {background:#294c3b;color:#fffef2;padding:7px;border:1px solid #365944;border-radius:5px;}
        QMenu {background:white;border:1px solid #e1e7d9;padding:6px;border-radius:9px;}
        QMenu::item {padding:10px 25px;border-radius:6px;}
        QMenu::item:selected {background:#edf4e6;}
        QCheckBox {spacing:9px;color:#768a76;font-size:11px;}
        QCheckBox::indicator {width:15px;height:15px;border:1px solid #c5d2bd;border-radius:4px;background:#ffffff;}
        QCheckBox::indicator:checked {background:#527a58;border:1px solid #527a58;image:url(:/icons/check.xpm);}
        QProgressBar {border:none;background:#edf2e7;border-radius:5px;text-align:center;min-height:10px;}
        QProgressBar::chunk {background:#729976;border-radius:5px;}
    )");
    const QMap<QString,QStringList> palettes{{"ocean",{"#3977b7","#eef5fd","#173b5f"}},{"lilac",{"#8164b3","#f5f0fb","#36264f"}},{"rose",{"#b66986","#fff0f5","#592e43"}},{"amber",{"#b77b3c","#fff7e9","#533c25"}},{"night",{"#5768ae","#f0f2fc","#252e53"}}};
    if(palettes.contains(theme)){const auto colors=palettes.value(theme);const QColor accent(colors[0]);sheet.replace("#356552",colors[0]).replace("#284f40",accent.darker(120).name()).replace("#1c4234",accent.darker(140).name()).replace("#173e34",colors[2]).replace("#39614f",accent.darker(125).name()).replace("#456d5a",accent.name()).replace("#234c3e",accent.darker(155).name()).replace("#234b3f",accent.darker(155).name()).replace("#36584b",accent.darker(125).name()).replace("#f4f6f1",colors[1]).replace("#eaf2e5",colors[1]).replace("#edf3e7",colors[1]).replace("#31583f",colors[0]).replace("#3f7253",colors[0]).replace("#507e5c",colors[0]);}
    app.setStyleSheet(sheet);
}

QString ThemeHsx::accent(const QString& theme) {
    const QMap<QString,QString> accents{{"jade","#1689ff"},{"ocean","#1689ff"},{"lilac","#8164b3"},{"rose","#b66986"},{"amber","#b77b3c"},{"night","#5768ae"}};
    return accents.value(theme,"#1689ff");
}
QString ThemeHsx::workspace(const QString& theme) {
    QString sheet=QString::fromUtf8(R"(
        QWidget {color:#233650;}
        QMainWindow {background:#f5f9ff;}
        QWidget#sidebar {background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #f6fbff,stop:1 #e4f0ff);border-right:1px solid #e0ebf8;}
        QWidget#sidebar QLabel#brand {color:#163e70;font-family:'Microsoft YaHei UI';font-size:27px;font-weight:700;letter-spacing:2px;}
        QPushButton#nav {background:transparent;color:#526d91;padding:11px 12px;font-size:14px;border:none;border-radius:10px;}
        QPushButton#nav:hover {background:#e6f1ff;color:#1689ff;}
        QPushButton#nav:checked {background:#d5eaff;color:#1689ff;border:none;}
        QFrame#header {background:#f8fbff;border-bottom:1px solid #e5edf7;}
        QLabel#pageTitle,QLabel#sectionTitle {color:#233650;}
        QLabel#muted,QLabel#kicker {color:#8295b0;}
        QPushButton {background:#ffffff;color:#526d91;border:1px solid #e2ebf6;}
        QPushButton:hover {background:#edf5ff;border-color:#b9d9fc;color:#1689ff;}
        QPushButton[primary="true"] {background:#1689ff;border-color:#1689ff;color:white;}
        QPushButton[primary="true"]:hover {background:#0875e4;border-color:#0875e4;}
        QPushButton[primary="true"]:disabled {background:#bdcfe3;border-color:#bdcfe3;color:#f5f9ff;}
        QPushButton#quietButton {color:#7890b1;}
        QLineEdit,QComboBox,QPlainTextEdit,QTextEdit,QSpinBox {background:#f0f5fc;border:1px solid #e3ebf5;selection-background-color:#cbe6ff;selection-color:#203955;}
        QLineEdit:focus,QComboBox:focus,QPlainTextEdit:focus,QTextEdit:focus {background:#ffffff;border-color:#9acbff;}
        QFrame#conversationPane {background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #f6faff,stop:1 #eff6ff);border-right:1px solid #e2ecf8;}
        QFrame#chatPane {background:#f2f7fe;border:none;}
        QFrame#chatHeader {background:#f8fbff;border-bottom:1px solid #e2ecf8;}
        QLabel#chatTitle {font-size:18px;font-weight:700;color:#20324e;}
        QLabel#chatSubtitle {font-size:11px;color:#8093ad;}
        QFrame#contactPanel {background:#f8fbff;border-left:1px solid #e3edf8;}
        QLabel#contactName {font-size:22px;font-weight:700;color:#20324e;}
        QLabel#contactDetails {font-size:12px;color:#778daa;line-height:150%;}
        QLabel#contactQuote {color:#8299b8;background:#eef4fc;border:1px solid #e5edf8;border-radius:10px;padding:12px;font-size:12px;}
        QLabel#contactSection {font-size:13px;font-weight:600;color:#2b4263;}
        QLabel#contactStats {background:#edf5ff;color:#557da7;border-radius:12px;padding:14px;font-size:12px;}
        QPushButton#contactAction {text-align:left;padding:13px 12px;border:1px solid #e7eef8;background:#ffffff;border-radius:10px;}
        QPushButton#contactAction:hover {background:#e9f4ff;color:#1689ff;}
        QFrame#composerFrame {background:#ffffff;border:1px solid #e5edf8;border-radius:18px;}
        QPushButton#chatIcon {padding:0;border:none;background:transparent;border-radius:9px;}
        QPushButton#chatIcon:hover {background:#e0efff;}
        QPushButton#sendMessage {padding:0;background:#1689ff;border:none;border-radius:22px;}
        QPushButton#sendMessage:hover {background:#0875e4;}
        QPushButton#sendMessage:disabled {background:#bdd6f0;}
        QCheckBox {color:#7289a8;}
        QCheckBox::indicator:checked {background:#1689ff;border-color:#1689ff;}
        QScrollBar::handle:vertical,QScrollBar::handle:horizontal {background:#ccdced;}
        QStatusBar {background:#f4f9ff;color:#8a9fbb;border-top:1px solid #e3edf8;}
        QMenu::item:selected {background:#e8f3ff;color:#1689ff;}
        QTableWidget {selection-background-color:#e8f3ff;selection-color:#294b72;alternate-background-color:#f8fbff;gridline-color:#edf2f9;}
        QTableWidget::item {border-bottom:1px solid #eef3fa;}
        QTableWidget::item:selected {background:#e8f3ff;}
        QHeaderView::section {background:#f1f6fd;color:#7890ae;font-weight:600;}
        QFrame#card,QWidget#card {background:#ffffff;border:1px solid #e3ecf8;border-radius:18px;}
        QFrame#header {background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #f9fbff,stop:1 #eef5ff);}
        QLabel#pageTitle {letter-spacing:1px;}
        QFrame#sidebarProfile {background:rgba(255,255,255,165);border:1px solid #e1ecf9;border-radius:16px;}
        QFrame#sidebarService {background:rgba(255,255,255,140);border:1px solid #e1ecf9;border-radius:14px;}
        QFrame#sidebarService QLabel {color:#8a9dba;font-size:10px;letter-spacing:1px;}
        QLabel#connectionState {color:#4f6f96;background:#e7f1fe;border-radius:9px;padding:3px 10px;font-size:10px;}
        QFrame#windowBar {background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #f9fbff,stop:1 #eef5ff);border:none;}
        QLabel#windowNotice {color:#8a9fbb;font-size:11px;}
        QFrame#windowBar QPushButton#softwareUpdates {padding:3px 12px;font-size:11px;min-height:0;}
    )");
    sheet.replace("#1689ff",accent(theme));
    return sheet;
}
}

