#pragma once
#include "AppSessionHsx.h"
#include <QWidget>
#include <QMap>
class QTabWidget;class QLabel;class QComboBox;class QTableWidget;class QLineEdit;class QPushButton;class QVBoxLayout;
namespace im {
class FireBuddyHsx;
class StudioPageHsx final : public QWidget {
    Q_OBJECT
public:
    explicit StudioPageHsx(AppSessionHsx&,QWidget* parent=nullptr);
    void refresh();
    void showMembership();
private:
    QWidget* appearance();
    QWidget* membership();
    QWidget* orders();
    void chooseImage(const QString& kind);
    void refreshPlan();
    AppSessionHsx& session_;
    QTabWidget* tabs_;
    QLabel *status_,*avatar_,*background_,*price_,*benefits_,*orderHint_;
    QComboBox *plan_,*frame_,*pet_;
    QLineEdit* reference_;
    QTableWidget* orders_;
    FireBuddyHsx* petPreview_;
    QMap<QString,QPushButton*> bubbles_,themes_;
};
}
