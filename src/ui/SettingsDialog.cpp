#include "ui/SettingsDialog.h"

#include <QCheckBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "core/PasswordStrength.h"
#include "core/SecureMemory.h"
#include "core/Settings.h"
#include "ui/Theme.h"

namespace mp {

namespace {
QGroupBox* group(const QString& title, QWidget* parent) {
    auto* g = new QGroupBox(title, parent);
    g->setStyleSheet(QStringLiteral(
        "QGroupBox{border:1px solid %1;border-radius:8px;margin-top:10px;padding:8px 6px 4px 6px;font-weight:600;}"
        "QGroupBox::title{subcontrol-origin:margin;left:10px;padding:0 4px;color:%2;}")
                         .arg(theme::kSeparator.name(), theme::kTextSecondary.name()));
    return g;
}
}  // namespace

SettingsDialog::SettingsDialog(Vault* vault, QWidget* parent) : QDialog(parent), m_vault(vault) {
    setWindowTitle("Settings");
    setModal(true);
    setMinimumWidth(480);
    Settings& s = Settings::instance();

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(20, 16, 20, 16);
    lay->setSpacing(12);

    // --- Security ---------------------------------------------------------
    auto* sec = group("Security", this);
    auto* sf = new QFormLayout(sec);
    m_autoLock = new QSpinBox(sec);
    m_autoLock->setRange(0, 240);
    m_autoLock->setSuffix(" min");
    m_autoLock->setSpecialValueText("Never");
    m_autoLock->setValue(s.autoLockMinutes());
    sf->addRow("Auto-lock after inactivity", m_autoLock);
    m_lockOnMinimize = new QCheckBox("Lock when the window is minimized", sec);
    m_lockOnMinimize->setChecked(s.lockOnMinimize());
    sf->addRow(QString(), m_lockOnMinimize);
    m_lockOnScreenLock = new QCheckBox("Lock when the computer is locked or sleeps", sec);
    m_lockOnScreenLock->setChecked(s.lockOnScreenLock());
    sf->addRow(QString(), m_lockOnScreenLock);
    m_clipboard = new QSpinBox(sec);
    m_clipboard->setRange(0, 600);
    m_clipboard->setSuffix(" s");
    m_clipboard->setSpecialValueText("Never");
    m_clipboard->setValue(s.clipboardClearSeconds());
    sf->addRow("Clear clipboard after", m_clipboard);
    m_noCapture = new QCheckBox("Hide window from screenshots and screen sharing", sec);
    m_noCapture->setChecked(s.excludeFromScreenCapture());
    sf->addRow(QString(), m_noCapture);
    m_tray = new QCheckBox("Keep running in the system tray when closed", sec);
    m_tray->setChecked(s.minimizeToTray());
    sf->addRow(QString(), m_tray);
    auto* changePw = new QPushButton("Change master password…", sec);
    changePw->setCursor(Qt::PointingHandCursor);
    connect(changePw, &QPushButton::clicked, this, &SettingsDialog::changeMasterPassword);
    sf->addRow(QString(), changePw);
    auto* backup = new QPushButton("Export encrypted backup…", sec);
    backup->setCursor(Qt::PointingHandCursor);
    connect(backup, &QPushButton::clicked, this, &SettingsDialog::exportBackup);
    sf->addRow(QString(), backup);
    lay->addWidget(sec);

    // --- Autofill ----------------------------------------------------------
    auto* af = group("Autofill", this);
    auto* aff = new QFormLayout(af);
    m_autofill = new QCheckBox("Enable global autofill hotkey", af);
    m_autofill->setChecked(s.autofillEnabled());
    aff->addRow(QString(), m_autofill);
    m_hotkey = new QLineEdit(s.autofillHotkey(), af);
    m_hotkey->setPlaceholderText("Ctrl+Alt+A");
    aff->addRow("Hotkey", m_hotkey);
    m_sequence = new QLineEdit(s.autofillSequence(), af);
    aff->addRow("Type sequence", m_sequence);
    auto* seqHint = new QLabel("Placeholders: {USERNAME} {PASSWORD} {TAB} {ENTER} {SPACE} {DELAY 500} "
                               "or any field key such as {totp}, {cardholder}, {number}.", af);
    seqHint->setObjectName("Muted");
    seqHint->setWordWrap(true);
    aff->addRow(QString(), seqHint);
    lay->addWidget(af);

    m_status = new QLabel(this);
    m_status->setObjectName("Muted");
    m_status->setText(QStringLiteral("Vault: %1").arg(vault->path()));
    m_status->setWordWrap(true);
    lay->addWidget(m_status);

    auto* btns = new QHBoxLayout();
    btns->addStretch();
    auto* cancel = new QPushButton("Cancel", this);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    btns->addWidget(cancel);
    auto* save = new QPushButton("Save", this);
    save->setObjectName("Primary");
    save->setDefault(true);
    connect(save, &QPushButton::clicked, this, [this] {
        Settings& st = Settings::instance();
        st.setAutoLockMinutes(m_autoLock->value());
        st.setLockOnMinimize(m_lockOnMinimize->isChecked());
        st.setLockOnScreenLock(m_lockOnScreenLock->isChecked());
        st.setClipboardClearSeconds(m_clipboard->value());
        st.setExcludeFromScreenCapture(m_noCapture->isChecked());
        st.setMinimizeToTray(m_tray->isChecked());
        st.setAutofillEnabled(m_autofill->isChecked());
        st.setAutofillHotkey(m_hotkey->text().trimmed());
        st.setAutofillSequence(m_sequence->text().trimmed().isEmpty() ? QStringLiteral("{USERNAME}{TAB}{PASSWORD}{ENTER}")
                                                                      : m_sequence->text().trimmed());
        emit hotkeyChanged();
        accept();
    });
    btns->addWidget(save);
    lay->addLayout(btns);
}

void SettingsDialog::changeMasterPassword() {
    bool ok = false;
    QString cur = QInputDialog::getText(this, "Change master password", "Current master password:",
                                        QLineEdit::Password, QString(), &ok);
    if (!ok) { wipe(cur); return; }
    QString next = QInputDialog::getText(this, "Change master password", "New master password:",
                                         QLineEdit::Password, QString(), &ok);
    if (!ok) { wipe(cur); wipe(next); return; }
    QString confirm = QInputDialog::getText(this, "Change master password", "Confirm new master password:",
                                            QLineEdit::Password, QString(), &ok);
    if (!ok || confirm != next) {
        if (ok) QMessageBox::warning(this, "Change master password", "The new passwords do not match.");
        wipe(cur); wipe(next); wipe(confirm);
        return;
    }
    wipe(confirm);
    if (estimateStrength(next).score < 2) {
        QMessageBox::warning(this, "Change master password", "The new master password is too weak.");
        wipe(cur); wipe(next);
        return;
    }
    SecureBytes a = SecureBytes::fromQString(cur);
    SecureBytes b = SecureBytes::fromQString(next);
    wipe(cur); wipe(next);
    const VaultError err = m_vault->changeMasterPassword(a, b);
    if (err == VaultError::None)
        QMessageBox::information(this, "Change master password", "Master password changed and vault re-encrypted.");
    else
        QMessageBox::critical(this, "Change master password", vaultErrorMessage(err));
}

void SettingsDialog::exportBackup() {
    const QString target = QFileDialog::getSaveFileName(this, "Export encrypted backup", "multipassword-backup.mpv",
                                                        "multipassword vault (*.mpv)");
    if (target.isEmpty()) return;
    auto blob = m_vault->exportEncrypted();
    if (!blob) { QMessageBox::critical(this, "Export", "Vault is locked."); return; }
    QFile f(target);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
        f.write(reinterpret_cast<const char*>(blob->data()), static_cast<qint64>(blob->size())) != static_cast<qint64>(blob->size())) {
        QMessageBox::critical(this, "Export", "Could not write the backup file.");
        return;
    }
    f.close();
    QMessageBox::information(this, "Export", "Encrypted backup written. It is protected by your current master password.");
}

}  // namespace mp
