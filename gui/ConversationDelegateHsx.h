#pragma once
#include <QStyledItemDelegate>
namespace im {
class ConversationDelegateHsx final : public QStyledItemDelegate {
public:
    explicit ConversationDelegateHsx(QObject* parent=nullptr);
    void paint(QPainter*,const QStyleOptionViewItem&,const QModelIndex&) const override;
    QSize sizeHint(const QStyleOptionViewItem&,const QModelIndex&) const override;
};
}
