#pragma once
#include <QDialog>

#include "core/Vault.h"

class QSpinBox;
class QCheckBox;
class QLineEdit;
class QLabel;

namespace mp {

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(Vault* vault, QWidget* parent = nullptr);

signals:
    void hotkeyChanged();

private:
    void changeMasterPassword();
    void exportBackup();

    Vault* m_vault;
    QSpinBox* m_autoLock = nullptr;
    QCheckBox* m_lockOnMinimize = nullptr;
    QCheckBox* m_lockOnScreenLock = nullptr;
    QSpinBox* m_clipboard = nullptr;
    QCheckBox* m_autofill = nullptr;
    QLineEdit* m_hotkey = nullptr;
    QLineEdit* m_sequence = nullptr;
    QCheckBox* m_noCapture = nullptr;
    QCheckBox* m_tray = nullptr;
    QLabel* m_status = nullptr;
};

}  // namespace mp
