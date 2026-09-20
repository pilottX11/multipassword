#include "ui/FieldRow.h"

#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "core/Bip39.h"
#include "core/SecureMemory.h"
#include "core/Totp.h"
#include "ui/ClipboardManager.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

namespace mp {

namespace {
bool isSecretKind(FieldKind k) {
    return k == FieldKind::Secret || k == FieldKind::SeedPhrase || k == FieldKind::Totp;
}
}  // namespace

FieldRow::FieldRow(const FieldDef& def, const QString& value, ClipboardManager* clipboard, QWidget* parent)
    : QWidget(parent), m_def(def), m_value(value), m_clipboard(clipboard) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 10, 0, 10);
    outer->setSpacing(4);

    auto* label = new QLabel(def.label, this);
    label->setObjectName("FieldLabel");
    outer->addWidget(label);

    auto* row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(6);

    m_valueLabel = new QLabel(this);
    m_valueLabel->setObjectName(isSecretKind(def.kind) || def.kind == FieldKind::Totp ? "FieldValueMono" : "FieldValue");
    m_valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_valueLabel->setWordWrap(def.kind == FieldKind::Multiline);
    m_valueLabel->setCursor(Qt::IBeamCursor);
    row->addWidget(m_valueLabel, 1);

    if (def.kind == FieldKind::SeedPhrase) {
        m_seedGrid = new QWidget(this);
        m_seedLayout = new QGridLayout(m_seedGrid);
        m_seedLayout->setContentsMargins(0, 0, 0, 0);
        m_seedLayout->setHorizontalSpacing(8);
        m_seedLayout->setVerticalSpacing(6);
        m_seedGrid->hide();
    }

    if (def.kind == FieldKind::Totp) {
        m_totpTimer = new QLabel(this);
        m_totpTimer->setObjectName("Muted");
        m_totpTimer->setFixedWidth(28);
        m_totpTimer->setAlignment(Qt::AlignCenter);
        row->addWidget(m_totpTimer);
        m_timer = new QTimer(this);
        m_timer->setInterval(1000);
        connect(m_timer, &QTimer::timeout, this, &FieldRow::updateTotp);
        m_timer->start();
    }

    auto mkBtn = [this](const QString& icon, const QString& tip) {
        auto* b = new QToolButton(this);
        b->setIcon(icons::icon(icon, theme::kTextSecondary, 15));
        b->setIconSize(QSize(15, 15));
        b->setToolTip(tip);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedSize(26, 26);
        return b;
    };

    if (isSecretKind(def.kind) && def.kind != FieldKind::Totp) {
        m_revealBtn = mkBtn("eye", "Reveal");
        connect(m_revealBtn, &QToolButton::clicked, this, [this] { setRevealed(!m_revealed); });
        row->addWidget(m_revealBtn);
    }
    if (def.kind == FieldKind::Url && !value.trimmed().isEmpty()) {
        m_openBtn = mkBtn("external-link", "Open website");
        connect(m_openBtn, &QToolButton::clicked, this, [this] { emit openUrlRequested(m_value); });
        row->addWidget(m_openBtn);
    }
    m_copyBtn = mkBtn("copy", "Copy");
    connect(m_copyBtn, &QToolButton::clicked, this, &FieldRow::copyValue);
    row->addWidget(m_copyBtn);

    outer->addLayout(row);
    if (m_seedGrid) outer->addWidget(m_seedGrid);

    rebuildValue();
}

FieldRow::~FieldRow() {
    wipe(m_value);
    wipe(m_totpCode);
}

QString FieldRow::maskedText() const {
    if (m_def.kind == FieldKind::SeedPhrase) {
        const int n = bip39::splitWords(m_value).size();
        return QStringLiteral("•••• •••• ••••  (%1 words)").arg(n);
    }
    const int n = std::clamp(static_cast<int>(m_value.size()), 8, 16);
    return QString(n, QChar(0x2022));
}

void FieldRow::setRevealed(bool revealed) {
    m_revealed = revealed;
    if (m_revealBtn) {
        m_revealBtn->setIcon(icons::icon(revealed ? "eye-off" : "eye", theme::kTextSecondary, 15));
        m_revealBtn->setToolTip(revealed ? "Hide" : "Reveal");
    }
    rebuildValue();
}

void FieldRow::rebuildValue() {
    if (m_def.kind == FieldKind::Totp) {
        updateTotp();
        return;
    }
    if (m_value.trimmed().isEmpty()) {
        m_valueLabel->setText(QStringLiteral("—"));
        m_valueLabel->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kTextMuted.name()));
        if (m_copyBtn) m_copyBtn->setEnabled(false);
        return;
    }
    m_valueLabel->setStyleSheet({});
    if (!isSecretKind(m_def.kind)) {
        m_valueLabel->setText(m_value);
        return;
    }
    if (m_def.kind == FieldKind::SeedPhrase) {
        if (m_revealed) {
            // word grid: "1. abandon  2. ability ..."
            while (QLayoutItem* it = m_seedLayout->takeAt(0)) { delete it->widget(); delete it; }
            const QStringList words = bip39::splitWords(m_value);
            const int cols = words.size() > 12 ? 4 : 3;
            for (int i = 0; i < words.size(); ++i) {
                auto* w = new QLabel(QStringLiteral("<span style='color:%1'>%2.</span> %3")
                                         .arg(theme::kTextMuted.name()).arg(i + 1).arg(words[i]), m_seedGrid);
                w->setObjectName("FieldValueMono");
                w->setStyleSheet(QStringLiteral("background:%1;border-radius:6px;padding:4px 8px;").arg(theme::kInputBg.name()));
                m_seedLayout->addWidget(w, i / cols, i % cols);
            }
            const bip39::Validation v = bip39::validate(m_value);
            auto* status = new QLabel(v.message, m_seedGrid);
            status->setObjectName(v.valid ? "Success" : "Error");
            m_seedLayout->addWidget(status, (words.size() + cols - 1) / cols, 0, 1, cols);
            m_valueLabel->clear();  // keep the (empty) label so buttons stay right-aligned
            m_seedGrid->show();
        } else {
            m_seedGrid->hide();
            m_valueLabel->setText(maskedText());
        }
        return;
    }
    m_valueLabel->setText(m_revealed ? m_value : maskedText());
}

void FieldRow::updateTotp() {
    auto params = totp::parse(m_value);
    if (!params) {
        m_valueLabel->setText(m_value.trimmed().isEmpty() ? QStringLiteral("—") : QStringLiteral("Invalid TOTP secret"));
        if (m_totpTimer) m_totpTimer->clear();
        if (m_copyBtn) m_copyBtn->setEnabled(false);
        return;
    }
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    wipe(m_totpCode);
    m_totpCode = totp::code(*params, now);
    QString pretty = m_totpCode;
    if (pretty.size() == 6) pretty.insert(3, ' ');
    m_valueLabel->setText(pretty);
    const int left = totp::secondsRemaining(*params, now);
    m_totpTimer->setText(QString::number(left));
    m_totpTimer->setStyleSheet(left <= 5 ? QStringLiteral("color:%1;").arg(theme::kDanger.name()) : QString());
}

void FieldRow::copyValue() {
    if (m_def.kind == FieldKind::Totp) {
        if (!m_totpCode.isEmpty()) m_clipboard->copy(m_totpCode, true, "One-time code");
        return;
    }
    m_clipboard->copy(m_value, isSecretKind(m_def.kind), m_def.label);
}

}  // namespace mp
