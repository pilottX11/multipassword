#include "ui/ClipboardManager.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>
#include <QTimer>

#include "core/Crypto.h"
#include "core/Settings.h"

namespace mp {

ClipboardManager::ClipboardManager(QObject* parent) : QObject(parent), m_timer(new QTimer(this)) {
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, [this] {
        QClipboard* cb = QGuiApplication::clipboard();
        if (hashOf(cb->text()) == m_lastHash) {
            cb->clear();
            emit cleared();
        }
        m_lastHash.clear();
    });
}

QString ClipboardManager::hashOf(const QString& s) {
    const QByteArray u = s.toUtf8();
    SecureBytes h = crypto::blake2b(reinterpret_cast<const std::uint8_t*>(u.constData()),
                                    static_cast<std::size_t>(u.size()));
    return crypto::toHex(h.data(), h.size());
}

void ClipboardManager::copy(const QString& text, bool sensitive, const QString& what) {
    QClipboard* cb = QGuiApplication::clipboard();
    auto* mime = new QMimeData();
    mime->setText(text);
#ifdef _WIN32
    // Ask Windows clipboard history / cloud sync to skip this entry.
    mime->setData(QStringLiteral("ExcludeClipboardContentFromMonitorProcessing"), QByteArray("1"));
    mime->setData(QStringLiteral("CanIncludeInClipboardHistory"), QByteArray(4, '\0'));
    mime->setData(QStringLiteral("CanUploadToCloudClipboard"), QByteArray(4, '\0'));
#endif
    cb->setMimeData(mime);
    int secs = 0;
    if (sensitive) {
        secs = Settings::instance().clipboardClearSeconds();
        if (secs > 0) {
            m_lastHash = hashOf(text);
            m_timer->start(secs * 1000);
        }
    }
    emit copied(what, secs);
}

void ClipboardManager::clearNow() {
    m_timer->stop();
    QGuiApplication::clipboard()->clear();
    m_lastHash.clear();
    emit cleared();
}

}  // namespace mp
