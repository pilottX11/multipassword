#pragma once
// Compact popup shown on the autofill hotkey: lists matching logins for the
// foreground window (best match first), with a search box to pick any other
// item. Enter / double-click fills; Esc cancels.

#include <QDialog>
#include <QVector>

#include "autofill/AutofillEngine.h"
#include "core/Vault.h"

class QLineEdit;
class QListWidget;
class QLabel;

namespace mp {

class AutofillPicker : public QDialog {
    Q_OBJECT
public:
    AutofillPicker(Vault* vault, const ForegroundContext& ctx, QWidget* parent = nullptr);
    QString chosenItemId() const { return m_chosen; }

protected:
    void keyPressEvent(QKeyEvent* e) override;

private:
    void rebuild();
    void choose();

    Vault* m_vault;
    ForegroundContext m_ctx;
    QVector<AutofillMatch> m_matches;
    QLineEdit* m_search = nullptr;
    QListWidget* m_list = nullptr;
    QLabel* m_context = nullptr;
    QString m_chosen;
};

}  // namespace mp
