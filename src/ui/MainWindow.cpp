#include "ui/MainWindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFile>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QScreen>
#include <QShortcut>
#include <QStackedWidget>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
#include <QWindow>

#include "core/Settings.h"
#include "ui/AutofillPicker.h"
#include "ui/ClipboardManager.h"
#include "ui/Icons.h"
#include "ui/ItemDetailPanel.h"
#include "ui/ItemEditPanel.h"
#include "ui/ItemListPanel.h"
#include "ui/SettingsDialog.h"
#include "ui/Sidebar.h"
#include "ui/Theme.h"
#include "ui/SignupDialog.h"
#include "ui/UnlockDialog.h"

#ifdef _WIN32
#include <windows.h>
#include <wtsapi32.h>
#endif

namespace mp {

MainWindow::MainWindow(Vault* vault, QWidget* parent)
    : QMainWindow(parent), m_vault(vault),
      m_clipboard(new ClipboardManager(this)), m_autoType(new AutoType(this)) {
    setWindowTitle("multipassword");
    setWindowIcon(icons::icon("shield", theme::kAccent, 64));
    resize(1120, 700);
    setMinimumSize(900, 540);

    auto* root = new QWidget(this);
    root->setObjectName("Root");
    auto* lay = new QHBoxLayout(root);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_sidebar = new Sidebar(m_vault, root);
    m_list = new ItemListPanel(m_vault, root);
    m_right = new QStackedWidget(root);
    m_detail = new ItemDetailPanel(m_clipboard, m_right);
    m_edit = new ItemEditPanel(m_vault, m_right);
    m_right->addWidget(m_detail);
    m_right->addWidget(m_edit);

    lay->addWidget(m_sidebar);
    lay->addWidget(m_list);
    lay->addWidget(m_right, 1);
    setCentralWidget(root);

    // toast overlay
    m_toast = new QLabel(root);
    m_toast->setStyleSheet(QStringLiteral(
        "background:%1;color:%2;border:1px solid %3;border-radius:8px;padding:8px 14px;font-weight:600;")
                               .arg(theme::kInputBg.name(), theme::kText.name(), theme::kSeparator.name()));
    m_toast->hide();
    m_toastTimer = new QTimer(this);
    m_toastTimer->setSingleShot(true);
    connect(m_toastTimer, &QTimer::timeout, m_toast, &QLabel::hide);

    // wiring ----------------------------------------------------------------
    connect(m_sidebar, &Sidebar::filterChanged, this, &MainWindow::onFilterChanged);
    connect(m_sidebar, &Sidebar::newFolderRequested, this, &MainWindow::onNewFolder);
    connect(m_sidebar, &Sidebar::renameFolderRequested, this, &MainWindow::onRenameFolder);
    connect(m_sidebar, &Sidebar::deleteFolderRequested, this, &MainWindow::onDeleteFolder);
    connect(m_sidebar, &Sidebar::emptyTrashRequested, this, &MainWindow::onEmptyTrash);
    connect(m_sidebar, &Sidebar::lockRequested, this, &MainWindow::lockVault);
    connect(m_sidebar, &Sidebar::settingsRequested, this, &MainWindow::showSettings);
    connect(m_list, &ItemListPanel::itemSelected, this, &MainWindow::onItemSelected);
    connect(m_list, &ItemListPanel::newItemRequested, this, &MainWindow::onNewItem);
    connect(m_detail, &ItemDetailPanel::editRequested, this, &MainWindow::onEdit);
    connect(m_detail, &ItemDetailPanel::deleteRequested, this, &MainWindow::onDelete);
    connect(m_detail, &ItemDetailPanel::restoreRequested, this, &MainWindow::onRestore);
    connect(m_detail, &ItemDetailPanel::favoriteToggled, this, &MainWindow::onFavorite);
    connect(m_detail, &ItemDetailPanel::autofillRequested, this, &MainWindow::autofillFromDetail);
    connect(m_detail, &ItemDetailPanel::openUrlRequested, this, &MainWindow::openUrl);
    connect(m_edit, &ItemEditPanel::saved, this, &MainWindow::onSaved);
    connect(m_edit, &ItemEditPanel::cancelled, this, [this] {
        m_editing = false;
        showDetailFor(m_list->selectedItemId());
    });
    connect(m_clipboard, &ClipboardManager::copied, this, [this](const QString& what, int secs) {
        toast(secs > 0 ? QStringLiteral("%1 copied · clears in %2 s").arg(what.isEmpty() ? "Value" : what).arg(secs)
                       : QStringLiteral("%1 copied").arg(what.isEmpty() ? "Value" : what));
    });
    connect(m_clipboard, &ClipboardManager::cleared, this, [this] { toast("Clipboard cleared", 1500); });

    // shortcuts
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_N), this, [this] { onNewItem(ItemType::Login); });
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F), this, [this] { m_list->focusSearch(); });
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_L), this, [this] { lockVault(); });
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma), this, [this] { showSettings(); });
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_E), this, [this] {
        if (!m_editing && !m_list->selectedItemId().isEmpty()) onEdit(m_list->selectedItemId());
    });

    // security timers ---------------------------------------------------------
    m_idleTimer = new QTimer(this);
    m_idleTimer->setSingleShot(true);
    connect(m_idleTimer, &QTimer::timeout, this, &MainWindow::lockVault);
    qApp->installEventFilter(this);
    resetIdleTimer();

    m_fgTimer = new QTimer(this);
    m_fgTimer->setInterval(400);
    connect(m_fgTimer, &QTimer::timeout, this, &MainWindow::trackForeground);
    if (AutoType::isSupported()) m_fgTimer->start();

    connect(&Settings::instance(), &Settings::changed, this, [this] {
        resetIdleTimer();
        applyCaptureProtection();
    });
    connect(m_vault, &Vault::lockedChanged, this, [this](bool unlocked) {
        if (!unlocked) {
            m_detail->clear();
            m_right->setCurrentWidget(m_detail);
            m_editing = false;
        }
    });

    setupTray();
}

