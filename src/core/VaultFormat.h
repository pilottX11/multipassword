#pragma once
// On-disk vault container (".mpv").
//
//   offset size  field
//   0      4     magic  "MPV\x01"
//   4      1     format version (1)
//   5      1     kdf algorithm id (1 = Argon2id v1.3)
//   6      8     kdf opsLimit  (little-endian u64)
//   14     8     kdf memLimit  (little-endian u64, bytes)
//   22     16    kdf salt
//   38     1     flags (reserved, 0)
//   39     72    wrapped vault key = XChaCha20-Poly1305(masterKey) over the
//                32-byte random vault key: nonce(24) || ct(32) || tag(16)
//                AAD = bytes [0,39)
//   111    ...   body = XChaCha20-Poly1305(vaultKey) over the JSON payload:
//                nonce(24) || ct || tag(16)
//                AAD = bytes [0,111)   (the whole header, incl. wrapped key)
//
// Two-level keying means changing the master password only rewraps the
// 32-byte vault key; the body does not need to be re-encrypted. Every save
// uses a fresh random body nonce.

#include <cstdint>
#include <optional>

#include "core/Crypto.h"
#include "core/SecureMemory.h"

namespace mp::vaultformat {

constexpr std::size_t kMagicSize = 4;
constexpr std::size_t kSaltOffset = 22;
constexpr std::size_t kWrapAadSize = 39;
constexpr std::size_t kWrappedKeySize = crypto::kNonceBytes + crypto::kKeyBytes + crypto::kTagBytes;  // 72
constexpr std::size_t kHeaderSize = kWrapAadSize + kWrappedKeySize;  // 111
constexpr std::uint8_t kFormatVersion = 1;
constexpr std::uint8_t kKdfArgon2id13 = 1;

struct Header {
    crypto::KdfParams kdf;
    std::uint8_t salt[crypto::kSaltBytes] = {};
    std::uint8_t wrappedKey[kWrappedKeySize] = {};
    std::uint8_t flags = 0;
};

// Serialises a header into exactly kHeaderSize bytes.
SecureBytes encodeHeader(const Header& h);
// Parses a header from the beginning of a blob. Fails on bad magic/version.
std::optional<Header> parseHeader(const std::uint8_t* blob, std::size_t len);

// Creates a brand-new header: random salt, random vault key wrapped by the
// key derived from `password`. Returns header + the plaintext vault key.
struct NewVault {
    Header header;
    SecureBytes vaultKey;
};
std::optional<NewVault> createHeader(const SecureBytes& password, const crypto::KdfParams& params);

// Rewraps an existing vault key under a new password (new salt too).
std::optional<Header> rewrapHeader(const SecureBytes& vaultKey, const SecureBytes& newPassword,
                                   const crypto::KdfParams& params);

// Derives the master key from the password and unwraps the vault key.
// nullopt => wrong password or corrupted header.
std::optional<SecureBytes> unwrapVaultKey(const Header& h, const SecureBytes& password);

// Full file assembly / disassembly.
SecureBytes encodeVaultFile(const Header& h, const SecureBytes& vaultKey, const SecureBytes& payload);
std::optional<SecureBytes> decodeVaultBody(const Header& h, const SecureBytes& vaultKey,
                                           const std::uint8_t* blob, std::size_t len);

}  // namespace mp::vaultformat
