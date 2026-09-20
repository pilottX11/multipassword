#include "ui/ItemEditPanel.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

#include "core/Bip39.h"
#include "core/PasswordGenerator.h"
#include "core/PasswordStrength.h"
#include "core/SecureMemory.h"
#include "core/Totp.h"
#include "ui/IconBadge.h"
#include "ui/Icons.h"
#include "ui/PasswordGeneratorDialog.h"
#include "ui/Theme.h"

namespace mp {

ItemEditPanel::ItemEditPanel(Vault* vault, QWidget* parent) : QWidget(parent), m_vault(vault) {
    setObjectName("EditPanel");
    setAttribute(Qt::WA_StyledBackground, true);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget(scroll);
    content->setAttribute(Qt::WA_TranslucentBackground);
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(28, 18, 28, 24);
    cl->setSpacing(12);

    auto* top = new QHBoxLayout();
    m_heading = new QLabel(content);
    m_heading->setObjectName("Title");
    top->addWidget(m_heading);
    top->addStretch();
    m_cancel = new QPushButton("Cancel", content);
    m_cancel->setCursor(Qt::PointingHandCursor);
    connect(m_cancel, &QPushButton::clicked, this, [this] { wipeForm(); emit cancelled(); });
    top->addWidget(m_cancel);
    m_save = new QPushButton("Save", content);
    m_save->setObjectName("Primary");
    m_save->setCursor(Qt::PointingHandCursor);
    m_save->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    connect(m_save, &QPushButton::clicked, this, [this] {
        QString err;
        if (!validate(err)) { m_error->setText(err); m_error->show(); return; }
        m_error->hide();
        Item it = currentItem();
        emit saved(it, m_isNew);
        wipeForm();
    });
    top->addWidget(m_save);
    cl->addLayout(top);

    auto* header = new QHBoxLayout();
    header->setSpacing(14);
    m_badge = new IconBadge(52, content);
    header->addWidget(m_badge, 0, Qt::AlignTop);
    auto* titleCol = new QVBoxLayout();
    titleCol->setSpacing(6);
    auto* titleLabel = new QLabel("Title", content);
    titleLabel->setObjectName("FieldLabel");
    titleCol->addWidget(titleLabel);
    m_title = new QLineEdit(content);
    m_title->setPlaceholderText("e.g. Adobe");
    connect(m_title, &QLineEdit::textChanged, this, [this](const QString& t) {
        Item preview = m_item;
        preview.title = t;
        if (auto* w = qobject_cast<QLineEdit*>(m_editors.value("website"))) preview.setField("website", w->text());
        m_badge->setItem(preview);
    });
    titleCol->addWidget(m_title);
    auto* folderLabel = new QLabel("Folder", content);
    folderLabel->setObjectName("FieldLabel");
    titleCol->addWidget(folderLabel);
    m_folder = new QComboBox(content);
    titleCol->addWidget(m_folder);
    header->addLayout(titleCol, 1);
    cl->addLayout(header);

    auto* formHost = new QWidget(content);
    formHost->setAttribute(Qt::WA_TranslucentBackground);
    m_form = new QVBoxLayout(formHost);
    m_form->setContentsMargins(0, 8, 0, 0);
    m_form->setSpacing(10);
    cl->addWidget(formHost);

    m_error = new QLabel(content);
    m_error->setObjectName("Error");
    m_error->setWordWrap(true);
    m_error->hide();
    cl->addWidget(m_error);
    cl->addStretch();

    scroll->setWidget(content);
    outer->addWidget(scroll);
}

ItemEditPanel::~ItemEditPanel() { wipeForm(); }

void ItemEditPanel::beginNew(ItemType type) {
    m_item = Item::create(type);
    m_isNew = true;
    m_heading->setText(QStringLiteral("New %1").arg(itemTypeLabel(type)));
    buildForm();
}

void ItemEditPanel::beginEdit(const Item& item) {
    m_item = item;
    m_isNew = false;
    m_heading->setText(QStringLiteral("Edit %1").arg(itemTypeLabel(item.type)));
    buildForm();
}

void ItemEditPanel::wipeForm() {
    for (QWidget* w : m_editors) {
        if (auto* le = qobject_cast<QLineEdit*>(w)) { QString t = le->text(); le->clear(); wipe(t); }
        else if (auto* te = qobject_cast<QPlainTextEdit*>(w)) { QString t = te->toPlainText(); te->clear(); wipe(t); }
    }
    for (auto& v : m_item.fields) wipe(v);
}

void ItemEditPanel::buildForm() {
    while (QLayoutItem* li = m_form->takeAt(0)) {
        if (QWidget* w = li->widget()) w->deleteLater();
        delete li;
    }
    m_editors.clear();
    m_strengthLabel = nullptr;
    m_strengthBar = nullptr;
    m_seedStatus = nullptr;
    m_error->hide();

    m_badge->setItem(m_item);
    m_title->setText(m_item.title);

    m_folder->clear();
    m_folder->addItem("No folder", QString());
    for (const Folder& f : m_vault->folders()) m_folder->addItem(f.name, f.id);
    const int fi = m_folder->findData(m_item.folderId);
    m_folder->setCurrentIndex(fi >= 0 ? fi : 0);

    for (const FieldDef& def : fieldSchema(m_item.type)) {
        auto* label = new QLabel(def.label, this);
        label->setObjectName("FieldLabel");
        m_form->addWidget(label);

        const QString value = m_item.field(def.key);
        if (def.kind == FieldKind::Multiline || def.kind == FieldKind::SeedPhrase) {
            auto* te = new QPlainTextEdit(this);
            te->setPlainText(value);
            te->setFixedHeight(def.kind == FieldKind::SeedPhrase ? 84 : 110);
            te->setTabChangesFocus(true);
            if (def.kind == FieldKind::SeedPhrase) {
                te->setPlaceholderText("12 or 24 words separated by spaces");
                QFont mono("Consolas");
                mono.setStyleHint(QFont::Monospace);
                te->setFont(mono);
                auto* row = new QHBoxLayout();
                row->addWidget(te, 1);
                auto* gen = new QToolButton(this);
                gen->setIcon(icons::icon("refresh", theme::kTextSecondary, 15));
                gen->setToolTip("Generate a new BIP-39 seed phrase");
                gen->setCursor(Qt::PointingHandCursor);
                connect(gen, &QToolButton::clicked, this, [this, te] { generateSeedInto(te); });
                row->addWidget(gen, 0, Qt::AlignTop);
                m_form->addLayout(row);
                m_seedStatus = new QLabel(this);
                m_seedStatus->setObjectName("Hint");
                m_seedStatus->setWordWrap(true);
                m_form->addWidget(m_seedStatus);
                connect(te, &QPlainTextEdit::textChanged, this, &ItemEditPanel::updateSeedStatus);
            } else {
                m_form->addWidget(te);
            }
            m_editors[def.key] = te;
        } else {
            auto* le = new QLineEdit(this);
            le->setText(value);
            if (def.kind == FieldKind::Secret || def.kind == FieldKind::Totp) {
                le->setEchoMode(QLineEdit::Password);
                auto* row = new QHBoxLayout();
                row->setSpacing(6);
                row->addWidget(le, 1);
                auto* reveal = new QToolButton(this);
                reveal->setIcon(icons::icon("eye", theme::kTextSecondary, 15));
                reveal->setCheckable(true);
                reveal->setCursor(Qt::PointingHandCursor);
                reveal->setToolTip("Reveal");
                connect(reveal, &QToolButton::toggled, this, [le, reveal](bool on) {
                    le->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
                    reveal->setIcon(icons::icon(on ? "eye-off" : "eye", theme::kTextSecondary, 15));
                });
                row->addWidget(reveal);
                if (def.key == "password" || def.key == "walletPassword" || def.key == "pin") {
                    auto* gen = new QToolButton(this);
                    gen->setIcon(icons::icon("refresh", theme::kTextSecondary, 15));
                    gen->setToolTip("Generate password");
                    gen->setCursor(Qt::PointingHandCursor);
                    connect(gen, &QToolButton::clicked, this, [this, le] { generatePasswordInto(le); });
                    row->addWidget(gen);
                }
                m_form->addLayout(row);
                if (def.key == "password") {
                    m_strengthBar = new QProgressBar(this);
                    m_strengthBar->setRange(0, 100);
                    m_strengthBar->setTextVisible(false);
                    m_strengthBar->setFixedHeight(6);
                    m_form->addWidget(m_strengthBar);
                    m_strengthLabel = new QLabel(this);
                    m_strengthLabel->setObjectName("Hint");
                    m_form->addWidget(m_strengthLabel);
                    connect(le, &QLineEdit::textChanged, this, &ItemEditPanel::updateStrength);
                }
                if (def.kind == FieldKind::Totp) le->setPlaceholderText("Base32 secret or otpauth:// URI");
            } else {
                if (def.kind == FieldKind::Url) {
                    le->setPlaceholderText("https://example.com");
                    connect(le, &QLineEdit::textChanged, this, [this](const QString& t) {
                        Item preview = m_item;
                        preview.title = m_title->text();
                        preview.setField("website", t);
                        m_badge->setItem(preview);
                    });
                }
                if (def.kind == FieldKind::Date) le->setPlaceholderText("YYYY-MM-DD");
                m_form->addWidget(le);
            }
            m_editors[def.key] = le;
        }
    }
    updateStrength();
    updateSeedStatus();
    m_title->setFocus();
}

void ItemEditPanel::updateStrength() {
    if (!m_strengthBar) return;
    auto* le = qobject_cast<QLineEdit*>(m_editors.value("password"));
    if (!le) return;
    const StrengthResult s = estimateStrength(le->text());
    const int pct = std::clamp(static_cast<int>(s.entropyBits / 100.0 * 100), 4, 100);
    m_strengthBar->setValue(le->text().isEmpty() ? 0 : pct);
    const QColor c = s.score >= 3 ? theme::kSuccess : (s.score == 2 ? theme::kStar : theme::kDanger);
    m_strengthBar->setStyleSheet(QStringLiteral("QProgressBar::chunk{background:%1;border-radius:3px;}").arg(c.name()));
    m_strengthLabel->setText(le->text().isEmpty()
                                 ? QString()
                                 : QStringLiteral("%1 · ~%2 bits%3").arg(s.label).arg(static_cast<int>(s.entropyBits))
                                       .arg(s.warning.isEmpty() ? QString() : "  ·  " + s.warning));
}

void ItemEditPanel::updateSeedStatus() {
    if (!m_seedStatus) return;
    auto* te = qobject_cast<QPlainTextEdit*>(m_editors.value("seedPhrase"));
    if (!te) return;
    const QString text = te->toPlainText();
    if (text.trimmed().isEmpty()) { m_seedStatus->clear(); return; }
    const bip39::Validation v = bip39::validate(text);
    m_seedStatus->setText(v.message);
    m_seedStatus->setStyleSheet(QStringLiteral("color:%1;").arg((v.valid ? theme::kSuccess : theme::kDanger).name()));
}

void ItemEditPanel::generatePasswordInto(QLineEdit* target) {
    PasswordGeneratorDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        target->setText(dlg.result());
        target->setEchoMode(QLineEdit::Normal);
    }
}

