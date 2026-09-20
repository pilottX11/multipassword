#include "ui/Theme.h"

#include <QFontDatabase>

namespace mp::theme {

QString fontFamily() {
#ifdef _WIN32
    return QStringLiteral("Segoe UI Variable Text, Segoe UI, Inter, sans-serif");
#elif defined(__APPLE__)
    return QStringLiteral("SF Pro Text, Helvetica Neue, sans-serif");
#else
    return QStringLiteral("Inter, Noto Sans, DejaVu Sans, sans-serif");
#endif
}

QString stylesheet() {
    static const QString qss = QString::fromUtf8(R"QSS(
* {
    font-family: %FONT%;
    font-size: 13px;
    color: %TEXT%;
    outline: none;
}
QMainWindow, QDialog, QWidget#Root { background: %DETAIL%; }

QWidget#Sidebar { background: %SIDEBAR%; border-right: 1px solid %SEP%; }
QWidget#ListPanel { background: %LIST%; border-right: 1px solid %SEP%; }
QWidget#DetailPanel, QWidget#EditPanel { background: %DETAIL%; }
QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }

QLabel { background: transparent; }
QLabel#SectionHeader { color: %MUTED%; font-size: 11px; font-weight: 600; padding: 10px 14px 4px 14px; }
QLabel#FieldLabel { color: %SECONDARY%; font-size: 11px; font-weight: 600; }
QLabel#FieldValue { color: %TEXT%; font-size: 13px; }
QLabel#FieldValueMono { color: %TEXT%; font-size: 13px; font-family: Consolas, "Cascadia Mono", monospace; }
QLabel#Title { font-size: 20px; font-weight: 700; }
QLabel#Subtitle { color: %SECONDARY%; font-size: 12px; }
QLabel#Muted { color: %MUTED%; font-size: 12px; }
QLabel#Hint { color: %SECONDARY%; font-size: 12px; }
QLabel#Error { color: %DANGER%; font-size: 12px; }
QLabel#Success { color: %SUCCESS%; font-size: 12px; }
QLabel#Link { color: %ACCENT%; }

QLineEdit, QTextEdit, QPlainTextEdit, QComboBox, QSpinBox {
    background: %INPUT%;
    border: 1px solid %SEP%;
    border-radius: 7px;
    padding: 6px 10px;
    selection-background-color: %ACCENT%;
}
QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QComboBox:focus, QSpinBox:focus {
    border: 1px solid %ACCENT%;
}
QLineEdit#Search {
    background: %INPUT%;
    border: 1px solid %SEP%;
    border-radius: 8px;
    padding: 6px 10px 6px 30px;
    font-size: 12px;
}
QLineEdit::placeholder { color: %MUTED%; }
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView {
    background: %INPUT%; border: 1px solid %SEP%; selection-background-color: %ACCENT%;
    outline: none;
}
QSpinBox::up-button, QSpinBox::down-button { width: 14px; border: none; background: transparent; }

QPushButton {
    background: %BUTTON%;
    border: 1px solid %SEP%;
    border-radius: 7px;
    padding: 6px 12px;
    font-weight: 600;
    font-size: 12px;
}
QPushButton:hover { background: %HOVER%; }
QPushButton:pressed { background: %SEP%; }
QPushButton:disabled { color: %MUTED%; }
QPushButton#Primary { background: %ACCENT%; border: 1px solid %ACCENT%; color: white; }
QPushButton#Primary:hover { background: %ACCENT_HOVER%; }
QPushButton#Primary:disabled { background: #2a3552; border-color: #2a3552; color: #7d89ad; }
QPushButton#Danger { color: %DANGER%; }
QPushButton#Danger:hover { background: rgba(229,72,72,0.15); border-color: %DANGER%; }
QPushButton#Flat { background: transparent; border: none; padding: 4px 6px; }
QPushButton#Flat:hover { background: %HOVER%; }
QPushButton#AddButton {
    background: %ACCENT%; border: none; border-radius: 8px; padding: 0; min-width: 30px; min-height: 30px;
    max-width: 30px; max-height: 30px;
}
QPushButton#AddButton:hover { background: %ACCENT_HOVER%; }
QPushButton#NewFolder { background: transparent; border: none; color: %SECONDARY%; text-align: left; padding: 6px 12px; font-weight: 500; }
QPushButton#NewFolder:hover { color: %TEXT%; }

QToolButton { background: transparent; border: none; border-radius: 6px; padding: 4px; }
QToolButton:hover { background: %HOVER%; }
QToolButton:pressed { background: %SEP%; }
QToolButton::menu-indicator { image: none; }

QListView {
    background: transparent;
    border: none;
    outline: none;
    padding: 0 8px;
}
QListView::item { border-radius: 8px; }

QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: #3a3a3f; border-radius: 4px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: #4a4a50; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QScrollBar:horizontal { height: 0; }

QMenu { background: %INPUT%; border: 1px solid %SEP%; border-radius: 8px; padding: 6px; }
QMenu::item { padding: 6px 24px 6px 12px; border-radius: 5px; }
QMenu::item:selected { background: %ACCENT%; }
QMenu::separator { height: 1px; background: %SEP%; margin: 4px 8px; }

QCheckBox { spacing: 8px; }
QCheckBox::indicator { width: 16px; height: 16px; border-radius: 4px; border: 1px solid %SEP%; background: %INPUT%; }
QCheckBox::indicator:checked { background: %ACCENT%; border-color: %ACCENT%; }

QSlider::groove:horizontal { height: 4px; background: %SEP%; border-radius: 2px; }
QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0; background: %ACCENT%; border-radius: 7px; }

QProgressBar { background: %SEP%; border: none; border-radius: 3px; height: 6px; text-align: center; }
QProgressBar::chunk { border-radius: 3px; background: %ACCENT%; }

QFrame#HLine { background: %SEP%; max-height: 1px; min-height: 1px; border: none; }
QFrame#Card { background: %LIST%; border: 1px solid %SEP%; border-radius: 10px; }

QToolTip { background: %INPUT%; color: %TEXT%; border: 1px solid %SEP%; padding: 4px 8px; }
QMessageBox { background: %DETAIL%; }
QTabWidget::pane { border: 1px solid %SEP%; border-radius: 8px; top: -1px; }
QTabBar::tab { background: transparent; padding: 8px 14px; color: %SECONDARY%; border-bottom: 2px solid transparent; }
QTabBar::tab:selected { color: %TEXT%; border-bottom: 2px solid %ACCENT%; }
)QSS");
    QString out = qss;
    out.replace("%FONT%", fontFamily());
    out.replace("%TEXT%", kText.name());
    out.replace("%SECONDARY%", kTextSecondary.name());
    out.replace("%MUTED%", kTextMuted.name());
    out.replace("%SIDEBAR%", kSidebarBg.name());
    out.replace("%LIST%", kListBg.name());
    out.replace("%DETAIL%", kDetailBg.name());
    out.replace("%SEP%", kSeparator.name());
    out.replace("%INPUT%", kInputBg.name());
    out.replace("%HOVER%", kHover.name());
    out.replace("%BUTTON%", kButtonBg.name());
    out.replace("%ACCENT_HOVER%", kAccentHover.name());
    out.replace("%ACCENT%", kAccent.name());
    out.replace("%DANGER%", kDanger.name());
    out.replace("%SUCCESS%", kSuccess.name());
    return out;
}

}  // namespace mp::theme
