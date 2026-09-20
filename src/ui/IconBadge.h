#pragma once
// The rounded-square "brand" tile shown next to every item (list + detail).
// Logins get a monogram on a brand-coloured background; other item types get
// their type glyph. Rendering is shared between the widget and the list
// delegate through paintBadge().

#include <QColor>
#include <QPainter>
#include <QWidget>

#include "core/Item.h"

namespace mp {

QColor badgeColorFor(const Item& item);
void paintBadge(QPainter& p, const QRectF& rect, const Item& item, qreal radius = -1);

class IconBadge : public QWidget {
    Q_OBJECT
public:
    explicit IconBadge(int size = 48, QWidget* parent = nullptr);
    void setItem(const Item& item);
    void setItemType(ItemType type);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    Item m_item;
    int m_size;
};

}  // namespace mp
