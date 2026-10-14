#pragma once
#include "AppSessionHsx.h"
#include <QWidget>
class QTableWidget; class QLineEdit; class QComboBox; class QLabel;
namespace im {
class FriendsPageHsx final : public QWidget {
    Q_OBJECT
public:
    FriendsPageHsx(AppSessionHsx&,QWidget* parent=nullptr);
    void refresh();
signals:
    void openChat(int user);
private:
    void addFriend();
    void editFriend();
    void block(bool on);
    void compare(bool common);
    AppSessionHsx& session_;
    QTableWidget* table_;
    QLineEdit* search_;
    QComboBox* filter_;
    QLabel* summary_;
};
}
