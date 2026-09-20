#include "ui/PasswordGeneratorDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "core/PasswordGenerator.h"
#include "core/PasswordStrength.h"
#include "core/SecureMemory.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

namespace mp {

PasswordGeneratorDialog::PasswordGeneratorDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Password Generator");
    setModal(true);
    setMinimumWidth(440);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(20, 20, 20, 20);
    lay->setSpacing(12);

    auto* outRow = new QHBoxLayout();
    m_output = new QLineEdit(this);
    m_output->setReadOnly(true);
    QFont mono("Consolas");
    mono.setStyleHint(QFont::Monospace);
    mono.setPixelSize(14);
    m_output->setFont(mono);
    outRow->addWidget(m_output, 1);
    auto* regen = new QToolButton(this);
    regen->setIcon(icons::icon("refresh", theme::kText, 16));
    regen->setToolTip("Regenerate");
    regen->setCursor(Qt::PointingHandCursor);
    connect(regen, &QToolButton::clicked, this, &PasswordGeneratorDialog::regenerate);
    outRow->addWidget(regen);
    lay->addLayout(outRow);

    m_strength = new QLabel(this);
    m_strength->setObjectName("Hint");
    lay->addWidget(m_strength);

    m_tabs = new QTabWidget(this);

    // --- Password tab ------------------------------------------------------
    auto* pw = new QWidget(m_tabs);
    auto* pf = new QFormLayout(pw);
    pf->setContentsMargins(14, 14, 14, 14);
    auto* lenRow = new QHBoxLayout();
    m_length = new QSlider(Qt::Horizontal, pw);
    m_length->setRange(8, 64);
    m_length->setValue(20);
    m_lengthSpin = new QSpinBox(pw);
    m_lengthSpin->setRange(8, 128);
    m_lengthSpin->setValue(20);
    m_lengthSpin->setFixedWidth(64);
    connect(m_length, &QSlider::valueChanged, m_lengthSpin, &QSpinBox::setValue);
    connect(m_lengthSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        if (v <= m_length->maximum()) m_length->setValue(v);
        regenerate();
    });
    lenRow->addWidget(m_length, 1);
    lenRow->addWidget(m_lengthSpin);
    pf->addRow("Length", lenRow);
    m_lower = new QCheckBox("Lowercase (a-z)", pw); m_lower->setChecked(true);
    m_upper = new QCheckBox("Uppercase (A-Z)", pw); m_upper->setChecked(true);
    m_digits = new QCheckBox("Digits (0-9)", pw); m_digits->setChecked(true);
    m_symbols = new QCheckBox("Symbols (!@#$…)", pw); m_symbols->setChecked(true);
    m_ambiguous = new QCheckBox("Exclude ambiguous (l 1 I O 0)", pw);
    for (QCheckBox* c : {m_lower, m_upper, m_digits, m_symbols, m_ambiguous}) {
        pf->addRow(QString(), c);
        connect(c, &QCheckBox::toggled, this, &PasswordGeneratorDialog::regenerate);
    }
    m_tabs->addTab(pw, "Password");

    // --- Passphrase tab ----------------------------------------------------
    auto* pp = new QWidget(m_tabs);
    auto* ppf = new QFormLayout(pp);
    ppf->setContentsMargins(14, 14, 14, 14);
    m_words = new QSpinBox(pp);
    m_words->setRange(3, 12);
    m_words->setValue(6);
    connect(m_words, qOverload<int>(&QSpinBox::valueChanged), this, &PasswordGeneratorDialog::regenerate);
    ppf->addRow("Words", m_words);
    m_separator = new QComboBox(pp);
    m_separator->addItem("Hyphen (-)", "-");
    m_separator->addItem("Space", " ");
    m_separator->addItem("Period (.)", ".");
    m_separator->addItem("Underscore (_)", "_");
    m_separator->addItem("None", "");
    connect(m_separator, qOverload<int>(&QComboBox::currentIndexChanged), this, &PasswordGeneratorDialog::regenerate);
    ppf->addRow("Separator", m_separator);
    m_capitalize = new QCheckBox("Capitalize words", pp);
    m_digitSuffix = new QCheckBox("Append a digit", pp);
    ppf->addRow(QString(), m_capitalize);
    ppf->addRow(QString(), m_digitSuffix);
    connect(m_capitalize, &QCheckBox::toggled, this, &PasswordGeneratorDialog::regenerate);
    connect(m_digitSuffix, &QCheckBox::toggled, this, &PasswordGeneratorDialog::regenerate);
    auto* hint = new QLabel("Each word adds 11 bits of entropy (2048-word list).", pp);
    hint->setObjectName("Muted");
    ppf->addRow(QString(), hint);
    m_tabs->addTab(pp, "Passphrase");
    connect(m_tabs, &QTabWidget::currentChanged, this, &PasswordGeneratorDialog::regenerate);
    lay->addWidget(m_tabs);

    auto* btns = new QHBoxLayout();
    btns->addStretch();
    auto* cancel = new QPushButton("Cancel", this);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    btns->addWidget(cancel);
    auto* use = new QPushButton("Use Password", this);
    use->setObjectName("Primary");
    use->setDefault(true);
    connect(use, &QPushButton::clicked, this, &QDialog::accept);
    btns->addWidget(use);
    lay->addLayout(btns);

    regenerate();
}

PasswordGeneratorDialog::~PasswordGeneratorDialog() {
    QString t = m_output->text();
    m_output->clear();
    wipe(t);
}

QString PasswordGeneratorDialog::result() const { return m_output->text(); }

void PasswordGeneratorDialog::regenerate() {
    QString pw;
    if (m_tabs->currentIndex() == 0) {
        PasswordOptions o;
        o.length = m_lengthSpin->value();
        o.lowercase = m_lower->isChecked();
        o.uppercase = m_upper->isChecked();
        o.digits = m_digits->isChecked();
        o.symbols = m_symbols->isChecked();
        o.excludeAmbiguous = m_ambiguous->isChecked();
        pw = generatePassword(o);
    } else {
        pw = generatePassphrase(m_words->value(), m_separator->currentData().toString(),
                                m_capitalize->isChecked(), m_digitSuffix->isChecked());
    }
    QString old = m_output->text();
    m_output->setText(pw);
    wipe(old);
    const StrengthResult s = estimateStrength(pw);
    const QColor c = s.score >= 3 ? theme::kSuccess : (s.score == 2 ? theme::kStar : theme::kDanger);
    m_strength->setText(QStringLiteral("<span style='color:%1'>●</span> %2 · ~%3 bits of entropy")
                            .arg(c.name(), s.label).arg(static_cast<int>(s.entropyBits)));
}

}  // namespace mp
