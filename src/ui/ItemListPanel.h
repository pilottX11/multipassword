#pragma once
// Middle column: search field, "+" button and the item list.

#include <QAbstractListModel>
#include <QWidget>

#include "core/Vault.h"
#include "ui/ListFilter.h"

class QLineEdit;
class QListView;
class QPushButton;
class QLabel;

namespace mp {

class ItemListModel : public QAbstractListModel {
    Q_OBJECT
public:
    explicit ItemListModel(QObject* parent = nullptr);
    void setItems(QVector<Item> items);
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    int rowOf(const QString& itemId) const;
    const Item* itemAt(int row) const;

private:
    QVector<Item> m_items;
};

class ItemListPanel : public QWidget {
    Q_OBJECT
public:
    explicit ItemListPanel(Vault* vault, QWidget* parent = nullptr);

    void setFilter(const ListFilter& f);
    void selectItem(const QString& id);
    QString selectedItemId() const;
    void focusSearch();
    void refresh();

signals:
    void itemSelected(const QString& itemId);   // empty when nothing selected
    void newItemRequested(ItemType type);

private:
    void rebuild();
    void showAddMenu();

    Vault* m_vault;
    ListFilter m_filter;
    QLineEdit* m_search = nullptr;
    QPushButton* m_add = nullptr;
    QListView* m_view = nullptr;
    ItemListModel* m_model = nullptr;
    QLabel* m_empty = nullptr;
    QString m_pendingSelect;
};

}  // namespace mp
