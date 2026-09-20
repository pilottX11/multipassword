#include "core/Crypto.h"

#include <cstring>

#include <sodium.h>

namespace mp::crypto {

static_assert(kKeyBytes == crypto_aead_xchacha20poly1305_ietf_KEYBYTES);
static_assert(kNonceBytes == crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
static_assert(kTagBytes == crypto_aead_xchacha20poly1305_ietf_ABYTES);
static_assert(kSaltBytes == crypto_pwhash_SALTBYTES);

KdfParams KdfParams::recommended() {
    KdfParams p;
    p.opsLimit = crypto_pwhash_OPSLIMIT_MODERATE;  // 3
    p.memLimit = crypto_pwhash_MEMLIMIT_MODERATE;  // 256 MiB
    p.algorithm = crypto_pwhash_ALG_ARGON2ID13;
    return p;
}

KdfParams KdfParams::fast() {
    KdfParams p;
    p.opsLimit = crypto_pwhash_OPSLIMIT_MIN;
    p.memLimit = crypto_pwhash_MEMLIMIT_MIN;
    p.algorithm = crypto_pwhash_ALG_ARGON2ID13;
    return p;
}

SecureBytes randomBytes(std::size_t n) {
    SecureBytes out(n);
    if (n) randombytes_buf(out.data(), n);
    return out;
}

void randomFill(std::uint8_t* out, std::size_t n) {
    if (n) randombytes_buf(out, n);
}

std::uint32_t randomUniform(std::uint32_t upperBound) {
    return randombytes_uniform(upperBound);
}

std::optional<SecureBytes> deriveKey(const SecureBytes& password,
                                     const std::uint8_t* salt, std::size_t saltLen,
                                     const KdfParams& params) {
    if (saltLen != kSaltBytes) return std::nullopt;
    if (params.algorithm != crypto_pwhash_ALG_ARGON2ID13) return std::nullopt;
    if (params.opsLimit < crypto_pwhash_OPSLIMIT_MIN ||
        params.memLimit < crypto_pwhash_MEMLIMIT_MIN)
        return std::nullopt;

    SecureBytes key(kKeyBytes);
    int rc = crypto_pwhash(key.data(), key.size(),
                           reinterpret_cast<const char*>(password.data()),
                           password.size(), salt,
                           params.opsLimit, static_cast<std::size_t>(params.memLimit),
                           params.algorithm);
    if (rc != 0) return std::nullopt;  // out of memory
    return key;
}

SecureBytes aeadEncrypt(const SecureBytes& key,
                        const std::uint8_t* plaintext, std::size_t plaintextLen,
                        const std::uint8_t* aad, std::size_t aadLen) {
    SecureBytes out(kNonceBytes + plaintextLen + kTagBytes);
    randombytes_buf(out.data(), kNonceBytes);
    unsigned long long clen = 0;
    crypto_aead_xchacha20poly1305_ietf_encrypt(
        out.data() + kNonceBytes, &clen,
        plaintext, plaintextLen,
        aad, aadLen,
        nullptr, out.data(), key.data());
    out.resize(kNonceBytes + static_cast<std::size_t>(clen));
    return out;
}

std::optional<SecureBytes> aeadDecrypt(const SecureBytes& key,
                                       const std::uint8_t* blob, std::size_t blobLen,
                                       const std::uint8_t* aad, std::size_t aadLen) {
    if (key.size() != kKeyBytes) return std::nullopt;
    if (blobLen < kNonceBytes + kTagBytes) return std::nullopt;
    const std::uint8_t* nonce = blob;
    const std::uint8_t* ct = blob + kNonceBytes;
    const std::size_t ctLen = blobLen - kNonceBytes;

    SecureBytes out(ctLen - kTagBytes);
    unsigned long long mlen = 0;
    int rc = crypto_aead_xchacha20poly1305_ietf_decrypt(
        out.data(), &mlen, nullptr,
        ct, ctLen, aad, aadLen, nonce, key.data());
    if (rc != 0) return std::nullopt;
    out.resize(static_cast<std::size_t>(mlen));
    return out;
}

SecureBytes sha256(const std::uint8_t* data, std::size_t len) {
    SecureBytes out(crypto_hash_sha256_BYTES);
    crypto_hash_sha256(out.data(), data, len);
    return out;
}

SecureBytes blake2b(const std::uint8_t* data, std::size_t len, const SecureBytes* key) {
    SecureBytes out(32);
    crypto_generichash(out.data(), out.size(), data, len,
                       key ? key->data() : nullptr, key ? key->size() : 0);
    return out;
}

SecureBytes hmacSha1(const SecureBytes& key, const std::uint8_t* data, std::size_t len) {
    // libsodium has no SHA-1; implement HMAC-SHA1 (RFC 2104) locally with a
    // compact SHA-1. SHA-1 is only used for TOTP where it is still the
    // de-facto standard (RFC 6238) and collision resistance is irrelevant.
    struct Sha1 {
        std::uint32_t h[5];
        std::uint8_t buf[64];
        std::uint64_t total = 0;
        std::size_t bufLen = 0;
        Sha1() { h[0] = 0x67452301; h[1] = 0xEFCDAB89; h[2] = 0x98BADCFE; h[3] = 0x10325476; h[4] = 0xC3D2E1F0; }
        static std::uint32_t rol(std::uint32_t v, int b) { return (v << b) | (v >> (32 - b)); }
        void block(const std::uint8_t* p) {
            std::uint32_t w[80];
            for (int i = 0; i < 16; ++i)
                w[i] = (std::uint32_t(p[i * 4]) << 24) | (std::uint32_t(p[i * 4 + 1]) << 16) |
                       (std::uint32_t(p[i * 4 + 2]) << 8) | std::uint32_t(p[i * 4 + 3]);
            for (int i = 16; i < 80; ++i) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
            std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
            for (int i = 0; i < 80; ++i) {
                std::uint32_t f, k;
                if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
                else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
                else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
                else { f = b ^ c ^ d; k = 0xCA62C1D6; }
                std::uint32_t t = rol(a, 5) + f + e + k + w[i];
                e = d; d = c; c = rol(b, 30); b = a; a = t;
            }
            h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
            sodium_memzero(w, sizeof(w));
        }
        void update(const std::uint8_t* p, std::size_t n) {
            total += n;
            while (n > 0) {
                std::size_t take = std::min(n, 64 - bufLen);
                std::memcpy(buf + bufLen, p, take);
                bufLen += take; p += take; n -= take;
                if (bufLen == 64) { block(buf); bufLen = 0; }
            }
        }
        void finish(std::uint8_t out[20]) {
            std::uint64_t bits = total * 8;
            std::uint8_t pad = 0x80;
            update(&pad, 1);
            std::uint8_t zero = 0;
            while (bufLen != 56) update(&zero, 1);
            std::uint8_t lenBytes[8];
            for (int i = 0; i < 8; ++i) lenBytes[i] = std::uint8_t(bits >> (56 - 8 * i));
            update(lenBytes, 8);
            for (int i = 0; i < 5; ++i) {
                out[i * 4] = std::uint8_t(h[i] >> 24); out[i * 4 + 1] = std::uint8_t(h[i] >> 16);
                out[i * 4 + 2] = std::uint8_t(h[i] >> 8); out[i * 4 + 3] = std::uint8_t(h[i]);
            }
            sodium_memzero(buf, sizeof(buf));
            sodium_memzero(h, sizeof(h));
        }
    };

    SecureBytes k(64);
    if (key.size() > 64) {
        Sha1 s; s.update(key.data(), key.size());
        std::uint8_t d[20]; s.finish(d);
        std::memcpy(k.data(), d, 20);
        sodium_memzero(d, 20);
    } else {
        std::memcpy(k.data(), key.data(), key.size());
    }
    SecureBytes ipad(64), opad(64);
    for (int i = 0; i < 64; ++i) { ipad.data()[i] = k.data()[i] ^ 0x36; opad.data()[i] = k.data()[i] ^ 0x5c; }

    Sha1 inner; inner.update(ipad.data(), 64); inner.update(data, len);
    std::uint8_t innerDigest[20]; inner.finish(innerDigest);
    Sha1 outer; outer.update(opad.data(), 64); outer.update(innerDigest, 20);
    SecureBytes out(20); outer.finish(out.data());
    sodium_memzero(innerDigest, 20);
    return out;
}

QString toBase64(const std::uint8_t* data, std::size_t len) {
    const std::size_t outLen = sodium_base64_encoded_len(len, sodium_base64_VARIANT_ORIGINAL);
    std::string s(outLen, '\0');
    sodium_bin2base64(s.data(), outLen, data, len, sodium_base64_VARIANT_ORIGINAL);
    s.resize(std::strlen(s.c_str()));
    QString q = QString::fromLatin1(s.c_str());
    wipe(s);
    return q;
}

std::optional<SecureBytes> fromBase64(const QString& s) {
    QByteArray in = s.toLatin1();
    SecureBytes out(static_cast<std::size_t>(in.size()));
    std::size_t outLen = 0;
    int rc = sodium_base642bin(out.data(), out.size(), in.constData(),
                               static_cast<std::size_t>(in.size()), nullptr, &outLen,
                               nullptr, sodium_base64_VARIANT_ORIGINAL);
    wipe(in);
    if (rc != 0) return std::nullopt;
    out.resize(outLen);
    return out;
}

QString toHex(const std::uint8_t* data, std::size_t len) {
    std::string s(len * 2 + 1, '\0');
    sodium_bin2hex(s.data(), s.size(), data, len);
    s.resize(len * 2);
    QString q = QString::fromLatin1(s.c_str());
    wipe(s);
    return q;
}

}  // namespace mp::crypto