MainWindow::~MainWindow() {
#ifdef _WIN32
    if (m_sessionNotifyRegistered) WTSUnRegisterSessionNotification(reinterpret_cast<HWND>(winId()));
#endif
}

// --- lifecycle ------------------------------------------------------------------

void MainWindow::showEvent(QShowEvent* e) {
    QMainWindow::showEvent(e);
#ifdef _WIN32
    if (!m_sessionNotifyRegistered) {
        m_sessionNotifyRegistered =
            WTSRegisterSessionNotification(reinterpret_cast<HWND>(winId()), NOTIFY_FOR_THIS_SESSION) != 0;
    }
#endif
    applyCaptureProtection();
    registerHotkey();
}

bool MainWindow::unlockInteractive() {
    bool ok = false;
    for (;;) {
        const QString path = Settings::instance().vaultPath();
        if (!QFile::exists(path)) {
            // First run (or vault moved): sign-up page creates the vault.
            SignupDialog signup(m_vault, path, isVisible() ? this : nullptr);
            const int r = signup.exec();
            if (r == QDialog::Accepted) { ok = true; break; }
            if (!signup.existingVaultChosen().isEmpty()) continue;  // user picked a file → unlock it
            return false;
        }
        UnlockDialog dlg(m_vault, path, isVisible() ? this : nullptr);
        ok = dlg.exec() == QDialog::Accepted;
        break;
    }
    if (ok) {
        m_sidebar->refresh();
        m_sidebar->setFilter(ListFilter{ListFilter::All});
        m_list->refresh();
        resetIdleTimer();
    }
    return ok;
}

void MainWindow::lockVault() {
    if (m_locking || !m_vault->isUnlocked()) return;
    m_locking = true;
    if (m_editing && m_edit->isDirty()) {
        // Auto-save a dirty edit rather than losing it on forced lock.
        Item it = m_edit->currentItem();
        if (!it.title.trimmed().isEmpty()) {
            if (m_vault->findItem(it.id)) m_vault->updateItem(it); else m_vault->addItem(it);
            persist();
        }
    }
    m_editing = false;
    m_clipboard->clearNow();
    m_vault->lock();
    m_idleTimer->stop();
    if (m_quitting) { m_locking = false; return; }
    show();
    raise();
    activateWindow();
    if (!unlockInteractive()) {
        m_quitting = true;
        qApp->quit();
    }
    m_locking = false;
}

