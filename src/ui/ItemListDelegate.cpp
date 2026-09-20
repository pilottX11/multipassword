#include "ui/ItemListDelegate.h"

#include <QPainter>
#include <QPainterPath>

#include "core/Item.h"
#include "ui/IconBadge.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

namespace mp {

void ItemListDelegate::paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& index) const {
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(opt.rect).adjusted(0, 1, 0, -1);
    const bool selected = opt.state & QStyle::State_Selected;
    const bool hover = opt.state & QStyle::State_MouseOver;

    if (selected || hover) {
        QPainterPath path;
        path.addRoundedRect(r, theme::kRadius, theme::kRadius);
        p->fillPath(path, selected ? theme::kAccent : theme::kHover);
    }

    const auto* item = static_cast<const Item*>(index.data(RoleItemPtr).value<void*>());
    const int badge = 38;
    const QRectF badgeRect(r.left() + 12, r.center().y() - badge / 2.0, badge, badge);
    if (item) paintBadge(*p, badgeRect, *item);

    const QColor titleColor = selected ? QColor(Qt::white) : theme::kText;
    const QColor subColor = selected ? QColor(255, 255, 255, 200) : theme::kTextSecondary;

    QFont tf = opt.font;
    tf.setPixelSize(13);
    tf.setWeight(QFont::DemiBold);
    QFont sf = opt.font;
    sf.setPixelSize(11);

    const qreal textLeft = badgeRect.right() + 12;
    const qreal textWidth = r.right() - textLeft - 12 - (index.data(RoleFavorite).toBool() ? 18 : 0);
    const QString title = index.data(RoleTitle).toString();
    const QString subtitle = index.data(RoleSubtitle).toString();

    p->setFont(tf);
    p->setPen(titleColor);
    const bool hasSub = !subtitle.isEmpty();
    const QRectF titleRect(textLeft, r.top() + (hasSub ? 12 : 0), textWidth, hasSub ? 18 : r.height());
    p->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                QFontMetrics(tf).elidedText(title.isEmpty() ? QStringLiteral("Untitled") : title, Qt::ElideRight, static_cast<int>(textWidth)));
    if (hasSub) {
        p->setFont(sf);
        p->setPen(subColor);
        const QRectF subRect(textLeft, r.top() + 31, textWidth, 16);
        p->drawText(subRect, Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(sf).elidedText(subtitle, Qt::ElideRight, static_cast<int>(textWidth)));
    }

    if (index.data(RoleFavorite).toBool()) {
        const int s = 12;
        QPixmap star = icons::pixmap("star-filled", selected ? QColor(Qt::white) : theme::kStar, s, p->device()->devicePixelRatio());
        p->drawPixmap(QPointF(r.right() - 12 - s, r.center().y() - s / 2.0), star);
    }
    p->restore();
}

QSize ItemListDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const {
    return QSize(0, theme::kListRowHeight);
}

}  // namespace mp
