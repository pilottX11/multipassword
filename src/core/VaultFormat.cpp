#include "core/VaultFormat.h"

#include <cstring>

#include <sodium.h>

namespace mp::vaultformat {

static const std::uint8_t kMagic[kMagicSize] = {'M', 'P', 'V', 0x01};

static void putU64(std::uint8_t* p, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i));
}
static std::uint64_t getU64(const std::uint8_t* p) {
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= static_cast<std::uint64_t>(p[i]) << (8 * i);
    return v;
}

SecureBytes encodeHeader(const Header& h) {
    SecureBytes out(kHeaderSize);
    std::uint8_t* p = out.data();
    std::memcpy(p, kMagic, kMagicSize);
    p[4] = kFormatVersion;
    p[5] = kKdfArgon2id13;
    putU64(p + 6, h.kdf.opsLimit);
    putU64(p + 14, h.kdf.memLimit);
    std::memcpy(p + kSaltOffset, h.salt, crypto::kSaltBytes);
    p[38] = h.flags;
    std::memcpy(p + kWrapAadSize, h.wrappedKey, kWrappedKeySize);
    return out;
}

std::optional<Header> parseHeader(const std::uint8_t* blob, std::size_t len) {
    if (len < kHeaderSize) return std::nullopt;
    if (std::memcmp(blob, kMagic, kMagicSize) != 0) return std::nullopt;
    if (blob[4] != kFormatVersion) return std::nullopt;
    if (blob[5] != kKdfArgon2id13) return std::nullopt;
    Header h;
    h.kdf.opsLimit = getU64(blob + 6);
    h.kdf.memLimit = getU64(blob + 14);
    h.kdf.algorithm = crypto_pwhash_ALG_ARGON2ID13;
    // Refuse absurd parameters that could be used to DoS the machine via a
    // crafted vault file (memLimit is bounded to 4 GiB, ops to 64).
    if (h.kdf.memLimit > (4ULL << 30) || h.kdf.opsLimit > 64) return std::nullopt;
    std::memcpy(h.salt, blob + kSaltOffset, crypto::kSaltBytes);
    h.flags = blob[38];
    std::memcpy(h.wrappedKey, blob + kWrapAadSize, kWrappedKeySize);
    return h;
}

static std::optional<Header> wrapWithPassword(const SecureBytes& vaultKey,
                                              const SecureBytes& password,
                                              const crypto::KdfParams& params) {
    Header h;
    h.kdf = params;
    crypto::randomFill(h.salt, crypto::kSaltBytes);
    h.flags = 0;

    auto masterKey = crypto::deriveKey(password, h.salt, crypto::kSaltBytes, params);
    if (!masterKey) return std::nullopt;

    // AAD for the wrap is the header prefix (magic..flags). Build a temporary
    // header with a zeroed wrapped-key slot to compute it.
    SecureBytes prefix = encodeHeader(h);
    SecureBytes wrapped = crypto::aeadEncrypt(*masterKey, vaultKey.data(), vaultKey.size(),
                                              prefix.data(), kWrapAadSize);
    if (wrapped.size() != kWrappedKeySize) return std::nullopt;
    std::memcpy(h.wrappedKey, wrapped.data(), kWrappedKeySize);
    return h;
}

std::optional<NewVault> createHeader(const SecureBytes& password, const crypto::KdfParams& params) {
    NewVault nv;
    nv.vaultKey = crypto::randomBytes(crypto::kKeyBytes);
    auto h = wrapWithPassword(nv.vaultKey, password, params);
    if (!h) return std::nullopt;
    nv.header = *h;
    return nv;
}

std::optional<Header> rewrapHeader(const SecureBytes& vaultKey, const SecureBytes& newPassword,
                                   const crypto::KdfParams& params) {
    return wrapWithPassword(vaultKey, newPassword, params);
}

std::optional<SecureBytes> unwrapVaultKey(const Header& h, const SecureBytes& password) {
    auto masterKey = crypto::deriveKey(password, h.salt, crypto::kSaltBytes, h.kdf);
    if (!masterKey) return std::nullopt;
    SecureBytes hdr = encodeHeader(h);
    auto key = crypto::aeadDecrypt(*masterKey, h.wrappedKey, kWrappedKeySize,
                                   hdr.data(), kWrapAadSize);
    if (!key || key->size() != crypto::kKeyBytes) return std::nullopt;
    return key;
}

SecureBytes encodeVaultFile(const Header& h, const SecureBytes& vaultKey, const SecureBytes& payload) {
    SecureBytes hdr = encodeHeader(h);
    SecureBytes body = crypto::aeadEncrypt(vaultKey, payload.data(), payload.size(),
                                           hdr.data(), hdr.size());
    SecureBytes file;
    file.append(hdr);
    file.append(body);
    return file;
}

std::optional<SecureBytes> decodeVaultBody(const Header& h, const SecureBytes& vaultKey,
                                           const std::uint8_t* blob, std::size_t len) {
    if (len < kHeaderSize) return std::nullopt;
    SecureBytes hdr = encodeHeader(h);
    // The AAD must be the exact header bytes as they appear on disk.
    if (sodium_memcmp(hdr.data(), blob, kHeaderSize) != 0) return std::nullopt;
    return crypto::aeadDecrypt(vaultKey, blob + kHeaderSize, len - kHeaderSize,
                               hdr.data(), hdr.size());
}

}  // namespace mp::vaultformat
