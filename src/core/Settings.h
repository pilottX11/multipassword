#pragma once
// Non-secret application preferences (stored with QSettings; nothing here is
// sensitive - vault contents never touch QSettings).

#include <QObject>
#include <QString>

namespace mp {

class Settings : public QObject {
    Q_OBJECT
public:
    static Settings& instance();

    QString vaultPath() const;
    void setVaultPath(const QString& p);

    int autoLockMinutes() const;           // 0 = never
    void setAutoLockMinutes(int m);

    bool lockOnMinimize() const;
    void setLockOnMinimize(bool b);

    bool lockOnScreenLock() const;
    void setLockOnScreenLock(bool b);

    int clipboardClearSeconds() const;     // 0 = never
    void setClipboardClearSeconds(int s);

    bool autofillEnabled() const;
    void setAutofillEnabled(bool b);

    QString autofillHotkey() const;        // e.g. "Ctrl+Alt+A"
    void setAutofillHotkey(const QString& s);

    QString autofillSequence() const;      // "{USERNAME}{TAB}{PASSWORD}{ENTER}"
    void setAutofillSequence(const QString& s);

    bool excludeFromScreenCapture() const;
    void setExcludeFromScreenCapture(bool b);

    bool minimizeToTray() const;
    void setMinimizeToTray(bool b);

    // Onboarding / account (non-secret). The hint is stored in plaintext by
    // design - the signup page warns about that.
    QString ownerName() const;
    void setOwnerName(const QString& s);
    QString passwordHint() const;
    void setPasswordHint(const QString& s);

signals:
    void changed();

private:
    Settings();
};

}  // namespace mp