void ItemEditPanel::generateSeedInto(QPlainTextEdit* target) {
    QMenu menu(this);
    QAction* w12 = menu.addAction("Generate 12-word seed phrase");
    QAction* w24 = menu.addAction("Generate 24-word seed phrase");
    QAction* chosen = menu.exec(QCursor::pos());
    if (chosen == w12) target->setPlainText(bip39::generate(12));
    else if (chosen == w24) target->setPlainText(bip39::generate(24));
}

bool ItemEditPanel::validate(QString& error) const {
    if (m_title->text().trimmed().isEmpty()) { error = "Please enter a title."; return false; }
    if (auto* te = qobject_cast<QPlainTextEdit*>(m_editors.value("seedPhrase"))) {
        const QString t = te->toPlainText();
        if (!t.trimmed().isEmpty()) {
            const bip39::Validation v = bip39::validate(t);
            if (!v.valid) {
                error = QStringLiteral("Seed phrase problem: %1 (saving anyway is not allowed to protect you from a typo — fix it or clear the field).").arg(v.message);
                return false;
            }
        }
    }
    if (auto* le = qobject_cast<QLineEdit*>(m_editors.value("totp"))) {
        if (!le->text().trimmed().isEmpty() && !totp::parse(le->text())) {
            error = "The TOTP secret is not valid base32 / otpauth URI.";
            return false;
        }
    }
    return true;
}

Item ItemEditPanel::currentItem() const {
    Item it = m_item;
    it.title = m_title->text().trimmed();
    it.folderId = m_folder->currentData().toString();
    for (auto e = m_editors.constBegin(); e != m_editors.constEnd(); ++e) {
        if (auto* le = qobject_cast<QLineEdit*>(e.value())) it.setField(e.key(), le->text());
        else if (auto* te = qobject_cast<QPlainTextEdit*>(e.value())) {
            QString v = te->toPlainText();
            if (e.key() == "seedPhrase") v = bip39::normalize(v);
            it.setField(e.key(), v);
        }
    }
    return it;
}

bool ItemEditPanel::isDirty() const {
    if (m_title->text().trimmed() != m_item.title) return true;
    const Item cur = currentItem();
    return cur.fields != m_item.fields || cur.folderId != m_item.folderId;
}

}  // namespace mp
