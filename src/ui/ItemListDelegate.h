#pragma once
#include <QStyledItemDelegate>

namespace mp {

// Roles exposed by the list model.
enum ItemRoles {
    RoleItemId = Qt::UserRole + 1,
    RoleTitle,
    RoleSubtitle,
    RoleItemPtr,   // const Item* (for badge painting)
    RoleFavorite,
};

class ItemListDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

}  // namespace mp
