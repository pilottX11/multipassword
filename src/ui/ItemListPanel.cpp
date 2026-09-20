#include "ui/ItemListPanel.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>

#include "ui/Icons.h"
#include "ui/ItemListDelegate.h"
#include "ui/Theme.h"

namespace mp {

// --- model -------------------------------------------------------------------

ItemListModel::ItemListModel(QObject* parent) : QAbstractListModel(parent) {}

void ItemListModel::setItems(QVector<Item> items) {
    beginResetModel();
    m_items = std::move(items);
    endResetModel();
}

int ItemListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_items.size();
}

QVariant ItemListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_items.size()) return {};
    const Item& it = m_items[index.row()];
    switch (role) {
        case Qt::DisplayRole:
        case RoleTitle: return it.title;
        case RoleSubtitle: return it.subtitle();
        case RoleItemId: return it.id;
        case RoleFavorite: return it.favorite;
        case RoleItemPtr: return QVariant::fromValue(static_cast<void*>(const_cast<Item*>(&it)));
        default: return {};
    }
}

int ItemListModel::rowOf(const QString& itemId) const {
    for (int i = 0; i < m_items.size(); ++i)
        if (m_items[i].id == itemId) return i;
    return -1;
}

const Item* ItemListModel::itemAt(int row) const {
    return (row >= 0 && row < m_items.size()) ? &m_items[row] : nullptr;
}

// --- panel -------------------------------------------------------------------

ItemListPanel::ItemListPanel(Vault* vault, QWidget* parent) : QWidget(parent), m_vault(vault) {
    setObjectName("ListPanel");
    setFixedWidth(theme::kListWidth);
    setAttribute(Qt::WA_StyledBackground, true);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    auto* top = new QWidget(this);
    auto* tl = new QHBoxLayout(top);
    tl->setContentsMargins(12, 12, 12, 10);
    tl->setSpacing(8);

    auto* searchHost = new QWidget(top);
    auto* sl = new QHBoxLayout(searchHost);
    sl->setContentsMargins(0, 0, 0, 0);
    m_search = new QLineEdit(searchHost);
    m_search->setObjectName("Search");
    m_search->setPlaceholderText("Search Vault");
    m_search->setClearButtonEnabled(true);
    sl->addWidget(m_search);
    auto* searchIcon = new QLabel(searchHost);
    searchIcon->setPixmap(icons::pixmap("search", theme::kTextMuted, 14, devicePixelRatioF()));
    searchIcon->setFixedSize(14, 14);
    searchIcon->setAttribute(Qt::WA_TransparentForMouseEvents);
    searchIcon->setParent(m_search);
    searchIcon->move(10, 8);
    tl->addWidget(searchHost, 1);

    m_add = new QPushButton(top);
    m_add->setObjectName("AddButton");
    m_add->setIcon(icons::icon("plus", Qt::white, 16));
    m_add->setIconSize(QSize(16, 16));
    m_add->setToolTip("New item (Ctrl+N)");
    m_add->setCursor(Qt::PointingHandCursor);
    connect(m_add, &QPushButton::clicked, this, &ItemListPanel::showAddMenu);
    tl->addWidget(m_add);
    lay->addWidget(top);

    m_model = new ItemListModel(this);
    m_view = new QListView(this);
    m_view->setModel(m_model);
    m_view->setItemDelegate(new ItemListDelegate(m_view));
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_view->setMouseTracking(true);
    m_view->setUniformItemSizes(true);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setSpacing(1);
    lay->addWidget(m_view, 1);

    m_empty = new QLabel("No items", this);
    m_empty->setObjectName("Muted");
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->hide();
    lay->addWidget(m_empty);

    connect(m_view->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex& cur, const QModelIndex&) {
                emit itemSelected(cur.isValid() ? cur.data(RoleItemId).toString() : QString());
            });
    connect(m_search, &QLineEdit::textChanged, this, [this] { rebuild(); });
    connect(m_vault, &Vault::changed, this, &ItemListPanel::refresh);

    rebuild();
}

void ItemListPanel::setFilter(const ListFilter& f) {
    m_filter = f;
    rebuild();
}

void ItemListPanel::focusSearch() {
    m_search->setFocus();
    m_search->selectAll();
}

QString ItemListPanel::selectedItemId() const {
    const QModelIndex cur = m_view->currentIndex();
    return cur.isValid() ? cur.data(RoleItemId).toString() : QString();
}

void ItemListPanel::selectItem(const QString& id) {
    const int row = m_model->rowOf(id);
    if (row < 0) { m_pendingSelect = id; return; }
    m_view->setCurrentIndex(m_model->index(row));
    m_view->scrollTo(m_model->index(row));
}

void ItemListPanel::refresh() {
    rebuild();
}

void ItemListPanel::rebuild() {
    const QString wanted = m_pendingSelect.isEmpty() ? selectedItemId() : m_pendingSelect;
    m_pendingSelect.clear();
    const QString q = m_search->text().trimmed().toLower();

    QVector<Item> items;
    for (const Item& it : m_vault->items()) {
        if (!m_filter.matches(it)) continue;
        if (!q.isEmpty()) {
            bool hit = it.title.toLower().contains(q) || it.subtitle().toLower().contains(q) ||
                       it.website().toLower().contains(q);
            if (!hit) {
                // search non-secret fields
                for (const FieldDef& fd : fieldSchema(it.type)) {
                    if (fd.kind == FieldKind::Secret || fd.kind == FieldKind::SeedPhrase || fd.kind == FieldKind::Totp) continue;
                    if (it.field(fd.key).toLower().contains(q)) { hit = true; break; }
                }
            }
            if (!hit) continue;
        }
        items.push_back(it);
    }
    std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        return a.title.compare(b.title, Qt::CaseInsensitive) < 0;
    });
    m_model->setItems(std::move(items));
    m_empty->setVisible(m_model->rowCount() == 0);
    m_empty->setText(q.isEmpty() ? "No items" : "No results");

    const int row = m_model->rowOf(wanted);
    if (row >= 0) {
        m_view->setCurrentIndex(m_model->index(row));
    } else if (!q.isEmpty() && m_model->rowCount() > 0) {
        // While searching, always show the best hit so Enter/Autofill act on it.
        m_view->setCurrentIndex(m_model->index(0));
    } else {
        m_view->clearSelection();
        m_view->setCurrentIndex(QModelIndex());
        emit itemSelected(QString());
    }
}

void ItemListPanel::showAddMenu() {
    QMenu menu(this);
    for (ItemType t : allItemTypes()) {
        QString iconName;
        switch (t) {
            case ItemType::Login: iconName = "key"; break;
            case ItemType::Card: iconName = "credit-card"; break;
            case ItemType::Identity: iconName = "user"; break;
            case ItemType::SecureNote: iconName = "file-text"; break;
            case ItemType::CryptoWallet: iconName = "wallet"; break;
        }
        QAction* a = menu.addAction(icons::icon(iconName, theme::kText), itemTypeLabel(t));
        connect(a, &QAction::triggered, this, [this, t] { emit newItemRequested(t); });
    }
    menu.exec(m_add->mapToGlobal(QPoint(0, m_add->height() + 4)));
}

}  // namespace mp
