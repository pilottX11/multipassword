#include "ui/Sidebar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

#include "ui/Icons.h"
#include "ui/Theme.h"

namespace mp {

// ---------------------------------------------------------------------------
// SidebarRow: icon + label + optional count, with hover/selected states.
// ---------------------------------------------------------------------------
class SidebarRow : public QWidget {
    Q_OBJECT
public:
    SidebarRow(const QString& icon, const QString& label, const ListFilter& filter,
               const QColor& iconColor, QWidget* parent = nullptr)
        : QWidget(parent), m_icon(icon), m_label(label), m_filter(filter), m_iconColor(iconColor) {
        setFixedHeight(theme::kSidebarRowHeight);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
    }

    ListFilter filter() const { return m_filter; }
    void setSelected(bool s) { if (m_selected != s) { m_selected = s; update(); } }
    bool selected() const { return m_selected; }
    void setCount(int c) { if (m_count != c) { m_count = c; update(); } }
    void setLabel(const QString& l) { m_label = l; update(); }
    QString label() const { return m_label; }

signals:
    void clicked(const ListFilter& f);
    void contextMenu(const ListFilter& f, const QPoint& globalPos);

protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) emit clicked(m_filter);
        else if (e->button() == Qt::RightButton) emit contextMenu(m_filter, e->globalPosition().toPoint());
    }
    void enterEvent(QEnterEvent*) override { m_hover = true; update(); }
    void leaveEvent(QEvent*) override { m_hover = false; update(); }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = rect().adjusted(8, 1, -8, -1);
        if (m_selected) {
            QPainterPath path;
            path.addRoundedRect(r, 6, 6);
            p.fillPath(path, theme::kAccent);
        } else if (m_hover) {
            QPainterPath path;
            path.addRoundedRect(r, 6, 6);
            p.fillPath(path, theme::kHover);
        }
        const QColor fg = m_selected ? QColor(Qt::white) : theme::kText;
        const QColor iconColor = m_selected ? QColor(Qt::white) : m_iconColor;
        const int iconSize = 15;
        const QRectF iconRect(r.left() + 10, r.center().y() - iconSize / 2.0, iconSize, iconSize);
        QPixmap pm = icons::pixmap(m_icon, iconColor, iconSize, devicePixelRatioF());
        p.drawPixmap(iconRect.topLeft(), pm);

        QFont f = font();
        f.setPixelSize(12);
        f.setWeight(m_selected ? QFont::DemiBold : QFont::Medium);
        p.setFont(f);
        p.setPen(fg);
        QRectF textRect(r.left() + 10 + iconSize + 9, r.top(), r.width() - 60, r.height());
        const QString elided = p.fontMetrics().elidedText(m_label, Qt::ElideRight, static_cast<int>(textRect.width()));
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, elided);

        if (m_count >= 0) {
            f.setPixelSize(11);
            f.setWeight(QFont::Medium);
            p.setFont(f);
            p.setPen(m_selected ? QColor(255, 255, 255, 220) : theme::kTextMuted);
            p.drawText(r.adjusted(0, 0, -10, 0), Qt::AlignVCenter | Qt::AlignRight, QString::number(m_count));
        }
    }

private:
    QString m_icon;
    QString m_label;
    ListFilter m_filter;
    QColor m_iconColor;
    bool m_selected = false;
    bool m_hover = false;
    int m_count = -1;
};

// ---------------------------------------------------------------------------

