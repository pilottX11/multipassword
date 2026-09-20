#include "ui/UnlockDialog.h"

#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "core/PasswordStrength.h"
#include "core/SecureMemory.h"
#include "core/Settings.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

namespace mp {

UnlockDialog::UnlockDialog(Vault* vault, const QString& vaultPath, QWidget* parent)
    : QDialog(parent), m_vault(vault), m_path(vaultPath) {
    setWindowTitle("multipassword");
    setModal(true);
    setFixedWidth(420);
    m_creating = !QFile::exists(m_path);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(32, 32, 32, 28);
    lay->setSpacing(10);

    auto* logo = new QLabel(this);
    logo->setPixmap(icons::pixmap("shield", theme::kAccent, 44, devicePixelRatioF()));
    logo->setAlignment(Qt::AlignCenter);
    lay->addWidget(logo);

    m_heading = new QLabel(this);
    m_heading->setObjectName("Title");
    m_heading->setAlignment(Qt::AlignCenter);
    lay->addWidget(m_heading);

    auto* sub = new QLabel(m_creating ? "Choose a strong master password. It is the only key to your vault and cannot be recovered."
                                      : "Enter your master password to unlock.", this);
    sub->setObjectName("Hint");
    sub->setWordWrap(true);
    sub->setAlignment(Qt::AlignCenter);
    lay->addWidget(sub);
    lay->addSpacing(8);

    auto* pwRow = new QHBoxLayout();
    m_password = new QLineEdit(this);
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setPlaceholderText("Master password");
    pwRow->addWidget(m_password, 1);
    auto* reveal = new QToolButton(this);
    reveal->setIcon(icons::icon("eye", theme::kTextSecondary, 15));
    reveal->setCheckable(true);
    reveal->setCursor(Qt::PointingHandCursor);
    connect(reveal, &QToolButton::toggled, this, [this, reveal](bool on) {
        m_password->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
        if (m_confirm) m_confirm->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
        reveal->setIcon(icons::icon(on ? "eye-off" : "eye", theme::kTextSecondary, 15));
    });
    pwRow->addWidget(reveal);
    lay->addLayout(pwRow);

    if (m_creating) {
        m_confirm = new QLineEdit(this);
        m_confirm->setEchoMode(QLineEdit::Password);
        m_confirm->setPlaceholderText("Confirm master password");
        lay->addWidget(m_confirm);
        m_strengthBar = new QProgressBar(this);
        m_strengthBar->setRange(0, 100);
        m_strengthBar->setTextVisible(false);
        m_strengthBar->setFixedHeight(6);
        lay->addWidget(m_strengthBar);
        m_strengthLabel = new QLabel(this);
        m_strengthLabel->setObjectName("Hint");
        m_strengthLabel->setWordWrap(true);
        lay->addWidget(m_strengthLabel);
        connect(m_password, &QLineEdit::textChanged, this, &UnlockDialog::updateStrength);
        connect(m_confirm, &QLineEdit::returnPressed, this, &UnlockDialog::attempt);
    }
    connect(m_password, &QLineEdit::returnPressed, this, [this] {
        if (m_confirm && m_confirm->text().isEmpty()) m_confirm->setFocus();
        else attempt();
    });

    m_error = new QLabel(this);
    m_error->setObjectName("Error");
    m_error->setWordWrap(true);
    m_error->setAlignment(Qt::AlignCenter);
    m_error->hide();
    lay->addWidget(m_error);

    m_unlock = new QPushButton(m_creating ? "Create Vault" : "Unlock", this);
    m_unlock->setObjectName("Primary");
    m_unlock->setDefault(true);
    m_unlock->setMinimumHeight(36);
    m_unlock->setCursor(Qt::PointingHandCursor);
    connect(m_unlock, &QPushButton::clicked, this, &UnlockDialog::attempt);
    lay->addWidget(m_unlock);

    auto* pathRow = new QHBoxLayout();
    m_pathLabel = new QLabel(this);
    m_pathLabel->setObjectName("Muted");
    m_pathLabel->setWordWrap(true);
    pathRow->addWidget(m_pathLabel, 1);
    m_browse = new QPushButton("Open other…", this);
    m_browse->setObjectName("Flat");
    m_browse->setCursor(Qt::PointingHandCursor);
    connect(m_browse, &QPushButton::clicked, this, &UnlockDialog::chooseVaultFile);
    pathRow->addWidget(m_browse);
    lay->addLayout(pathRow);

    m_lockout = new QTimer(this);
    m_lockout->setSingleShot(true);
    connect(m_lockout, &QTimer::timeout, this, [this] { setBusy(false); m_error->hide(); });

    const QString owner = Settings::instance().ownerName();
    m_heading->setText(m_creating ? QStringLiteral("Create your vault")
                                  : (owner.isEmpty() ? QStringLiteral("Welcome back") : QStringLiteral("Welcome back, %1").arg(owner)));
    const QString hint = Settings::instance().passwordHint();
    if (!m_creating && !hint.isEmpty()) m_password->setToolTip(QStringLiteral("Hint: %1").arg(hint));
    if (!m_creating && !hint.isEmpty()) {
        auto* hintLabel = new QLabel(QStringLiteral("Hint: %1").arg(hint), this);
        hintLabel->setObjectName("Muted");
        hintLabel->setAlignment(Qt::AlignCenter);
        hintLabel->setWordWrap(true);
        static_cast<QVBoxLayout*>(layout())->insertWidget(static_cast<QVBoxLayout*>(layout())->indexOf(m_error), hintLabel);
    }
    m_pathLabel->setText(QFileInfo(m_path).fileName() + "  ·  " + QFileInfo(m_path).absolutePath());
    m_password->setFocus();
}

UnlockDialog::~UnlockDialog() {
    QString a = m_password->text();
    m_password->clear();
    wipe(a);
    if (m_confirm) { QString b = m_confirm->text(); m_confirm->clear(); wipe(b); }
}

void UnlockDialog::chooseVaultFile() {
    const QString f = QFileDialog::getOpenFileName(this, "Open vault", QFileInfo(m_path).absolutePath(),
                                                   "multipassword vault (*.mpv);;All files (*)");
    if (f.isEmpty()) return;
    Settings::instance().setVaultPath(f);
    m_path = f;
    // Rebuild as unlock dialog for the chosen file.
    m_creating = !QFile::exists(m_path);
    m_pathLabel->setText(QFileInfo(m_path).fileName() + "  ·  " + QFileInfo(m_path).absolutePath());
    m_heading->setText(m_creating ? "Create your vault" : "Welcome back");
    m_unlock->setText(m_creating ? "Create Vault" : "Unlock");
    if (!m_creating && m_confirm) { m_confirm->hide(); m_strengthBar->hide(); m_strengthLabel->hide(); }
}

void UnlockDialog::updateStrength() {
    if (!m_strengthBar) return;
    const StrengthResult s = estimateStrength(m_password->text());
    m_strengthBar->setValue(m_password->text().isEmpty() ? 0 : std::clamp(static_cast<int>(s.entropyBits), 4, 100));
    const QColor c = s.score >= 3 ? theme::kSuccess : (s.score == 2 ? theme::kStar : theme::kDanger);
    m_strengthBar->setStyleSheet(QStringLiteral("QProgressBar::chunk{background:%1;border-radius:3px;}").arg(c.name()));
    m_strengthLabel->setText(m_password->text().isEmpty() ? QString()
                                                          : QStringLiteral("%1 · ~%2 bits%3").arg(s.label).arg(static_cast<int>(s.entropyBits))
                                                                .arg(s.warning.isEmpty() ? QString() : "  ·  " + s.warning));
}

void UnlockDialog::setBusy(bool busy) {
    m_unlock->setEnabled(!busy);
    m_password->setEnabled(!busy);
    if (m_confirm) m_confirm->setEnabled(!busy);
    m_browse->setEnabled(!busy);
    if (busy) QApplication::setOverrideCursor(Qt::WaitCursor);
    else QApplication::restoreOverrideCursor();
}

void UnlockDialog::attempt() {
    m_error->hide();
    QString pw = m_password->text();
    if (pw.isEmpty()) { m_error->setText("Enter a master password."); m_error->show(); return; }
    if (m_creating) {
        if (pw != m_confirm->text()) { m_error->setText("Passwords do not match."); m_error->show(); return; }
        const StrengthResult s = estimateStrength(pw);
        if (s.score < 2) {
            m_error->setText("That master password is too weak. Use a long passphrase (e.g. five random words) — it protects everything.");
            m_error->show();
            return;
        }
    }
    setBusy(true);
    m_unlock->setText(m_creating ? "Creating…" : "Unlocking…");
    QApplication::processEvents();

    SecureBytes secret = SecureBytes::fromQString(pw);
    wipe(pw);
    VaultError err = m_creating ? m_vault->create(m_path, secret) : m_vault->open(m_path, secret);
    secret.clear();

    if (err == VaultError::None) {
        QString a = m_password->text(); m_password->clear(); wipe(a);
        if (m_confirm) { QString b = m_confirm->text(); m_confirm->clear(); wipe(b); }
        setBusy(false);
        accept();
        return;
    }
    m_unlock->setText(m_creating ? "Create Vault" : "Unlock");
    m_error->setText(vaultErrorMessage(err));
    m_error->show();
    if (err == VaultError::WrongPassword) {
        // Exponential lockout: 0s, 1s, 2s, 4s, ... capped at 30s
        ++m_failures;
        const int delayMs = std::min(30000, (m_failures >= 2 ? (1 << std::min(m_failures - 2, 5)) : 0) * 1000);
        if (delayMs > 0) {
            m_error->setText(QStringLiteral("Incorrect master password. Try again in %1 s.").arg(delayMs / 1000));
            m_lockout->start(delayMs);
            m_password->selectAll();
            return;  // stays busy until the timer fires
        }
    }
    setBusy(false);
    m_password->selectAll();
    m_password->setFocus();
}

}  // namespace mp
