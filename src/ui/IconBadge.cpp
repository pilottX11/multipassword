#include "ui/IconBadge.h"

#include <QHash>
#include <QPainterPath>
#include <QSvgRenderer>

#include "ui/Icons.h"
#include "ui/Theme.h"

namespace mp {

namespace {

// A handful of well-known brands get their real brand colour; everything
// else gets a stable colour derived from the title.
const QHash<QString, QColor>& brandColors() {
    static const QHash<QString, QColor> m = {
        {"adobe", QColor(0xed, 0x1c, 0x24)},     {"apple", QColor(0xf5, 0xf5, 0xf7)},
        {"dribbble", QColor(0xea, 0x4c, 0x89)},  {"etsy", QColor(0xf1, 0x64, 0x1e)},
        {"facebook", QColor(0x18, 0x77, 0xf2)},  {"google", QColor(0xff, 0xff, 0xff)},
        {"imdb", QColor(0xf5, 0xc5, 0x18)},      {"invision", QColor(0xff, 0x33, 0x66)},
        {"telegram", QColor(0x26, 0xa5, 0xe4)},  {"github", QColor(0x24, 0x29, 0x2f)},
        {"twitter", QColor(0x1d, 0x9b, 0xf0)},   {"x", QColor(0x00, 0x00, 0x00)},
        {"amazon", QColor(0xff, 0x99, 0x00)},    {"netflix", QColor(0xe5, 0x09, 0x14)},
        {"spotify", QColor(0x1d, 0xb9, 0x54)},   {"microsoft", QColor(0x00, 0x78, 0xd4)},
        {"discord", QColor(0x58, 0x65, 0xf2)},   {"slack", QColor(0x4a, 0x15, 0x4b)},
        {"reddit", QColor(0xff, 0x45, 0x00)},    {"linkedin", QColor(0x0a, 0x66, 0xc2)},
        {"paypal", QColor(0x00, 0x30, 0x87)},    {"dropbox", QColor(0x00, 0x61, 0xff)},
        {"instagram", QColor(0xe4, 0x40, 0x5f)}, {"youtube", QColor(0xff, 0x00, 0x00)},
        {"coinbase", QColor(0x00, 0x52, 0xff)},  {"binance", QColor(0xf3, 0xba, 0x2f)},
        {"metamask", QColor(0xf6, 0x85, 0x1b)},  {"ledger", QColor(0x00, 0x00, 0x00)},
        {"kraken", QColor(0x57, 0x41, 0xd9)},    {"steam", QColor(0x1b, 0x28, 0x38)},
        {"notion", QColor(0xff, 0xff, 0xff)},    {"figma", QColor(0xa2, 0x59, 0xff)},
    };
    return m;
}

QColor typeColor(ItemType t) {
    switch (t) {
        case ItemType::Login: return QColor(0x3b, 0x82, 0xf6);
        case ItemType::Card: return QColor(0x8b, 0x5c, 0xf6);
        case ItemType::Identity: return QColor(0x10, 0xb9, 0x81);
        case ItemType::SecureNote: return QColor(0xf5, 0x9e, 0x0b);
        case ItemType::CryptoWallet: return QColor(0xf9, 0x73, 0x16);
    }
    return theme::kAccent;
}

QString typeIconName(ItemType t) {
    switch (t) {
        case ItemType::Login: return "key";
        case ItemType::Card: return "credit-card";
        case ItemType::Identity: return "user";
        case ItemType::SecureNote: return "file-text";
        case ItemType::CryptoWallet: return "wallet";
    }
    return "key";
}

}  // namespace

QColor badgeColorFor(const Item& item) {
    if (item.type != ItemType::Login) return typeColor(item.type);
    const QString key = item.title.trimmed().toLower();
    const QString host = canonicalHost(item.website()).section('.', 0, 0);
    if (brandColors().contains(key)) return brandColors().value(key);
    if (brandColors().contains(host)) return brandColors().value(host);
    if (key.isEmpty()) return typeColor(item.type);
    // Stable hue from title hash.
    const size_t h = qHash(key);
    return QColor::fromHsl(static_cast<int>(h % 360), 150, 120);
}

void paintBadge(QPainter& p, const QRectF& rect, const Item& item, qreal radius) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    if (radius < 0) radius = rect.width() * 0.24;
    const QColor bg = badgeColorFor(item);
    QPainterPath path;
    path.addRoundedRect(rect, radius, radius);
    p.fillPath(path, bg);

    // Choose a foreground with enough contrast.
    const bool lightBg = bg.lightness() > 150;
    const QColor fg = lightBg ? QColor(0x1a, 0x1a, 0x1a) : QColor(Qt::white);

    if (item.type == ItemType::Login && !item.title.trimmed().isEmpty()) {
        QFont f = p.font();
        f.setPixelSize(static_cast<int>(rect.height() * 0.5));
        f.setWeight(QFont::Bold);
        p.setFont(f);
        p.setPen(fg);
        p.drawText(rect, Qt::AlignCenter, item.title.trimmed().left(1).toUpper());
    } else {
        const qreal inset = rect.width() * 0.25;
        QSvgRenderer r(icons::svg(typeIconName(item.type), fg, 2.0).toUtf8());
        r.render(&p, rect.adjusted(inset, inset, -inset, -inset));
    }
    p.restore();
}

IconBadge::IconBadge(int size, QWidget* parent) : QWidget(parent), m_size(size) {
    setFixedSize(size, size);
    setAttribute(Qt::WA_TranslucentBackground);
}

void IconBadge::setItem(const Item& item) {
    m_item = item;
    update();
}

void IconBadge::setItemType(ItemType type) {
    m_item = Item{};
    m_item.type = type;
    update();
}

void IconBadge::paintEvent(QPaintEvent*) {
    QPainter p(this);
    paintBadge(p, QRectF(0, 0, m_size, m_size), m_item);
}

}  // namespace mp