void MainWindow::resetIdleTimer() {
    const int mins = Settings::instance().autoLockMinutes();
    if (mins <= 0) { m_idleTimer->stop(); return; }
    m_idleTimer->start(mins * 60 * 1000);
}

bool MainWindow::eventFilter(QObject* obj, QEvent* e) {
    switch (e->type()) {
        case QEvent::KeyPress:
        case QEvent::MouseButtonPress:
        case QEvent::MouseMove:
        case QEvent::Wheel:
        case QEvent::TouchBegin:
            if (m_idleTimer->isActive()) resetIdleTimer();
            break;
        default:
            break;
    }
    return QMainWindow::eventFilter(obj, e);
}

void MainWindow::changeEvent(QEvent* e) {
    QMainWindow::changeEvent(e);
    if (e->type() == QEvent::WindowStateChange && isMinimized() && Settings::instance().lockOnMinimize()) {
        QTimer::singleShot(0, this, &MainWindow::lockVault);
    }
}

void MainWindow::closeEvent(QCloseEvent* e) {
    if (!m_quitting && Settings::instance().minimizeToTray() && m_tray && m_tray->isVisible()) {
        hide();
        e->ignore();
        return;
    }
    if (m_editing && m_edit->isDirty() && !confirmDiscardEdit()) { e->ignore(); return; }
    m_quitting = true;
    m_clipboard->clearNow();
    m_vault->lock();
    e->accept();
    qApp->quit();
}

void MainWindow::applyCaptureProtection() {
#ifdef _WIN32
    HWND hwnd = reinterpret_cast<HWND>(winId());
    SetWindowDisplayAffinity(hwnd, Settings::instance().excludeFromScreenCapture() ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
#endif
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
#ifdef _WIN32
    if (eventType == "windows_generic_MSG") {
        MSG* msg = static_cast<MSG*>(message);
        if (msg->message == WM_HOTKEY && static_cast<int>(msg->wParam) == m_autoType->hotkeyId()) {
            QTimer::singleShot(0, this, &MainWindow::onHotkey);
            *result = 0;
            return true;
        }
        if (msg->message == WM_WTSSESSION_CHANGE) {
            if ((msg->wParam == WTS_SESSION_LOCK || msg->wParam == WTS_SESSION_LOGOFF ||
                 msg->wParam == WTS_CONSOLE_DISCONNECT || msg->wParam == WTS_REMOTE_DISCONNECT) &&
                Settings::instance().lockOnScreenLock()) {
                QTimer::singleShot(0, this, &MainWindow::lockVault);
            }
        }
        if (msg->message == WM_POWERBROADCAST && msg->wParam == PBT_APMSUSPEND &&
            Settings::instance().lockOnScreenLock()) {
            QTimer::singleShot(0, this, &MainWindow::lockVault);
        }
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::setupTray() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;
    m_tray = new QSystemTrayIcon(icons::icon("shield", theme::kAccent, 32), this);
    m_tray->setToolTip("multipassword");
    auto* menu = new QMenu(this);
    menu->addAction(icons::icon("grid", theme::kText), "Open multipassword", this, [this] {
        showNormal(); raise(); activateWindow();
    });
    menu->addAction(icons::icon("zap", theme::kText), "Autofill…", this, &MainWindow::onHotkey);
    menu->addAction(icons::icon("lock", theme::kText), "Lock vault", this, &MainWindow::lockVault);
    menu->addSeparator();
    menu->addAction(icons::icon("x", theme::kDanger), "Quit", this, [this] { m_quitting = true; close(); });
    m_tray->setContextMenu(menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick) {
            showNormal(); raise(); activateWindow();
        }
    });
    m_tray->show();
}

void MainWindow::toast(const QString& text, int ms) {
    m_toast->setText(text);
    m_toast->adjustSize();
    QWidget* root = centralWidget();
    m_toast->move((root->width() - m_toast->width()) / 2, root->height() - m_toast->height() - 18);
    m_toast->raise();
    m_toast->show();
    m_toastTimer->start(ms);
}

// --- data wiring --------------------------------------------------------------------

void MainWindow::persist() {
    const VaultError err = m_vault->save();
    if (err != VaultError::None)
        QMessageBox::critical(this, "Save failed", vaultErrorMessage(err) + "\n\nYour changes are still in memory; fix the problem and try again.");
}

