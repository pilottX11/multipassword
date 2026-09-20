#pragma once
// Inline Feather-style line icons rendered from SVG at any colour/size.

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

namespace mp::icons {

// Names: grid, star, star-filled, trash, key, credit-card, user, file-text,
//        wallet, folder, plus, search, edit, eye, eye-off, copy, lock,
//        unlock, settings, refresh, external-link, check, x, chevron-down,
//        zap (autofill), shield, clock, more
QString svg(const QString& name, const QColor& color, qreal strokeWidth = 1.8);
QIcon icon(const QString& name, const QColor& color, int size = 16);
QPixmap pixmap(const QString& name, const QColor& color, int size = 16, qreal dpr = 1.0);

}  // namespace mp::icons
