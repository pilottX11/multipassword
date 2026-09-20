#pragma once
// Thin, opinionated wrapper around libsodium.
//
// Algorithms (fixed, no negotiation = no downgrade attacks):
//   KDF   : Argon2id (crypto_pwhash, ALG_ARGON2ID13)
//   AEAD  : XChaCha20-Poly1305-IETF (24-byte random nonce, 16-byte tag)
//   Hash  : BLAKE2b (generic) / SHA-256 (BIP39 checksum only)
//   RNG   : libsodium randombytes (OS CSPRNG)

#include <cstddef>
#include <cstdint>
#include <optional>

#include "core/SecureMemory.h"

namespace mp::crypto {

constexpr std::size_t kKeyBytes = 32;    // crypto_aead_xchacha20poly1305_ietf_KEYBYTES
constexpr std::size_t kNonceBytes = 24;  // crypto_aead_xchacha20poly1305_ietf_NPUBBYTES
constexpr std::size_t kTagBytes = 16;    // crypto_aead_xchacha20poly1305_ietf_ABYTES
constexpr std::size_t kSaltBytes = 16;   // crypto_pwhash_SALTBYTES

struct KdfParams {
    std::uint64_t opsLimit = 0;   // iterations
    std::uint64_t memLimit = 0;   // bytes
    int algorithm = 0;            // crypto_pwhash_ALG_ARGON2ID13

    // Defaults tuned for an interactive desktop app (~0.5-1s on a modern CPU,
    // 256 MiB). These are stored in the vault header so they can be raised
    // over time without breaking older vaults.
    static KdfParams recommended();
    // Cheaper parameters used only by unit tests.
    static KdfParams fast();
};

// --- random ---------------------------------------------------------------
SecureBytes randomBytes(std::size_t n);
void randomFill(std::uint8_t* out, std::size_t n);
// Uniform integer in [0, upperBound) without modulo bias.
std::uint32_t randomUniform(std::uint32_t upperBound);

// --- key derivation -------------------------------------------------------
// Derives a kKeyBytes key from a password. Returns nullopt on OOM.
std::optional<SecureBytes> deriveKey(const SecureBytes& password,
                                     const std::uint8_t* salt, std::size_t saltLen,
                                     const KdfParams& params);

// --- authenticated encryption --------------------------------------------
// Output layout: nonce(24) || ciphertext || tag(16)
SecureBytes aeadEncrypt(const SecureBytes& key,
                        const std::uint8_t* plaintext, std::size_t plaintextLen,
                        const std::uint8_t* aad, std::size_t aadLen);

// Input layout must match aeadEncrypt. Returns nullopt on authentication
// failure (wrong key, tampered data, wrong AAD).
std::optional<SecureBytes> aeadDecrypt(const SecureBytes& key,
                                       const std::uint8_t* blob, std::size_t blobLen,
                                       const std::uint8_t* aad, std::size_t aadLen);

// --- hashing ---------------------------------------------------------------
SecureBytes sha256(const std::uint8_t* data, std::size_t len);
// 32-byte BLAKE2b hash, optionally keyed.
SecureBytes blake2b(const std::uint8_t* data, std::size_t len,
                    const SecureBytes* key = nullptr);
// HMAC-SHA1 (only for RFC 6238 TOTP compatibility).
SecureBytes hmacSha1(const SecureBytes& key, const std::uint8_t* data, std::size_t len);

// --- encoding --------------------------------------------------------------
QString toBase64(const std::uint8_t* data, std::size_t len);
std::optional<SecureBytes> fromBase64(const QString& s);
QString toHex(const std::uint8_t* data, std::size_t len);

}  // namespace mp::crypto
