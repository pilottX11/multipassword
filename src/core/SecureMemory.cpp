#include "core/SecureMemory.h"

#include <cstring>
#include <new>
#include <stdexcept>

#include <sodium.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace mp {

void* secureAllocRaw(std::size_t bytes) {
    if (bytes == 0) bytes = 1;
    // sodium_malloc places guard pages around the allocation and mlocks it.
    // It also fills with canary bytes and aborts on overflow detection.
    void* p = sodium_malloc(bytes);
    if (!p) throw std::bad_alloc();
    return p;
}

void secureFreeRaw(void* p, std::size_t /*bytes*/) noexcept {
    if (p) sodium_free(p);  // sodium_free wipes before releasing
}

SecureBytes::SecureBytes(std::size_t size) : m_buf(size, 0) {}

SecureBytes::SecureBytes(const std::uint8_t* data, std::size_t size)
    : m_buf(data, data + size) {}

SecureBytes::SecureBytes(std::string_view s)
    : m_buf(reinterpret_cast<const std::uint8_t*>(s.data()),
            reinterpret_cast<const std::uint8_t*>(s.data()) + s.size()) {}

SecureBytes SecureBytes::clone() const {
    return SecureBytes(data(), size());
}

SecureBytes SecureBytes::fromQByteArray(const QByteArray& b) {
    return SecureBytes(reinterpret_cast<const std::uint8_t*>(b.constData()),
                       static_cast<std::size_t>(b.size()));
}

SecureBytes SecureBytes::fromQString(const QString& s) {
    QByteArray utf8 = s.toUtf8();
    SecureBytes out = fromQByteArray(utf8);
    wipe(utf8);
    return out;
}

QByteArray SecureBytes::toQByteArray() const {
    return QByteArray(reinterpret_cast<const char*>(data()),
                      static_cast<qsizetype>(size()));
}

QString SecureBytes::toQString() const {
    return QString::fromUtf8(reinterpret_cast<const char*>(data()),
                             static_cast<qsizetype>(size()));
}

void SecureBytes::clear() {
    if (!m_buf.empty()) sodium_memzero(m_buf.data(), m_buf.size());
    m_buf.clear();
    m_buf.shrink_to_fit();
}

void SecureBytes::append(const std::uint8_t* d, std::size_t n) {
    m_buf.insert(m_buf.end(), d, d + n);
}

bool SecureBytes::constantTimeEquals(const SecureBytes& other) const {
    if (size() != other.size()) return false;
    if (empty()) return true;
    return sodium_memcmp(data(), other.data(), size()) == 0;
}

void wipe(QByteArray& b) {
    if (b.isEmpty()) return;
    b.detach();
    sodium_memzero(b.data(), static_cast<std::size_t>(b.size()));
    b.clear();
}

void wipe(QString& s) {
    if (s.isEmpty()) return;
    s.detach();
    sodium_memzero(s.data(), static_cast<std::size_t>(s.size()) * sizeof(QChar));
    s.clear();
}

void wipe(std::string& s) {
    if (s.empty()) return;
    sodium_memzero(s.data(), s.size());
    s.clear();
}

bool initSecureRuntime() {
    if (sodium_init() < 0) return false;

#ifdef _WIN32
    // Raise the working-set minimum so mlock'd pages are not refused once we
    // hold a few hundred KB of locked secrets. Best-effort only.
    SIZE_T minWs = 0, maxWs = 0;
    if (GetProcessWorkingSetSize(GetCurrentProcess(), &minWs, &maxWs)) {
        const SIZE_T extra = 32 * 1024 * 1024;
        SetProcessWorkingSetSize(GetCurrentProcess(), minWs + extra, maxWs + extra);
    }

    // Opt out of Windows Error Reporting memory dumps for this process; a
    // crash dump would otherwise contain unlocked secrets.
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

    // Refuse to load DLLs from the current working directory / remote shares
    // (DLL-preloading attacks). Only system + application directories remain.
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);

    // Prevent lower-integrity processes from injecting into or reading this
    // process by opting into extension-point disabling (AppInit DLLs, IMEs).
    PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY ext{};
    ext.DisableExtensionPoints = 1;
    SetProcessMitigationPolicy(ProcessExtensionPointDisablePolicy, &ext, sizeof(ext));
#endif
    return true;
}

}  // namespace mp
