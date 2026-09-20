#include "ui/SignupDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

#include "core/PasswordStrength.h"
#include "core/SecureMemory.h"
#include "core/Settings.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

namespace mp {

namespace {
QLineEdit* passwordField(QWidget* parent, const QString& placeholder, QHBoxLayout*& rowOut) {
    auto* le = new QLineEdit(parent);
    le->setEchoMode(QLineEdit::Password);
    le->setPlaceholderText(placeholder);
    le->setMinimumHeight(36);
    auto* reveal = new QToolButton(parent);
    reveal->setIcon(icons::icon("eye", theme::kTextSecondary, 15));
    reveal->setCheckable(true);
    reveal->setCursor(Qt::PointingHandCursor);
    reveal->setToolTip("Show");
    reveal->setFocusPolicy(Qt::NoFocus);
    QObject::connect(reveal, &QToolButton::toggled, le, [le, reveal](bool on) {
        le->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
        reveal->setIcon(icons::icon(on ? "eye-off" : "eye", theme::kTextSecondary, 15));
    });
    rowOut = new QHBoxLayout();
    rowOut->setSpacing(6);
    rowOut->addWidget(le, 1);
    rowOut->addWidget(reveal);
    return le;
}
}  // namespace

SignupDialog::SignupDialog(Vault* vault, const QString& vaultPath, QWidget* parent)
    : QDialog(parent), m_vault(vault), m_path(vaultPath) {
    setWindowTitle("multipassword — Sign up");
    setModal(true);
    setFixedWidth(880);
    setMinimumHeight(600);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---- left: brand / pitch -------------------------------------------------
    auto* left = new QWidget(this);
    left->setFixedWidth(340);
    left->setAutoFillBackground(true);
    left->setStyleSheet(QStringLiteral("background:%1;").arg(theme::kSidebarBg.name()));
    auto* ll = new QVBoxLayout(left);
    ll->setContentsMargins(36, 40, 36, 36);
    ll->setSpacing(14);
    auto* logo = new QLabel(left);
    logo->setPixmap(icons::pixmap("shield", theme::kAccent, 56, devicePixelRatioF()));
    ll->addWidget(logo);
    auto* brand = new QLabel("multipassword", left);
    brand->setObjectName("Title");
    ll->addWidget(brand);
    auto* pitch = new QLabel("One master password.\nEverything else encrypted on this device.", left);
    pitch->setObjectName("Hint");
    pitch->setWordWrap(true);
    ll->addWidget(pitch);
    ll->addSpacing(18);
    const QStringList points = {
        "Argon2id + XChaCha20-Poly1305 encryption",
        "Logins, cards, identities, secure notes",
        "Crypto wallet seed phrases (BIP-39 verified)",
        "Autofill with a global hotkey",
        "No account, no server, no telemetry",
    };
    for (const QString& p : points) {
        auto* row = new QHBoxLayout();
        row->setSpacing(10);
        auto* ic = new QLabel(left);
        ic->setPixmap(icons::pixmap("check", theme::kSuccess, 14, devicePixelRatioF()));
        ic->setFixedSize(14, 14);
        row->addWidget(ic, 0, Qt::AlignTop);
        auto* t = new QLabel(p, left);
        t->setObjectName("Hint");
        t->setWordWrap(true);
        row->addWidget(t, 1);
        ll->addLayout(row);
    }
    ll->addStretch();
    auto* foot = new QLabel("Your master password is never stored or sent anywhere.\nIf you forget it, your vault cannot be recovered.", left);
    foot->setObjectName("Muted");
    foot->setWordWrap(true);
    ll->addWidget(foot);
    root->addWidget(left);

    // ---- right: form ----------------------------------------------------------
    auto* right = new QWidget(this);
    auto* rl = new QVBoxLayout(right);
    rl->setContentsMargins(44, 40, 44, 32);
    rl->setSpacing(10);

    auto* heading = new QLabel("Create your account", right);
    heading->setObjectName("Title");
    rl->addWidget(heading);
    auto* sub = new QLabel("Pick a name and a strong master password to set up your encrypted vault.", right);
    sub->setObjectName("Hint");
    sub->setWordWrap(true);
    rl->addWidget(sub);
    rl->addSpacing(10);

    auto label = [right](const QString& t) {
        auto* l = new QLabel(t, right);
        l->setObjectName("FieldLabel");
        return l;
    };

    rl->addWidget(label("YOUR NAME"));
    m_name = new QLineEdit(right);
    m_name->setPlaceholderText("How should we greet you?");
    m_name->setMinimumHeight(36);
    m_name->setMaxLength(60);
    rl->addWidget(m_name);

    rl->addWidget(label("MASTER PASSWORD"));
    QHBoxLayout* pwRow = nullptr;
    m_password = passwordField(right, "At least 12 characters — a long passphrase works best", pwRow);
    rl->addLayout(pwRow);
    m_bar = new QProgressBar(right);
    m_bar->setRange(0, 100);
    m_bar->setTextVisible(false);
    m_bar->setFixedHeight(6);
    rl->addWidget(m_bar);
    m_strength = new QLabel(right);
    m_strength->setObjectName("Hint");
    rl->addWidget(m_strength);

    rl->addWidget(label("CONFIRM MASTER PASSWORD"));
    QHBoxLayout* cfRow = nullptr;
    m_confirm = passwordField(right, "Type it again", cfRow);
    rl->addLayout(cfRow);

    // requirement checklist (two columns)
    auto* reqHost = new QWidget(right);
    auto* reqCols = new QHBoxLayout(reqHost);
    reqCols->setContentsMargins(0, 6, 0, 2);
    reqCols->setSpacing(24);
    auto* colA = new QVBoxLayout();
    auto* colB = new QVBoxLayout();
    colA->setSpacing(4);
    colB->setSpacing(4);
    m_reqLength = addRequirement(colA, "At least 12 characters");
    m_reqCase = addRequirement(colA, "Upper and lower case letters");
    m_reqDigit = addRequirement(colA, "At least one number");
    m_reqSymbol = addRequirement(colB, "At least one symbol");
    m_reqMatch = addRequirement(colB, "Passwords match");
    m_reqNotHint = addRequirement(colB, "Hint does not contain the password");
    reqCols->addLayout(colA, 1);
    reqCols->addLayout(colB, 1);
    rl->addWidget(reqHost);

    rl->addWidget(label("PASSWORD HINT  (optional)"));
    m_hint = new QLineEdit(right);
    m_hint->setPlaceholderText("Shown on the unlock screen. Stored unencrypted — never put the password itself here.");
    m_hint->setMinimumHeight(36);
    m_hint->setMaxLength(120);
    rl->addWidget(m_hint);

    m_ack = new QCheckBox("I understand my master password cannot be reset or recovered.", right);
    rl->addWidget(m_ack);

    m_error = new QLabel(right);
    m_error->setObjectName("Error");
    m_error->setWordWrap(true);
    m_error->hide();
    rl->addWidget(m_error);
    rl->addStretch();

    m_create = new QPushButton("Create my vault", right);
    m_create->setObjectName("Primary");
    m_create->setMinimumHeight(40);
    m_create->setCursor(Qt::PointingHandCursor);
    m_create->setDefault(true);
    connect(m_create, &QPushButton::clicked, this, &SignupDialog::createVault);
    rl->addWidget(m_create);

    auto* alt = new QHBoxLayout();
    auto* altText = new QLabel("Already have a vault file?", right);
    altText->setObjectName("Muted");
    alt->addWidget(altText);
    auto* open = new QPushButton("Open it instead", right);
    open->setObjectName("Flat");
    open->setStyleSheet(QStringLiteral("color:%1;").arg(theme::kAccent.name()));
    open->setCursor(Qt::PointingHandCursor);
    connect(open, &QPushButton::clicked, this, &SignupDialog::openExisting);
    alt->addWidget(open);
    alt->addStretch();
    auto* where = new QLabel(QStringLiteral("Vault: %1").arg(QFileInfo(m_path).absoluteFilePath()), right);
    where->setObjectName("Muted");
    alt->addWidget(where);
    rl->addLayout(alt);

    root->addWidget(right, 1);

    for (QLineEdit* le : {m_password, m_confirm, m_hint}) connect(le, &QLineEdit::textChanged, this, &SignupDialog::refresh);
    connect(m_ack, &QCheckBox::toggled, this, &SignupDialog::refresh);
    connect(m_name, &QLineEdit::returnPressed, m_password, qOverload<>(&QWidget::setFocus));
    connect(m_password, &QLineEdit::returnPressed, m_confirm, qOverload<>(&QWidget::setFocus));
    connect(m_confirm, &QLineEdit::returnPressed, this, [this] { if (m_create->isEnabled()) createVault(); });

    refresh();
    m_name->setFocus();
}

SignupDialog::~SignupDialog() {
    for (QLineEdit* le : {m_password, m_confirm}) { QString t = le->text(); le->clear(); wipe(t); }
}

SignupDialog::Req SignupDialog::addRequirement(QVBoxLayout* into, const QString& text) {
    auto* row = new QHBoxLayout();
    row->setSpacing(8);
    Req r;
    r.icon = new QLabel(this);
    r.icon->setFixedSize(14, 14);
    r.text = new QLabel(text, this);
    r.text->setObjectName("Hint");
    row->addWidget(r.icon);
    row->addWidget(r.text, 1);
    into->addLayout(row);
    setReq(r, false);
    return r;
}

void SignupDialog::setReq(const Req& r, bool ok) {
    r.icon->setPixmap(icons::pixmap(ok ? "check" : "x", ok ? theme::kSuccess : theme::kTextMuted, 14, devicePixelRatioF()));
    r.text->setStyleSheet(QStringLiteral("color:%1;").arg((ok ? theme::kText : theme::kTextSecondary).name()));
}

void SignupDialog::refresh() {
    const QString pw = m_password->text();
    bool lower = false, upper = false, digit = false, symbol = false;
    for (QChar c : pw) {
        if (c.isLower()) lower = true;
        else if (c.isUpper()) upper = true;
        else if (c.isDigit()) digit = true;
        else symbol = true;
    }
    const bool okLen = pw.size() >= 12;
    const bool okCase = lower && upper;
    const bool okMatch = !pw.isEmpty() && pw == m_confirm->text();
    const QString hint = m_hint->text().trimmed().toLower();
    const bool okHint = hint.isEmpty() || pw.isEmpty() ||
                        (!hint.contains(pw.toLower()) && !pw.toLower().contains(hint));
    setReq(m_reqLength, okLen);
    setReq(m_reqCase, okCase);
    setReq(m_reqDigit, digit);
    setReq(m_reqSymbol, symbol);
    setReq(m_reqMatch, okMatch);
    setReq(m_reqNotHint, okHint);

    const StrengthResult s = estimateStrength(pw);
    m_bar->setValue(pw.isEmpty() ? 0 : std::clamp(static_cast<int>(s.entropyBits), 4, 100));
    const QColor c = s.score >= 3 ? theme::kSuccess : (s.score == 2 ? theme::kStar : theme::kDanger);
    m_bar->setStyleSheet(QStringLiteral("QProgressBar::chunk{background:%1;border-radius:3px;}").arg(c.name()));
    m_strength->setText(pw.isEmpty() ? QStringLiteral("Tip: four or five random words are easier to remember than 12 mixed characters.")
                                     : QStringLiteral("<span style='color:%1'>●</span> %2 · ~%3 bits of entropy%4")
                                           .arg(c.name(), s.label).arg(static_cast<int>(s.entropyBits))
                                           .arg(s.warning.isEmpty() ? QString() : "  ·  " + s.warning));

    // A long passphrase may lack a symbol/digit yet be far stronger than a
    // short "complex" password: accept either the checklist or >= 60 bits.
    const bool strongEnough = (okLen && okCase && digit && symbol) || (okLen && s.entropyBits >= 60);
    m_create->setEnabled(strongEnough && okMatch && okHint && m_ack->isChecked());
    m_error->hide();
}

void SignupDialog::createVault() {
    if (!m_create->isEnabled()) return;
    m_create->setEnabled(false);
    m_create->setText("Encrypting your vault…");
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents();

    QString pw = m_password->text();
    SecureBytes secret = SecureBytes::fromQString(pw);
    wipe(pw);
    const VaultError err = m_vault->create(m_path, secret);
    secret.clear();
    QApplication::restoreOverrideCursor();

    if (err != VaultError::None) {
        m_error->setText(vaultErrorMessage(err));
        m_error->show();
        m_create->setText("Create my vault");
        m_create->setEnabled(true);
        return;
    }
    Settings::instance().setOwnerName(m_name->text());
    Settings::instance().setPasswordHint(m_hint->text());
    for (QLineEdit* le : {m_password, m_confirm}) { QString t = le->text(); le->clear(); wipe(t); }
    accept();
}

void SignupDialog::openExisting() {
    const QString f = QFileDialog::getOpenFileName(this, "Open vault", QFileInfo(m_path).absolutePath(),
                                                   "multipassword vault (*.mpv);;All files (*)");
    if (f.isEmpty()) return;
    m_existingVault = f;
    Settings::instance().setVaultPath(f);
    reject();  // caller sees existingVaultChosen() and shows the unlock screen
}

}  // namespace mp
