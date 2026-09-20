#pragma once
// Platform layer for autofill.
//
//   * captures the foreground window (title, process, browser URL via
//     UI Automation) BEFORE our own window steals focus,
//   * registers a system-wide hotkey (RegisterHotKey) that the main window
//     receives as WM_HOTKEY,
//   * types a sequence of actions into the target window with SendInput
//     using Unicode scan codes (works regardless of keyboard layout).
//
// Only the Windows implementation is functional; other platforms compile to
// no-ops so the core/UI still build.

#include <QObject>
#include <QString>
#include <QVector>

#include "autofill/AutofillEngine.h"

namespace mp {

class AutoType : public QObject {
    Q_OBJECT
public:
    explicit AutoType(QObject* parent = nullptr);
    ~AutoType() override;

    // Snapshot of the window that currently has focus (call this before
    // showing any of our own UI).
    static ForegroundContext captureForeground(bool resolveBrowserUrl = true);

    // Registers/unregisters a global hotkey on the given native window id.
    // Sequence like "Ctrl+Alt+A". Returns false if the OS refused it.
    bool registerHotkey(quintptr nativeWindowId, const QString& sequence);
    void unregisterHotkey();
    int hotkeyId() const { return m_hotkeyId; }

    // Brings the target to the foreground and types the actions. Runs
    // synchronously; keep sequences short. Secrets are wiped afterwards.
    bool typeInto(const ForegroundContext& target, QVector<AutofillEngine::Action> actions);

    static bool isSupported();

private:
    quintptr m_hwnd = 0;
    int m_hotkeyId = 0;
    bool m_registered = false;
};

}  // namespace mp
