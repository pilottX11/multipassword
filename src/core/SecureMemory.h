#pragma once
// Secure memory primitives for multipassword.
//
// SecureBytes is a byte buffer whose storage is:
//   * locked into RAM (sodium_mlock) so it is never swapped to disk,
//   * wiped with sodium_memzero before the memory is released,
//   * never copied implicitly (copy operations are explicit).
//
// Every key, password and seed phrase that passes through the core layer
// lives in a SecureBytes. Qt string types are only used at the UI boundary.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include <QByteArray>
#include <QString>

namespace mp {

// Custom allocator that mlock()s and wipes every allocation.
template <typename T>
struct SecureAllocator {
    using value_type = T;

    SecureAllocator() noexcept = default;
    template <typename U>
    SecureAllocator(const SecureAllocator<U>&) noexcept {}

    T* allocate(std::size_t n);
    void deallocate(T* p, std::size_t n) noexcept;

    template <typename U>
    bool operator==(const SecureAllocator<U>&) const noexcept { return true; }
    template <typename U>
    bool operator!=(const SecureAllocator<U>&) const noexcept { return false; }
};

void* secureAllocRaw(std::size_t bytes);
void secureFreeRaw(void* p, std::size_t bytes) noexcept;

template <typename T>
T* SecureAllocator<T>::allocate(std::size_t n) {
    return static_cast<T*>(secureAllocRaw(n * sizeof(T)));
}
template <typename T>
void SecureAllocator<T>::deallocate(T* p, std::size_t n) noexcept {
    secureFreeRaw(p, n * sizeof(T));
}

class SecureBytes {
public:
    SecureBytes() = default;
    explicit SecureBytes(std::size_t size);
    SecureBytes(const std::uint8_t* data, std::size_t size);
    explicit SecureBytes(std::string_view s);

    SecureBytes(SecureBytes&&) noexcept = default;
    SecureBytes& operator=(SecureBytes&&) noexcept = default;

    // Copies are explicit to make secret duplication visible in code review.
    SecureBytes(const SecureBytes&) = delete;
    SecureBytes& operator=(const SecureBytes&) = delete;
    SecureBytes clone() const;

    ~SecureBytes() = default;  // allocator wipes

    static SecureBytes fromQByteArray(const QByteArray& b);
    static SecureBytes fromQString(const QString& s);  // UTF-8

    // Convert to Qt types. The returned Qt object is NOT secure memory; the
    // caller must wipe it (see wipe()) or keep its lifetime minimal.
    QByteArray toQByteArray() const;
    QString toQString() const;  // UTF-8

    std::uint8_t* data() { return m_buf.data(); }
    const std::uint8_t* data() const { return m_buf.data(); }
    std::size_t size() const { return m_buf.size(); }
    bool empty() const { return m_buf.empty(); }

    void resize(std::size_t n) { m_buf.resize(n); }
    void clear();
    void append(const std::uint8_t* d, std::size_t n);
    void append(const SecureBytes& other) { append(other.data(), other.size()); }

    // Constant-time comparison.
    bool constantTimeEquals(const SecureBytes& other) const;

private:
    std::vector<std::uint8_t, SecureAllocator<std::uint8_t>> m_buf;
};

// Best-effort wiping of Qt containers. Works when the container is not
// shared (implicit sharing) - callers should ensure they hold the only ref.
void wipe(QByteArray& b);
void wipe(QString& s);
void wipe(std::string& s);

// Zero any trivially-copyable object.
template <typename T>
void wipeObject(T& obj) {
    static_assert(std::is_trivially_copyable_v<T>);
    volatile auto* p = reinterpret_cast<volatile std::uint8_t*>(&obj);
    for (std::size_t i = 0; i < sizeof(T); ++i) p[i] = 0;
}

// Must be called once at startup before any other core API.
bool initSecureRuntime();

}  // namespace mp
