#pragma once
// Visual constants replicating the reference dark three-pane design.

#include <QColor>
#include <QString>

namespace mp::theme {

// Backgrounds (left -> right get progressively lighter, as in the mockup)
inline const QColor kSidebarBg{0x17, 0x17, 0x18};
inline const QColor kListBg{0x1d, 0x1d, 0x1f};
inline const QColor kDetailBg{0x23, 0x23, 0x25};
inline const QColor kSeparator{0x2c, 0x2c, 0x2f};
inline const QColor kInputBg{0x2a, 0x2a, 0x2d};
inline const QColor kHover{0x2a, 0x2a, 0x2e};
inline const QColor kButtonBg{0x2f, 0x2f, 0x33};

// Accent
inline const QColor kAccent{0x2f, 0x6b, 0xff};
inline const QColor kAccentHover{0x3d, 0x76, 0xff};
inline const QColor kStar{0xf5, 0xb4, 0x00};
inline const QColor kDanger{0xe5, 0x48, 0x48};
inline const QColor kSuccess{0x3d, 0xc2, 0x7a};

// Text
inline const QColor kText{0xf2, 0xf2, 0xf3};
inline const QColor kTextSecondary{0x9a, 0x9a, 0xa2};
inline const QColor kTextMuted{0x6e, 0x6e, 0x76};

// Layout
constexpr int kSidebarWidth = 208;
constexpr int kListWidth = 312;
constexpr int kListRowHeight = 60;
constexpr int kSidebarRowHeight = 30;
constexpr int kRadius = 8;

QString stylesheet();
QString fontFamily();

}  // namespace mp::theme
