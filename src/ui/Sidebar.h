#pragma once
// Left navigation column: All Items / Favorites / Trash, Types, Folders and
// the "+ New Folder" action. Mirrors the reference design one-to-one.

#include <QVector>
#include <QWidget>

#include "core/Vault.h"
#include "ui/ListFilter.h"

class QVBoxLayout;
class QLabel;
class QPushButton;

namespace mp {

class SidebarRow;

class Sidebar : public QWidget {
    Q_OBJECT
public:
    explicit Sidebar(Vault* vault, QWidget* parent = nullptr);

    ListFilter currentFilter() const { return m_current; }
    void setFilter(const ListFilter& f);
    void refresh();  // counts + folder list

signals:
    void filterChanged(const ListFilter& filter);
    void newFolderRequested();
    void renameFolderRequested(const QString& folderId);
    void deleteFolderRequested(const QString& folderId);
    void emptyTrashRequested();
    void lockRequested();
    void settingsRequested();

protected:
    void paintEvent(QPaintEvent*) override;

private:
    SidebarRow* addRow(QVBoxLayout* into, const QString& icon, const QString& label,
                       const ListFilter& filter, const QColor& iconColor);
    void rebuildFolders();
    void updateSelection();

    Vault* m_vault;
    ListFilter m_current;
    QVector<SidebarRow*> m_rows;
    QVBoxLayout* m_folderLayout = nullptr;
    QVector<SidebarRow*> m_folderRows;
    SidebarRow* m_allRow = nullptr;
    SidebarRow* m_favRow = nullptr;
    SidebarRow* m_trashRow = nullptr;
    QLabel* m_foldersHeader = nullptr;
};

}  // namespace mp
