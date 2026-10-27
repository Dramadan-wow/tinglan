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
    // Open the wardrobe for the fire buddy shared with this friend.
    void showWardrobe(int friendId);
private:
    QWidget* appearance();
    QWidget* membership();
    QWidget* orders();
    void chooseImage(const QString& kind);
    void refreshPlan();
    AppSessionHsx& session_;
    QTabWidget* tabs_;
    QLabel *status_,*avatar_,*background_,*price_,*benefits_,*orderHint_;
    QComboBox *plan_,*frame_,*buddyFriend_;
    QLabel* buddyNote_;
    int buddyTarget_=0;
    QLineEdit* reference_;
    QTableWidget* orders_;
    FireBuddyHsx* petPreview_;
    QMap<QString,QPushButton*> bubbles_,themes_,skins_,outfits_;
    QMap<QString,FireBuddyHsx*> skinSamples_,outfitSamples_;
};
}