Sidebar::Sidebar(Vault* vault, QWidget* parent) : QWidget(parent), m_vault(vault) {
    setObjectName("Sidebar");
    setFixedWidth(theme::kSidebarWidth);
    setAttribute(Qt::WA_StyledBackground, true);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // App header with lock/settings actions.
    auto* header = new QWidget(this);
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(16, 14, 10, 8);
    auto* brand = new QLabel("multipassword", header);
    QFont bf = brand->font();
    bf.setPixelSize(13);
    bf.setWeight(QFont::Bold);
    brand->setFont(bf);
    hl->addWidget(brand);
    hl->addStretch();
    auto* settingsBtn = new QToolButton(header);
    settingsBtn->setIcon(icons::icon("settings", theme::kTextSecondary, 15));
    settingsBtn->setToolTip("Settings");
    settingsBtn->setCursor(Qt::PointingHandCursor);
    connect(settingsBtn, &QToolButton::clicked, this, &Sidebar::settingsRequested);
    hl->addWidget(settingsBtn);
    auto* lockBtn = new QToolButton(header);
    lockBtn->setIcon(icons::icon("lock", theme::kTextSecondary, 15));
    lockBtn->setToolTip("Lock vault (Ctrl+L)");
    lockBtn->setCursor(Qt::PointingHandCursor);
    connect(lockBtn, &QToolButton::clicked, this, &Sidebar::lockRequested);
    hl->addWidget(lockBtn);
    outer->addWidget(header);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget(scroll);
    content->setAttribute(Qt::WA_TranslucentBackground);
    auto* lay = new QVBoxLayout(content);
    lay->setContentsMargins(0, 4, 0, 8);
    lay->setSpacing(1);

    m_allRow = addRow(lay, "grid", "All Items", ListFilter{ListFilter::All}, theme::kText);
    m_favRow = addRow(lay, "star", "Favorites", ListFilter{ListFilter::Favorites}, theme::kText);
    m_trashRow = addRow(lay, "trash", "Trash", ListFilter{ListFilter::Trash}, theme::kText);
    connect(m_trashRow, &SidebarRow::contextMenu, this, [this](const ListFilter&, const QPoint& pos) {
        QMenu menu(this);
        QAction* empty = menu.addAction(icons::icon("trash", theme::kDanger), "Empty Trash");
        if (menu.exec(pos) == empty) emit emptyTrashRequested();
    });

    auto* typesHeader = new QLabel("Types", content);
    typesHeader->setObjectName("SectionHeader");
    lay->addWidget(typesHeader);
    addRow(lay, "key", "Login", ListFilter{ListFilter::Type, ItemType::Login}, theme::kTextSecondary);
    addRow(lay, "credit-card", "Card", ListFilter{ListFilter::Type, ItemType::Card}, theme::kTextSecondary);
    addRow(lay, "user", "Identity", ListFilter{ListFilter::Type, ItemType::Identity}, theme::kTextSecondary);
    addRow(lay, "file-text", "Secure Note", ListFilter{ListFilter::Type, ItemType::SecureNote}, theme::kTextSecondary);
    addRow(lay, "wallet", "Crypto Wallet", ListFilter{ListFilter::Type, ItemType::CryptoWallet}, theme::kTextSecondary);

    m_foldersHeader = new QLabel("Folders", content);
    m_foldersHeader->setObjectName("SectionHeader");
    lay->addWidget(m_foldersHeader);
    auto* folderHost = new QWidget(content);
    folderHost->setAttribute(Qt::WA_TranslucentBackground);
    m_folderLayout = new QVBoxLayout(folderHost);
    m_folderLayout->setContentsMargins(0, 0, 0, 0);
    m_folderLayout->setSpacing(1);
    lay->addWidget(folderHost);
    lay->addStretch();

    scroll->setWidget(content);
    outer->addWidget(scroll, 1);

    auto* newFolder = new QPushButton(content);
    newFolder->setObjectName("NewFolder");
    newFolder->setIcon(icons::icon("plus", theme::kTextSecondary, 14));
    newFolder->setText("New Folder");
    newFolder->setCursor(Qt::PointingHandCursor);
    connect(newFolder, &QPushButton::clicked, this, &Sidebar::newFolderRequested);
    outer->addWidget(newFolder);

    connect(m_vault, &Vault::changed, this, &Sidebar::refresh);
    m_current = ListFilter{ListFilter::All};
    refresh();
}

SidebarRow* Sidebar::addRow(QVBoxLayout* into, const QString& icon, const QString& label,
                            const ListFilter& filter, const QColor& iconColor) {
    auto* row = new SidebarRow(icon, label, filter, iconColor, this);
    connect(row, &SidebarRow::clicked, this, &Sidebar::setFilter);
    into->addWidget(row);
    m_rows.push_back(row);
    return row;
}

void Sidebar::setFilter(const ListFilter& f) {
    m_current = f;
    updateSelection();
    emit filterChanged(f);
}

void Sidebar::updateSelection() {
    for (SidebarRow* r : m_rows) r->setSelected(r->filter() == m_current);
    for (SidebarRow* r : m_folderRows) r->setSelected(r->filter() == m_current);
}

void Sidebar::refresh() {
    int all = 0, fav = 0, trash = 0;
    for (const Item& i : m_vault->items()) {
        if (i.trashed) ++trash;
        else { ++all; if (i.favorite) ++fav; }
    }
    m_allRow->setCount(all);
    m_favRow->setCount(fav);
    m_trashRow->setCount(trash);
    rebuildFolders();
    updateSelection();
}

void Sidebar::rebuildFolders() {
    // Rebuild only when the folder set changed.
    QStringList ids;
    for (const Folder& f : m_vault->folders()) ids << f.id + "\x1f" + f.name;
    QStringList existing;
    for (SidebarRow* r : m_folderRows) existing << r->filter().folderId + "\x1f" + r->label();
    if (ids == existing) return;

    for (SidebarRow* r : m_folderRows) { m_folderLayout->removeWidget(r); r->deleteLater(); }
    m_folderRows.clear();
    for (const Folder& f : m_vault->folders()) {
        ListFilter lf{ListFilter::Folder};
        lf.folderId = f.id;
        auto* row = new SidebarRow("folder", f.name, lf, theme::kTextSecondary, this);
        connect(row, &SidebarRow::clicked, this, &Sidebar::setFilter);
        connect(row, &SidebarRow::contextMenu, this, [this](const ListFilter& f, const QPoint& pos) {
            QMenu menu(this);
            QAction* rename = menu.addAction(icons::icon("edit", theme::kText), "Rename Folder");
            QAction* del = menu.addAction(icons::icon("trash", theme::kDanger), "Delete Folder");
            QAction* chosen = menu.exec(pos);
            if (chosen == rename) emit renameFolderRequested(f.folderId);
            else if (chosen == del) emit deleteFolderRequested(f.folderId);
        });
        m_folderLayout->addWidget(row);
        m_folderRows.push_back(row);
    }
    m_foldersHeader->setVisible(true);
    // If the current filter pointed at a removed folder, fall back to All.
    if (m_current.kind == ListFilter::Folder && !m_vault->findFolder(m_current.folderId))
        setFilter(ListFilter{ListFilter::All});
}

void Sidebar::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), theme::kSidebarBg);
    p.fillRect(QRect(width() - 1, 0, 1, height()), theme::kSeparator);
}

}  // namespace mp

#include "Sidebar.moc"
