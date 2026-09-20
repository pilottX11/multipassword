#include "core/Settings.h"

#include <QSettings>

#include "core/Vault.h"

namespace mp {

namespace {
QSettings& store() {
    static QSettings s(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("multipassword"), QStringLiteral("multipassword"));
    return s;
}
}  // namespace

Settings& Settings::instance() {
    static Settings s;
    return s;
}

Settings::Settings() = default;

#define MP_SETTING(TYPE, GETTER, SETTER, KEY, DEFAULT, READ)                       \
    TYPE Settings::GETTER() const { return store().value(QStringLiteral(KEY), DEFAULT).READ; } \
    void Settings::SETTER(TYPE v) {                                                 \
        store().setValue(QStringLiteral(KEY), v);                                   \
        store().sync();                                                             \
        emit changed();                                                             \
    }

MP_SETTING(int, autoLockMinutes, setAutoLockMinutes, "security/autoLockMinutes", 5, toInt())
MP_SETTING(bool, lockOnMinimize, setLockOnMinimize, "security/lockOnMinimize", false, toBool())
MP_SETTING(bool, lockOnScreenLock, setLockOnScreenLock, "security/lockOnScreenLock", true, toBool())
MP_SETTING(int, clipboardClearSeconds, setClipboardClearSeconds, "security/clipboardClearSeconds", 30, toInt())
MP_SETTING(bool, autofillEnabled, setAutofillEnabled, "autofill/enabled", true, toBool())
MP_SETTING(bool, excludeFromScreenCapture, setExcludeFromScreenCapture, "security/excludeFromScreenCapture", false, toBool())
MP_SETTING(bool, minimizeToTray, setMinimizeToTray, "ui/minimizeToTray", true, toBool())

QString Settings::vaultPath() const {
    return store().value(QStringLiteral("vault/path"), defaultVaultPath()).toString();
}
void Settings::setVaultPath(const QString& p) {
    store().setValue(QStringLiteral("vault/path"), p);
    store().sync();
    emit changed();
}

QString Settings::autofillHotkey() const {
    return store().value(QStringLiteral("autofill/hotkey"), QStringLiteral("Ctrl+Alt+A")).toString();
}
void Settings::setAutofillHotkey(const QString& s) {
    store().setValue(QStringLiteral("autofill/hotkey"), s);
    store().sync();
    emit changed();
}

QString Settings::autofillSequence() const {
    return store().value(QStringLiteral("autofill/sequence"),
                         QStringLiteral("{USERNAME}{TAB}{PASSWORD}{ENTER}")).toString();
}
void Settings::setAutofillSequence(const QString& s) {
    store().setValue(QStringLiteral("autofill/sequence"), s);
    store().sync();
    emit changed();
}

}  // namespace mp

namespace mp {

QString Settings::ownerName() const {
    return store().value(QStringLiteral("account/ownerName")).toString();
}
void Settings::setOwnerName(const QString& s) {
    store().setValue(QStringLiteral("account/ownerName"), s.trimmed());
    store().sync();
    emit changed();
}

QString Settings::passwordHint() const {
    return store().value(QStringLiteral("account/passwordHint")).toString();
}
void Settings::setPasswordHint(const QString& s) {
    store().setValue(QStringLiteral("account/passwordHint"), s.trimmed());
    store().sync();
    emit changed();
}

}  // namespace mp