void MainWindow::onFilterChanged(const ListFilter& f) {
    if (m_editing && !confirmDiscardEdit()) return;
    m_editing = false;
    m_list->setFilter(f);
}

void MainWindow::showDetailFor(const QString& id) {
    const Item* it = id.isEmpty() ? nullptr : m_vault->findItem(id);
    QString folderName;
    if (it) if (const Folder* f = m_vault->findFolder(it->folderId)) folderName = f->name;
    m_detail->setItem(it, folderName);
    m_right->setCurrentWidget(m_detail);
}

bool MainWindow::confirmDiscardEdit() {
    if (!m_editing || !m_edit->isDirty()) return true;
    const auto r = QMessageBox::question(this, "Discard changes?",
                                         "You have unsaved changes. Discard them?",
                                         QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
    return r == QMessageBox::Discard;
}

void MainWindow::onItemSelected(const QString& id) {
    if (m_editing) {
        if (!m_edit->isDirty()) { m_editing = false; }
        else return;  // keep editing; list selection change is ignored
    }
    showDetailFor(id);
}

void MainWindow::onNewItem(ItemType type) {
    if (!m_vault->isUnlocked()) return;
    if (m_editing && !confirmDiscardEdit()) return;
    m_editing = true;
    m_edit->beginNew(type);
    // Pre-fill folder from current filter
    m_right->setCurrentWidget(m_edit);
}

void MainWindow::onEdit(const QString& id) {
    const Item* it = m_vault->findItem(id);
    if (!it) return;
    m_editing = true;
    m_edit->beginEdit(*it);
    m_right->setCurrentWidget(m_edit);
}

void MainWindow::onSaved(const Item& item, bool isNew) {
    if (isNew) {
        Item copy = item;
        const ListFilter f = m_sidebar->currentFilter();
        if (copy.folderId.isEmpty() && f.kind == ListFilter::Folder) copy.folderId = f.folderId;
        if (f.kind == ListFilter::Favorites) copy.favorite = true;
        m_vault->addItem(copy);
        // Make sure the new item is visible in the current filter.
        if (!f.matches(copy)) m_sidebar->setFilter(ListFilter{ListFilter::All});
    } else {
        m_vault->updateItem(item);
    }
    m_editing = false;
    persist();
    m_list->selectItem(item.id);
    showDetailFor(item.id);
    toast(isNew ? "Item created" : "Item saved", 1500);
}

void MainWindow::onDelete(const QString& id) {
    const Item* it = m_vault->findItem(id);
    if (!it) return;
    if (it->trashed) {
        const auto r = QMessageBox::warning(this, "Delete forever",
                                            QStringLiteral("Permanently delete \"%1\"? This cannot be undone.").arg(it->title),
                                            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (r != QMessageBox::Yes) return;
        m_vault->deletePermanently(id);
    } else {
        m_vault->moveToTrash(id);
        toast("Moved to Trash", 1500);
    }
    persist();
}

void MainWindow::onRestore(const QString& id) {
    m_vault->restoreFromTrash(id);
    persist();
    toast("Item restored", 1500);
}

void MainWindow::onFavorite(const QString& id, bool fav) {
    m_vault->setFavorite(id, fav);
    persist();
    m_list->selectItem(id);
    showDetailFor(id);
}

void MainWindow::onNewFolder() {
    bool ok = false;
    const QString name = QInputDialog::getText(this, "New Folder", "Folder name:", QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || name.isEmpty()) return;
    const Folder f = m_vault->addFolder(name);
    persist();
    ListFilter lf{ListFilter::Folder};
    lf.folderId = f.id;
    m_sidebar->setFilter(lf);
}

void MainWindow::onRenameFolder(const QString& id) {
    const Folder* f = m_vault->findFolder(id);
    if (!f) return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, "Rename Folder", "Folder name:", QLineEdit::Normal, f->name, &ok).trimmed();
    if (!ok || name.isEmpty()) return;
    m_vault->renameFolder(id, name);
    persist();
}

void MainWindow::onDeleteFolder(const QString& id) {
    const Folder* f = m_vault->findFolder(id);
    if (!f) return;
    const auto r = QMessageBox::question(this, "Delete Folder",
                                         QStringLiteral("Delete folder \"%1\"? Items inside are kept and moved to \"No folder\".").arg(f->name),
                                         QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (r != QMessageBox::Yes) return;
    m_vault->removeFolder(id);
    persist();
}

void MainWindow::onEmptyTrash() {
    const auto r = QMessageBox::warning(this, "Empty Trash", "Permanently delete everything in the Trash?",
                                        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (r != QMessageBox::Yes) return;
    m_vault->emptyTrash();
    persist();
}

void MainWindow::openUrl(const QString& url) {
    QString u = url.trimmed();
    if (u.isEmpty()) return;
    if (!u.contains("://")) u.prepend("https://");
    const QUrl q(u);
    if (q.scheme() != "https" && q.scheme() != "http") return;  // never launch arbitrary schemes
    QDesktopServices::openUrl(q);
}

void MainWindow::showSettings() {
    SettingsDialog dlg(m_vault, this);
    connect(&dlg, &SettingsDialog::hotkeyChanged, this, &MainWindow::registerHotkey);
    dlg.exec();
}

// --- autofill -------------------------------------------------------------------------

void MainWindow::registerHotkey() {
    m_autoType->unregisterHotkey();
    const Settings& s = Settings::instance();
    if (!s.autofillEnabled() || !AutoType::isSupported()) return;
    if (!m_autoType->registerHotkey(winId(), s.autofillHotkey()))
        toast(QStringLiteral("Could not register autofill hotkey %1").arg(s.autofillHotkey()), 4000);
}

void MainWindow::trackForeground() {
    // Remember the most recent foreground window that is not one of ours so
    // the "Autofill" button in the detail pane knows where to type.
    // Cheap poll: no UI Automation lookup here (that only happens on demand).
    const ForegroundContext ctx = AutoType::captureForeground(false);
    if (ctx.nativeHandle == 0) return;
    for (QWindow* w : qApp->topLevelWindows())
        if (w->winId() == ctx.nativeHandle) return;
    m_lastForeign = ctx;
}

void MainWindow::onHotkey() {
    const ForegroundContext ctx = AutoType::captureForeground();
    if (!m_vault->isUnlocked()) {
        showNormal(); raise(); activateWindow();
        if (!unlockInteractive()) return;
    }
    const auto matches = AutofillEngine::match(m_vault->items(), ctx);
    // Exactly one high-confidence (URL) match → fill without asking.
    if (matches.size() == 1 && matches[0].score >= 100) {
        autofillItem(matches[0].itemId, ctx);
        return;
    }
    if (matches.size() >= 2 && matches[0].score >= 100 && matches[1].score < 100) {
        autofillItem(matches[0].itemId, ctx);
        return;
    }
    AutofillPicker picker(m_vault, ctx, nullptr);
    QScreen* screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen) screen = QGuiApplication::primaryScreen();
    const QRect g = screen->availableGeometry();
    picker.move(g.center() - QPoint(picker.width() / 2, picker.height() / 2));
    picker.show();
    picker.raise();
    picker.activateWindow();
    if (picker.exec() == QDialog::Accepted && !picker.chosenItemId().isEmpty())
        autofillItem(picker.chosenItemId(), ctx);
}

void MainWindow::autofillItem(const QString& id, const ForegroundContext& ctx) {
    const Item* it = m_vault->findItem(id);
    if (!it) return;
    if (ctx.nativeHandle == 0) { toast("No target window to type into", 3000); return; }
    QString seq = Settings::instance().autofillSequence();
    if (it->type == ItemType::Card) seq = "{number}{TAB}{expiry}{TAB}{cvv}";
    else if (it->type == ItemType::Identity) seq = "{firstName}{TAB}{lastName}{TAB}{email}";
    auto actions = AutofillEngine::expandSequence(seq, *it);
    resetIdleTimer();
    const bool ok = m_autoType->typeInto(ctx, std::move(actions));
    if (!ok) toast("Autofill aborted: target window lost focus", 3000);
}

void MainWindow::autofillFromDetail(const QString& id) {
    if (m_lastForeign.nativeHandle == 0) {
        toast("Focus the login form in another app first, then press Autofill", 3500);
        return;
    }
    autofillItem(id, m_lastForeign);
}

}  // namespace mp
