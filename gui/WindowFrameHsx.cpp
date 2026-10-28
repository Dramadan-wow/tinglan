#include "WindowFrameHsx.h"
#include "IconHsx.h"
#include <QAbstractButton>
#include <QAbstractNativeEventFilter>
#include <QAbstractScrollArea>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QCursor>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHash>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QTextEdit>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#endif
namespace im {
namespace {
#ifdef Q_OS_WIN
struct Framed { QPointer<QWidget> window; std::function<bool(const QPoint&)> caption; bool resizable=true; };
class Filter final : public QAbstractNativeEventFilter {
public:
    QHash<HWND,Framed> windows;
    bool nativeEventFilter(const QByteArray& type,void* message,qintptr* result) override {
        if(type!="windows_generic_MSG")return false;auto* msg=static_cast<MSG*>(message);const auto it=windows.find(msg->hwnd);if(it==windows.end())return false;
        if(!it->window){windows.erase(it);return false;}
        const HWND hwnd=msg->hwnd;const UINT dpi=GetDpiForWindow(hwnd);
        if(msg->message==WM_NCCALCSIZE&&msg->wParam){
            // The whole window is client area. A maximised window reaches past the screen by its frame, so inset it.
            if(IsZoomed(hwnd)){auto* p=reinterpret_cast<NCCALCSIZE_PARAMS*>(msg->lParam);const int pad=GetSystemMetricsForDpi(SM_CXPADDEDBORDER,dpi),fx=GetSystemMetricsForDpi(SM_CXSIZEFRAME,dpi)+pad,fy=GetSystemMetricsForDpi(SM_CYSIZEFRAME,dpi)+pad;
                p->rgrc[0].left+=fx;p->rgrc[0].top+=fy;p->rgrc[0].right-=fx;p->rgrc[0].bottom-=fy;}
            *result=0;return true;
        }
        if(msg->message==WM_NCHITTEST){
            RECT r;GetWindowRect(hwnd,&r);const LONG x=GET_X_LPARAM(msg->lParam),y=GET_Y_LPARAM(msg->lParam);
            if(it->resizable&&!IsZoomed(hwnd)){
                const int edge=MulDiv(6,int(dpi),96);const bool left=x<r.left+edge,right=x>=r.right-edge,top=y<r.top+edge,bottom=y>=r.bottom-edge;
                const LRESULT hit=top&&left?HTTOPLEFT:top&&right?HTTOPRIGHT:bottom&&left?HTBOTTOMLEFT:bottom&&right?HTBOTTOMRIGHT:left?HTLEFT:right?HTRIGHT:top?HTTOP:bottom?HTBOTTOM:HTNOWHERE;
                if(hit!=HTNOWHERE){*result=hit;return true;}
            }
            *result=it->caption&&it->caption(it->window->mapFromGlobal(QCursor::pos()))?HTCAPTION:HTCLIENT;return true;
        }
        return false;
    }
};
Filter* filter(){static Filter* f=[]{auto* created=new Filter;QCoreApplication::instance()->installNativeEventFilter(created);return created;}();return f;}
#endif
// Swaps the maximise button between maximise and restore as the window state changes.
class StateWatcher final : public QObject {
public:
    explicit StateWatcher(QPushButton* button):QObject(button),button_(button){}
    bool eventFilter(QObject* watched,QEvent* event) override {
        if(event->type()==QEvent::WindowStateChange){const bool max=static_cast<QWidget*>(watched)->isMaximized();button_->setIcon(IconHsx::get(max?"win-restore":"win-max","#5b6f8c"));button_->setToolTip(max?"还原":"最大化");button_->setAccessibleName(button_->toolTip());}
        return false;
    }
private:
    QPushButton* button_;
};
}
bool WindowFrameHsx::native(){
#ifdef Q_OS_WIN
    return QGuiApplication::platformName()=="windows";
#else
    return false;
#endif
}
void WindowFrameHsx::install(QWidget* window,std::function<bool(const QPoint&)> caption,bool resizable){
#ifdef Q_OS_WIN
    if(!native())return;
    window->setWindowFlag(Qt::FramelessWindowHint,true);
    const auto hwnd=reinterpret_cast<HWND>(window->winId());
    // Qt drops these styles for frameless windows; they bring back snapping, the shadow and the animations.
    SetWindowLongPtrW(hwnd,GWL_STYLE,GetWindowLongPtrW(hwnd,GWL_STYLE)|WS_THICKFRAME|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|(resizable?WS_MAXIMIZEBOX:0));
    const MARGINS shadow{0,0,1,0};DwmExtendFrameIntoClientArea(hwnd,&shadow);
    filter()->windows[hwnd]={window,std::move(caption),resizable};
    SetWindowPos(hwnd,nullptr,0,0,0,0,SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
#else
    Q_UNUSED(window);Q_UNUSED(caption);Q_UNUSED(resizable);
#endif
}
bool WindowFrameHsx::passive(QWidget* window,const QPoint& point){
    for(QWidget* w=window->childAt(point);w&&w!=window;w=w->parentWidget())
        if(qobject_cast<QAbstractButton*>(w)||qobject_cast<QComboBox*>(w)||qobject_cast<QLineEdit*>(w)||qobject_cast<QAbstractSpinBox*>(w)||qobject_cast<QAbstractScrollArea*>(w)||qobject_cast<QTextEdit*>(w))return false;
    return true;
}
QWidget* WindowFrameHsx::buttons(QWidget* window,bool maximize){
    auto* box=new QWidget;box->setObjectName("windowButtons");auto* row=new QHBoxLayout(box);row->setContentsMargins(0,0,0,0);row->setSpacing(2);
    auto make=[&](const QString& name,const QString& icon,const QString& tip){
        auto* b=new QPushButton;b->setObjectName(name);b->setIcon(IconHsx::get(icon,"#5b6f8c"));b->setIconSize(QSize(16,16));b->setFixedSize(42,30);b->setToolTip(tip);b->setAccessibleName(tip);b->setFocusPolicy(Qt::NoFocus);b->setCursor(Qt::ArrowCursor);row->addWidget(b);return b;};
    QObject::connect(make("windowMinimize","win-min","最小化"),&QPushButton::clicked,window,[window]{window->showMinimized();});
    if(maximize){auto* toggle=make("windowMaximize","win-max","最大化");window->installEventFilter(new StateWatcher(toggle));
        QObject::connect(toggle,&QPushButton::clicked,window,[window]{if(window->isMaximized())window->showNormal();else window->showMaximized();});}
    auto* close=make("windowClose","win-close","关闭");QObject::connect(close,&QPushButton::clicked,window,[window]{window->close();});
    close->setIcon([]{QIcon icon=IconHsx::get("win-close","#5b6f8c");icon.addPixmap(IconHsx::get("win-close","#ffffff").pixmap(32,32),QIcon::Active);return icon;}());
    box->setStyleSheet("QPushButton{background:transparent;border:none;border-radius:8px;padding:0;min-height:0;}QPushButton:hover{background:rgba(22,137,255,28);}QPushButton#windowClose:hover{background:#e5484d;}");
    box->setVisible(native());return box;
}
}
