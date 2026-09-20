#pragma once
#include <QMainWindow>
#include <QPointer>

#include "autofill/AutoType.h"
#include "core/Vault.h"
#include "ui/ListFilter.h"

class QStackedWidget;
class QTimer;
class QLabel;
class QSystemTrayIcon;

namespace mp {

class Sidebar;
class ItemListPanel;
class ItemDetailPanel;
class ItemEditPanel;
class ClipboardManager;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(Vault* vault, QWidget* parent = nullptr);
    ~MainWindow() override;

    // Shows the unlock dialog; returns false if the user gave up (=> quit).
    bool unlockInteractive();

protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void closeEvent(QCloseEvent* e) override;
    void changeEvent(QEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* e) override;
    void showEvent(QShowEvent* e) override;

private:
    // wiring
    void onFilterChanged(const ListFilter& f);
    void onItemSelected(const QString& id);
    void onNewItem(ItemType type);
    void onEdit(const QString& id);
    void onSaved(const Item& item, bool isNew);
    void onDelete(const QString& id);
    void onRestore(const QString& id);
    void onFavorite(const QString& id, bool fav);
    void onNewFolder();
    void onRenameFolder(const QString& id);
    void onDeleteFolder(const QString& id);
    void onEmptyTrash();
    void openUrl(const QString& url);
    void showSettings();
    void showDetailFor(const QString& id);
    bool confirmDiscardEdit();

    // security
    void lockVault();
    void resetIdleTimer();
    void applyCaptureProtection();
    void persist();

    // autofill
    void registerHotkey();
    void onHotkey();
    void autofillItem(const QString& id, const ForegroundContext& ctx);
    void autofillFromDetail(const QString& id);
    void trackForeground();

    void toast(const QString& text, int ms = 2500);
    void setupTray();

    Vault* m_vault;
    ClipboardManager* m_clipboard;
    AutoType* m_autoType;
    Sidebar* m_sidebar = nullptr;
    ItemListPanel* m_list = nullptr;
    QStackedWidget* m_right = nullptr;
    ItemDetailPanel* m_detail = nullptr;
    ItemEditPanel* m_edit = nullptr;
    QLabel* m_toast = nullptr;
    QTimer* m_toastTimer = nullptr;
    QTimer* m_idleTimer = nullptr;
    QTimer* m_fgTimer = nullptr;
    QSystemTrayIcon* m_tray = nullptr;
    ForegroundContext m_lastForeign;
    bool m_editing = false;
    bool m_locking = false;
    bool m_quitting = false;
    bool m_sessionNotifyRegistered = false;
};

}  // namespace mp
