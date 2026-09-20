#pragma once
// Right column, edit mode: form for creating/updating an item.

#include <QMap>
#include <QWidget>

#include "core/Item.h"
#include "core/Vault.h"

class QLineEdit;
class QComboBox;
class QLabel;
class QPushButton;
class QPlainTextEdit;
class QVBoxLayout;
class QProgressBar;

namespace mp {

class IconBadge;

class ItemEditPanel : public QWidget {
    Q_OBJECT
public:
    explicit ItemEditPanel(Vault* vault, QWidget* parent = nullptr);
    ~ItemEditPanel() override;

    void beginNew(ItemType type);
    void beginEdit(const Item& item);
    Item currentItem() const;  // assembled from the form
    bool isDirty() const;

signals:
    void saved(const Item& item, bool isNew);
    void cancelled();

private:
    void buildForm();
    void wipeForm();
    bool validate(QString& error) const;
    void updateStrength();
    void updateSeedStatus();
    void generatePasswordInto(QLineEdit* target);
    void generateSeedInto(QPlainTextEdit* target);

    Vault* m_vault;
    Item m_item;
    bool m_isNew = false;
    IconBadge* m_badge = nullptr;
    QLabel* m_heading = nullptr;
    QLineEdit* m_title = nullptr;
    QComboBox* m_folder = nullptr;
    QVBoxLayout* m_form = nullptr;
    QMap<QString, QWidget*> m_editors;   // field key -> QLineEdit / QPlainTextEdit
    QLabel* m_strengthLabel = nullptr;
    QProgressBar* m_strengthBar = nullptr;
    QLabel* m_seedStatus = nullptr;
    QLabel* m_error = nullptr;
    QPushButton* m_save = nullptr;
    QPushButton* m_cancel = nullptr;
};

}  // namespace mp
