#include "ui/AutofillPicker.h"

#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QVBoxLayout>

#include "ui/IconBadge.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

namespace mp {

AutofillPicker::AutofillPicker(Vault* vault, const ForegroundContext& ctx, QWidget* parent)
    : QDialog(parent), m_vault(vault), m_ctx(ctx) {
    setWindowTitle("Autofill");
    setWindowFlags(Qt::Dialog | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(380, 380);

    auto* frame = new QWidget(this);
    frame->setObjectName("Card");
    frame->setAttribute(Qt::WA_StyledBackground, true);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(frame);

    auto* lay = new QVBoxLayout(frame);
    lay->setContentsMargins(14, 14, 14, 14);
    lay->setSpacing(8);

    auto* title = new QLabel("Autofill", frame);
    title->setObjectName("Title");
    lay->addWidget(title);

    m_context = new QLabel(frame);
    m_context->setObjectName("Muted");
    m_context->setWordWrap(true);
    QString where = ctx.url.isEmpty() ? ctx.windowTitle : ctx.url;
    if (where.size() > 70) where = where.left(67) + "…";
    m_context->setText(where.isEmpty() ? "No target window detected" : "→ " + where);
    lay->addWidget(m_context);

    m_search = new QLineEdit(frame);
    m_search->setObjectName("Search");
    m_search->setPlaceholderText("Search Vault");
    lay->addWidget(m_search);

    m_list = new QListWidget(frame);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setIconSize(QSize(28, 28));
    m_list->setStyleSheet(QStringLiteral(
        "QListWidget::item{padding:6px 8px;border-radius:6px;} QListWidget::item:selected{background:%1;color:white;}")
                              .arg(theme::kAccent.name()));
    lay->addWidget(m_list, 1);

    auto* hint = new QLabel("Enter to fill  ·  Esc to cancel", frame);
    hint->setObjectName("Muted");
    hint->setAlignment(Qt::AlignCenter);
    lay->addWidget(hint);

    m_matches = AutofillEngine::match(vault->items(), ctx);
    connect(m_search, &QLineEdit::textChanged, this, &AutofillPicker::rebuild);
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem*) { choose(); });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) { choose(); });
    rebuild();
    m_search->setFocus();
}

void AutofillPicker::rebuild() {
    m_list->clear();
    const QString q = m_search->text().trimmed().toLower();
    auto addRow = [this](const Item& it, const QString& reason) {
        QPixmap pm(28 * 2, 28 * 2);
        pm.setDevicePixelRatio(2.0);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        paintBadge(p, QRectF(0, 0, 28, 28), it);
        p.end();
        auto* row = new QListWidgetItem(QIcon(pm), it.title + (it.subtitle().isEmpty() ? QString() : "  —  " + it.subtitle()));
        row->setData(Qt::UserRole, it.id);
        row->setToolTip(reason);
        m_list->addItem(row);
    };
    if (q.isEmpty()) {
        for (const AutofillMatch& m : m_matches)
            if (const Item* it = m_vault->findItem(m.itemId)) addRow(*it, m.reason);
        if (m_matches.isEmpty()) {
            for (const Item& it : m_vault->items())
                if (!it.trashed && it.type == ItemType::Login) addRow(it, "All logins");
        }
    } else {
        for (const Item& it : m_vault->items()) {
            if (it.trashed) continue;
            if (it.title.toLower().contains(q) || it.subtitle().toLower().contains(q) || it.website().toLower().contains(q))
                addRow(it, "Search result");
        }
    }
    if (m_list->count() > 0) m_list->setCurrentRow(0);
}

void AutofillPicker::choose() {
    QListWidgetItem* cur = m_list->currentItem();
    if (!cur) return;
    m_chosen = cur->data(Qt::UserRole).toString();
    accept();
}

void AutofillPicker::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) { reject(); return; }
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) { choose(); return; }
    if (e->key() == Qt::Key_Down || e->key() == Qt::Key_Up) {
        int r = m_list->currentRow() + (e->key() == Qt::Key_Down ? 1 : -1);
        r = std::clamp(r, 0, std::max(0, m_list->count() - 1));
        m_list->setCurrentRow(r);
        return;
    }
    QDialog::keyPressEvent(e);
}

}  // namespace mp
