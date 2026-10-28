#pragma once
#include "AppSessionHsx.h"
#include <QWidget>
#include <array>
class QTimer; class QResizeEvent; class QListWidget; class QLineEdit; class QLabel; class QScrollArea; class QVBoxLayout; class QComboBox; class QCheckBox; class QPushButton;
namespace im {
class FireBuddyHsx;
class ChatPageHsx final : public QWidget {
    Q_OBJECT
public:
    explicit ChatPageHsx(AppSessionHsx& session, QWidget* parent = nullptr);
    void refresh();
    void setActive(bool active);
    void open(MessageKindHsx kind, int target);
    int target() const { return target_; }
signals:
    void openStudio();
    void openWardrobe(int friendId);
    void openSection(int page);
protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
private:
    void showHistory();
    void refreshDetails();
    void showProfile();
    void send();
    AppSessionHsx& session_;
    QListWidget* conversations_;
    QLineEdit *search_, *composer_, *conversationSearch_;
    QLabel *title_, *subtitle_, *headerAvatar_, *contactName_, *contactDetails_, *contactStats_, *contactStatus_;
    std::array<QLabel*,3> contactTiles_{};
    QWidget *contactPanel_, *contactArtwork_, *filters_;
    QPushButton *profileButton_, *detailsToggle_ = nullptr;
    QScrollArea* scroll_;
    QWidget* content_;
    QVBoxLayout* bubbles_;
    QComboBox* mentions_;
    QCheckBox* favorites_;
    QPushButton *send_, *older_, *file_;
    FireBuddyHsx* buddy_;
    QTimer* reflow_ = nullptr;
    MessageKindHsx kind_ = MessageKindHsx::Private;
    int renderedWidth_ = 0;
    int target_ = 0, limit_ = 40, user_ = 0;
    ServiceHsx service_ = ServiceHsx::Count;
    bool active_ = false;
};
}
