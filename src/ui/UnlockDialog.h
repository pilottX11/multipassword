#pragma once
// First screen: create a vault (new master password, confirmation, strength
// meter) or unlock an existing one. Failed unlock attempts are rate limited
// with an exponential delay.

#include <QDialog>

#include "core/Vault.h"

class QLineEdit;
class QLabel;
class QPushButton;
class QProgressBar;
class QTimer;

namespace mp {

class UnlockDialog : public QDialog {
    Q_OBJECT
public:
    explicit UnlockDialog(Vault* vault, const QString& vaultPath, QWidget* parent = nullptr);
    ~UnlockDialog() override;

private:
    void attempt();
    void updateStrength();
    void setBusy(bool busy);
    void chooseVaultFile();

    Vault* m_vault;
    QString m_path;
    bool m_creating = false;
    QLabel* m_heading = nullptr;
    QLabel* m_pathLabel = nullptr;
    QLineEdit* m_password = nullptr;
    QLineEdit* m_confirm = nullptr;
    QProgressBar* m_strengthBar = nullptr;
    QLabel* m_strengthLabel = nullptr;
    QLabel* m_error = nullptr;
    QPushButton* m_unlock = nullptr;
    QPushButton* m_browse = nullptr;
    QTimer* m_lockout = nullptr;
    int m_failures = 0;
};

}  // namespace mp
