#include "ui/ItemDetailPanel.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>

#include "autofill/AutoType.h"
#include "core/PasswordStrength.h"
#include "ui/FieldRow.h"
#include "ui/IconBadge.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

namespace mp {

namespace {
QFrame* hline(QWidget* parent) {
    auto* f = new QFrame(parent);
    f->setObjectName("HLine");
    f->setFrameShape(QFrame::NoFrame);
    f->setFixedHeight(1);
    return f;
}
}  // namespace

ItemDetailPanel::ItemDetailPanel(ClipboardManager* clipboard, QWidget* parent)
    : QWidget(parent), m_clipboard(clipboard) {
    setObjectName("DetailPanel");
    setAttribute(Qt::WA_StyledBackground, true);

    auto* stack = new QVBoxLayout(this);
    stack->setContentsMargins(0, 0, 0, 0);

    // Empty state --------------------------------------------------------------
    m_empty = new QWidget(this);
    auto* el = new QVBoxLayout(m_empty);
    el->setAlignment(Qt::AlignCenter);
    auto* emptyIcon = new QLabel(m_empty);
    emptyIcon->setPixmap(icons::pixmap("shield", theme::kTextMuted, 40, devicePixelRatioF()));
    emptyIcon->setAlignment(Qt::AlignCenter);
    el->addWidget(emptyIcon);
    auto* emptyText = new QLabel("Select an item to view its details", m_empty);
    emptyText->setObjectName("Muted");
    emptyText->setAlignment(Qt::AlignCenter);
    el->addWidget(emptyText);
    stack->addWidget(m_empty);

    // Content --------------------------------------------------------------------
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_content = new QWidget(m_scroll);
    m_content->setAttribute(Qt::WA_TranslucentBackground);
    auto* cl = new QVBoxLayout(m_content);
    cl->setContentsMargins(28, 18, 28, 24);
    cl->setSpacing(0);

    // top-right actions
    auto* actions = new QHBoxLayout();
    actions->setContentsMargins(0, 0, 0, 0);
    actions->addStretch();
    m_autofill = new QPushButton(m_content);
    m_autofill->setIcon(icons::icon("zap", theme::kText, 14));
    m_autofill->setText("Autofill");
    m_autofill->setToolTip("Type these credentials into the previously focused window");
    m_autofill->setCursor(Qt::PointingHandCursor);
    m_autofill->setVisible(AutoType::isSupported());
    connect(m_autofill, &QPushButton::clicked, this, [this] { emit autofillRequested(m_itemId); });
    actions->addWidget(m_autofill);
    m_restore = new QPushButton(m_content);
    m_restore->setIcon(icons::icon("rotate-ccw", theme::kText, 14));
    m_restore->setText("Restore");
    m_restore->setCursor(Qt::PointingHandCursor);
    connect(m_restore, &QPushButton::clicked, this, [this] { emit restoreRequested(m_itemId); });
    actions->addWidget(m_restore);
    m_edit = new QPushButton(m_content);
    m_edit->setIcon(icons::icon("edit", theme::kText, 14));
    m_edit->setText("Edit");
    m_edit->setCursor(Qt::PointingHandCursor);
    connect(m_edit, &QPushButton::clicked, this, [this] { emit editRequested(m_itemId); });
    actions->addWidget(m_edit);
    m_delete = new QPushButton(m_content);
    m_delete->setObjectName("Danger");
    m_delete->setIcon(icons::icon("trash", theme::kDanger, 14));
    m_delete->setText("Delete");
    m_delete->setCursor(Qt::PointingHandCursor);
    connect(m_delete, &QPushButton::clicked, this, [this] { emit deleteRequested(m_itemId); });
    actions->addWidget(m_delete);
    cl->addLayout(actions);
    cl->addSpacing(10);

    // header: badge + title/subtitle + star
    auto* header = new QHBoxLayout();
    header->setSpacing(14);
    m_badge = new IconBadge(52, m_content);
    header->addWidget(m_badge, 0, Qt::AlignTop);
    auto* titles = new QVBoxLayout();
    titles->setSpacing(2);
    auto* titleRow = new QHBoxLayout();
    titleRow->setSpacing(8);
    m_title = new QLabel(m_content);
    m_title->setObjectName("Title");
    m_title->setTextInteractionFlags(Qt::TextSelectableByMouse);
    titleRow->addWidget(m_title);
    m_star = new QToolButton(m_content);
    m_star->setFixedSize(24, 24);
    m_star->setCursor(Qt::PointingHandCursor);
    m_star->setToolTip("Toggle favourite");
    connect(m_star, &QToolButton::clicked, this, [this] {
        emit favoriteToggled(m_itemId, !m_favorite);
    });
    titleRow->addWidget(m_star);
    titleRow->addStretch();
    titles->addLayout(titleRow);
    m_subtitle = new QLabel(m_content);
    m_subtitle->setObjectName("Subtitle");
    titles->addWidget(m_subtitle);
    header->addLayout(titles, 1);
    header->setAlignment(titles, Qt::AlignVCenter);
    cl->addLayout(header);
    cl->addSpacing(22);

    auto* fieldsHost = new QWidget(m_content);
    fieldsHost->setAttribute(Qt::WA_TranslucentBackground);
    m_fields = new QVBoxLayout(fieldsHost);
    m_fields->setContentsMargins(0, 0, 0, 0);
    m_fields->setSpacing(0);
    cl->addWidget(fieldsHost);

    cl->addSpacing(18);
    m_meta = new QLabel(m_content);
    m_meta->setObjectName("Muted");
    m_meta->setWordWrap(true);
    cl->addWidget(m_meta);
    cl->addStretch();

    m_scroll->setWidget(m_content);
    stack->addWidget(m_scroll);
    clear();
}

void ItemDetailPanel::clear() {
    m_itemId.clear();
    m_scroll->hide();
    m_empty->show();
}

void ItemDetailPanel::setItem(const Item* item, const QString& folderName) {
    if (!item) { clear(); return; }
    m_itemId = item->id;
    m_favorite = item->favorite;
    m_badge->setItem(*item);
    m_title->setText(item->title.isEmpty() ? QStringLiteral("Untitled") : item->title);
    QString sub = itemTypeLabel(item->type);
    if (!folderName.isEmpty()) sub += QStringLiteral("  ·  ") + folderName;
    m_subtitle->setText(sub);
    m_star->setIcon(icons::icon(item->favorite ? "star-filled" : "star",
                                item->favorite ? theme::kStar : theme::kTextMuted, 18));
    m_star->setIconSize(QSize(18, 18));
    m_restore->setVisible(item->trashed);
    m_edit->setVisible(!item->trashed);
    m_autofill->setVisible(AutoType::isSupported() && !item->trashed &&
                           (item->type == ItemType::Login || item->type == ItemType::Card ||
                            item->type == ItemType::Identity));
    m_delete->setText(item->trashed ? "Delete Forever" : "Delete");
    rebuildFields(*item, folderName);
    m_empty->hide();
    m_scroll->show();
    m_scroll->verticalScrollBar()->setValue(0);
}

void ItemDetailPanel::rebuildFields(const Item& item, const QString&) {
    while (QLayoutItem* li = m_fields->takeAt(0)) {
        if (QWidget* w = li->widget()) w->deleteLater();
        delete li;
    }
    bool first = true;
    for (const FieldDef& def : fieldSchema(item.type)) {
        const QString value = item.field(def.key);
        // Hide empty optional secrets to keep the view tidy, but always show
        // the primary fields (first three of the schema).
        const int idx = static_cast<int>(&def - fieldSchema(item.type).data());
        if (value.trimmed().isEmpty() && idx >= 3) continue;
        if (!first) m_fields->addWidget(hline(m_content));
        first = false;
        auto* row = new FieldRow(def, value, m_clipboard, m_content);
        connect(row, &FieldRow::openUrlRequested, this, &ItemDetailPanel::openUrlRequested);
        m_fields->addWidget(row);

        if (def.key == "password" && !value.isEmpty()) {
            const StrengthResult s = estimateStrength(value);
            auto* strength = new QLabel(m_content);
            strength->setObjectName("Hint");
            const QColor c = s.score >= 3 ? theme::kSuccess : (s.score == 2 ? theme::kStar : theme::kDanger);
            strength->setText(QStringLiteral("<span style='color:%1'>●</span> %2 · ~%3 bits%4")
                                  .arg(c.name(), s.label)
                                  .arg(static_cast<int>(s.entropyBits))
                                  .arg(s.warning.isEmpty() ? QString() : QStringLiteral(" · ") + s.warning));
            strength->setContentsMargins(0, 0, 0, 8);
            m_fields->addWidget(strength);
        }
    }
    // custom fields
    for (auto it = item.fields.constBegin(); it != item.fields.constEnd(); ++it) {
        if (!it.key().startsWith("custom:")) continue;
        FieldDef def{it.key(), it.key().mid(7), FieldKind::Secret, false};
        m_fields->addWidget(hline(m_content));
        m_fields->addWidget(new FieldRow(def, it.value(), m_clipboard, m_content));
    }

    QString meta = QStringLiteral("Created %1  ·  Modified %2")
                       .arg(item.created.toLocalTime().toString("MMM d, yyyy"),
                            item.modified.toLocalTime().toString("MMM d, yyyy h:mm AP"));
    if (item.passwordChanged.isValid())
        meta += QStringLiteral("  ·  Password changed %1").arg(item.passwordChanged.toLocalTime().toString("MMM d, yyyy"));
    m_meta->setText(meta);
}

}  // namespace mp
