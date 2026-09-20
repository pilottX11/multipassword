#pragma once
// Right column, read mode: big badge, title, type, favourite star, Edit /
// Delete buttons top-right, then the field rows.

#include <QWidget>

#include "core/Item.h"

class QLabel;
class QPushButton;
class QToolButton;
class QVBoxLayout;
class QScrollArea;

namespace mp {

class IconBadge;
class ClipboardManager;

class ItemDetailPanel : public QWidget {
    Q_OBJECT
public:
    explicit ItemDetailPanel(ClipboardManager* clipboard, QWidget* parent = nullptr);

    void setItem(const Item* item, const QString& folderName);  // nullptr => empty state
    void clear();

signals:
    void editRequested(const QString& itemId);
    void deleteRequested(const QString& itemId);
    void restoreRequested(const QString& itemId);
    void favoriteToggled(const QString& itemId, bool favorite);
    void autofillRequested(const QString& itemId);
    void openUrlRequested(const QString& url);

private:
    void rebuildFields(const Item& item, const QString& folderName);

    ClipboardManager* m_clipboard;
    QWidget* m_empty = nullptr;
    QWidget* m_content = nullptr;
    QScrollArea* m_scroll = nullptr;
    IconBadge* m_badge = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_subtitle = nullptr;
    QToolButton* m_star = nullptr;
    QPushButton* m_edit = nullptr;
    QPushButton* m_delete = nullptr;
    QPushButton* m_restore = nullptr;
    QPushButton* m_autofill = nullptr;
    QVBoxLayout* m_fields = nullptr;
    QLabel* m_meta = nullptr;
    QString m_itemId;
    bool m_favorite = false;
};

}  // namespace mp
