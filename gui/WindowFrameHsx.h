#pragma once
#include <QWidget>
#include <functional>
class QPushButton;
namespace im {
// Replaces the white system title bar with the window's own top area. On Windows the native frame stays
// (resizing, Aero snap, shadow and the minimise/maximise animations); only its title bar is not drawn.
// Other platforms keep the native frame and the buttons are hidden.
class WindowFrameHsx final {
public:
    // caption(localPoint) tells which empty, non-interactive spots drag the window.
    static void install(QWidget* window,std::function<bool(const QPoint&)> caption,bool resizable=true);
    // Minimise, maximise/restore (resizable windows only) and close, for the window's own title row.
    static QWidget* buttons(QWidget* window,bool maximize=true);
    // True when the widget under a window-local point is not a button, input or other control.
    static bool passive(QWidget* window,const QPoint& point);
    static bool active() { return native(); }
private:
    static bool native();
};
}
