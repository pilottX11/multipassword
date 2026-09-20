#pragma once
// First-run "sign up" page: choose a name and a master password, see live
// strength + requirement checklist, confirm, optionally leave a hint, and
// acknowledge that the password cannot be recovered. On success the vault
// is created and the dialog accepts. Everything stays on this device — there
// is no server account.

#include <QDialog>

#include "core/Vault.h"

class QLineEdit;
class QLabel;
class QPushButton;
class QProgressBar;
class QCheckBox;
class QVBoxLayout;

namespace mp {

class SignupDialog : public QDialog {
    Q_OBJECT
public:
    SignupDialog(Vault* vault, const QString& vaultPath, QWidget* parent = nullptr);
    ~SignupDialog() override;

    // Set when the user chose "open an existing vault" instead.
    QString existingVaultChosen() const { return m_existingVault; }

private:
    void refresh();
    void createVault();
    void openExisting();
    struct Req { QLabel* icon; QLabel* text; };
    Req addRequirement(QVBoxLayout* into, const QString& text);
    void setReq(const Req& r, bool ok);

    Vault* m_vault;
    QString m_path;
    QString m_existingVault;
    QLineEdit* m_name = nullptr;
    QLineEdit* m_password = nullptr;
    QLineEdit* m_confirm = nullptr;
    QLineEdit* m_hint = nullptr;
    QProgressBar* m_bar = nullptr;
    QLabel* m_strength = nullptr;
    QCheckBox* m_ack = nullptr;
    QPushButton* m_create = nullptr;
    QLabel* m_error = nullptr;
    Req m_reqLength{}, m_reqCase{}, m_reqDigit{}, m_reqSymbol{}, m_reqMatch{}, m_reqNotHint{};
};

}  // namespace mp
