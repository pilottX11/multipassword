// Minimal dependency-free test runner for the multipassword core.
// Run: build/<preset>/mp_tests.exe

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <cstdio>
#include <functional>
#include <vector>

#include "autofill/AutofillEngine.h"
#include "core/Bip39.h"
#include "core/Crypto.h"
#include "core/PasswordGenerator.h"
#include "core/PasswordStrength.h"
#include "core/SecureMemory.h"
#include "core/Totp.h"
#include "core/Vault.h"
#include "core/VaultFormat.h"


namespace {
int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                                 \
    do {                                                                            \
        ++g_checks;                                                                 \
        if (!(cond)) {                                                              \
            ++g_failures;                                                           \
            std::fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
        }                                                                           \
    } while (0)

struct Test {
    const char* name;
    std::function<void()> fn;
};
std::vector<Test>& tests() { static std::vector<Test> t; return t; }
struct Reg { Reg(const char* n, std::function<void()> f) { tests().push_back({n, std::move(f)}); } };
#define TEST(name) static void name(); static Reg reg_##name(#name, name); static void name()

using namespace mp;

// ---------------------------------------------------------------------------

TEST(secure_bytes_basics) {
    SecureBytes a("hello");
    CHECK(a.size() == 5);
    SecureBytes b = a.clone();
    CHECK(a.constantTimeEquals(b));
    b.data()[0] = 'j';
    CHECK(!a.constantTimeEquals(b));
    CHECK(a.toQString() == "hello");
    a.clear();
    CHECK(a.empty());
}

TEST(aead_roundtrip_and_tamper) {
    SecureBytes key = crypto::randomBytes(crypto::kKeyBytes);
    const std::string msg = "attack at dawn";
    const std::string aad = "header";
    SecureBytes ct = crypto::aeadEncrypt(key, reinterpret_cast<const std::uint8_t*>(msg.data()), msg.size(),
                                         reinterpret_cast<const std::uint8_t*>(aad.data()), aad.size());
    CHECK(ct.size() == crypto::kNonceBytes + msg.size() + crypto::kTagBytes);
    auto pt = crypto::aeadDecrypt(key, ct.data(), ct.size(), reinterpret_cast<const std::uint8_t*>(aad.data()), aad.size());
    CHECK(pt.has_value());
    CHECK(pt && std::string(reinterpret_cast<const char*>(pt->data()), pt->size()) == msg);
    // tamper ciphertext
    ct.data()[crypto::kNonceBytes + 2] ^= 0x01;
    CHECK(!crypto::aeadDecrypt(key, ct.data(), ct.size(), reinterpret_cast<const std::uint8_t*>(aad.data()), aad.size()));
    ct.data()[crypto::kNonceBytes + 2] ^= 0x01;
    // wrong AAD
    CHECK(!crypto::aeadDecrypt(key, ct.data(), ct.size(), reinterpret_cast<const std::uint8_t*>("x"), 1));
    // wrong key
    SecureBytes key2 = crypto::randomBytes(crypto::kKeyBytes);
    CHECK(!crypto::aeadDecrypt(key2, ct.data(), ct.size(), reinterpret_cast<const std::uint8_t*>(aad.data()), aad.size()));
}

TEST(kdf_deterministic) {
    SecureBytes pw("correct horse battery staple");
    std::uint8_t salt[crypto::kSaltBytes] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    auto k1 = crypto::deriveKey(pw, salt, sizeof(salt), crypto::KdfParams::fast());
    auto k2 = crypto::deriveKey(pw, salt, sizeof(salt), crypto::KdfParams::fast());
    CHECK(k1 && k2 && k1->constantTimeEquals(*k2));
    salt[0] = 99;
    auto k3 = crypto::deriveKey(pw, salt, sizeof(salt), crypto::KdfParams::fast());
    CHECK(k3 && !k1->constantTimeEquals(*k3));
}

TEST(vault_format_roundtrip) {
    SecureBytes pw("master-password-123");
    auto nv = vaultformat::createHeader(pw, crypto::KdfParams::fast());
    CHECK(nv.has_value());
    SecureBytes payload("{\"schema\":1}");
    SecureBytes file = vaultformat::encodeVaultFile(nv->header, nv->vaultKey, payload);
    CHECK(file.size() == vaultformat::kHeaderSize + crypto::kNonceBytes + payload.size() + crypto::kTagBytes);

    auto hdr = vaultformat::parseHeader(file.data(), file.size());
    CHECK(hdr.has_value());
    auto key = vaultformat::unwrapVaultKey(*hdr, pw);
    CHECK(key.has_value());
    CHECK(key && key->constantTimeEquals(nv->vaultKey));
    auto body = vaultformat::decodeVaultBody(*hdr, *key, file.data(), file.size());
    CHECK(body && body->constantTimeEquals(payload));

    SecureBytes wrong("master-password-124");
    CHECK(!vaultformat::unwrapVaultKey(*hdr, wrong));

    // Header tamper (flip a salt byte) must break unwrap.
    file.data()[vaultformat::kSaltOffset] ^= 0xFF;
    auto hdr2 = vaultformat::parseHeader(file.data(), file.size());
    CHECK(hdr2 && !vaultformat::unwrapVaultKey(*hdr2, pw));
    file.data()[vaultformat::kSaltOffset] ^= 0xFF;

    // Bad magic
    file.data()[0] = 'X';
    CHECK(!vaultformat::parseHeader(file.data(), file.size()));
}

TEST(vault_create_open_save) {
    QTemporaryDir dir;
    const QString path = dir.filePath("test.mpv");
    SecureBytes pw("a strong master passphrase");
    {
        Vault v;
        CHECK(v.create(path, pw, crypto::KdfParams::fast()) == VaultError::None);
        CHECK(QFile::exists(path));
        Item login = Item::create(ItemType::Login);
        login.title = "Adobe";
        login.setField("username", "thomas@gmail.com");
        login.setField("password", "hunter2!");
        login.setField("website", "https://adobe.com");
        v.addItem(login);
        Item wallet = Item::create(ItemType::CryptoWallet);
        wallet.title = "Ledger";
        wallet.setField("seedPhrase", bip39::generate(24));
        v.addItem(wallet);
        Folder f = v.addFolder("Work");
        CHECK(!f.id.isEmpty());
        CHECK(v.save() == VaultError::None);
        CHECK(QFile::exists(path + ".bak"));  // second save wrote a backup
        CHECK(!QFile::exists(path + ".tmp"));
    }
    {
        Vault v;
        SecureBytes wrong("a strong master passphrasE");
        CHECK(v.open(path, wrong) == VaultError::WrongPassword);
        CHECK(!v.isUnlocked());
        CHECK(v.open(path, pw) == VaultError::None);
        CHECK(v.isUnlocked());
        CHECK(v.items().size() == 2);
        CHECK(v.folders().size() == 1);
        const Item* a = nullptr;
        for (const Item& i : v.items()) if (i.title == "Adobe") a = &i;
        CHECK(a && a->field("password") == "hunter2!");
        CHECK(a && a->subtitle() == "thomas@gmail.com");

        // change master password
        SecureBytes np("new master passphrase!");
        CHECK(v.changeMasterPassword(wrong, np) == VaultError::WrongPassword);
        CHECK(v.changeMasterPassword(pw, np) == VaultError::None);
        Vault v2;
        CHECK(v2.open(path, pw) == VaultError::WrongPassword);
        CHECK(v2.open(path, np) == VaultError::None);
        CHECK(v2.items().size() == 2);

        // trash lifecycle
        v2.moveToTrash(v2.items()[0].id);
        int trashed = 0;
        for (const Item& i : v2.items()) if (i.trashed) ++trashed;
        CHECK(trashed == 1);
        v2.emptyTrash();
        CHECK(v2.items().size() == 1);
        v2.lock();
        CHECK(!v2.isUnlocked());
        CHECK(v2.items().isEmpty());
    }
    // Corrupt file must be rejected, not crash.
    {
        QFile f(path);
        f.open(QIODevice::ReadWrite);
        f.seek(vaultformat::kHeaderSize + 30);
        f.write("\xFF\xFF\xFF", 3);
        f.close();
        Vault v;
        SecureBytes np("new master passphrase!");
        CHECK(v.open(path, np) == VaultError::InvalidFormat);
    }
}

TEST(bip39_validation) {
    const bip39::Validation ok = bip39::validate(
        "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about");
    CHECK(ok.valid);
    CHECK(ok.wordCount == 12);
    const bip39::Validation bad = bip39::validate(
        "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon");
    CHECK(!bad.valid);
    CHECK(bad.checksumOk == false);
    const bip39::Validation unknown = bip39::validate("abandon foo bar");
    CHECK(!unknown.valid && unknown.unknownWords.contains("foo"));
    for (int n : {12, 15, 18, 21, 24}) {
        const QString m = bip39::generate(n);
        const bip39::Validation v = bip39::validate(m);
        CHECK(v.valid && v.wordCount == n);
    }
    CHECK(bip39::normalize("  Abandon   ABILITY\nable ") == "abandon ability able");
    CHECK(bip39::wordlist().size() == 2048);
    CHECK(bip39::suggestions("zo").contains("zone"));
}

TEST(totp_rfc6238_vectors) {
    // RFC 6238 test key "12345678901234567890" = base32 GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ
    auto p = totp::parse("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ");
    CHECK(p.has_value());
    if (p) {
        p->digits = 8;
        CHECK(totp::code(*p, 59) == "94287082");
        CHECK(totp::code(*p, 1111111109) == "07081804");
        CHECK(totp::code(*p, 1234567890) == "89005924");
        CHECK(totp::code(*p, 20000000000LL) == "65353130");
    }
    auto uri = totp::parse("otpauth://totp/Example:alice@google.com?secret=JBSWY3DPEHPK3PXP&issuer=Example&digits=6&period=30");
    CHECK(uri.has_value() && uri->digits == 6 && uri->period == 30);
    CHECK(!totp::parse("not base32 !!!"));
}

TEST(password_generator) {
    PasswordOptions o;
    o.length = 24;
    const QString p = generatePassword(o);
    CHECK(p.size() == 24);
    bool l = false, u = false, d = false, s = false;
    for (QChar c : p) { l |= c.isLower(); u |= c.isUpper(); d |= c.isDigit(); s |= (!c.isLetterOrNumber()); }
    CHECK(l && u && d && s);
    CHECK(generatePassword(o) != p);
    o.symbols = false; o.uppercase = false; o.digits = false; o.excludeAmbiguous = true;
    const QString lower = generatePassword(o);
    for (QChar c : lower) CHECK(c.isLower() && c != 'l');
    const QString phrase = generatePassphrase(5, "-");
    CHECK(phrase.count('-') == 4);
    CHECK(estimateStrength("password").score == 0);
    CHECK(estimateStrength(p).score >= 3);
}

TEST(autofill_matching) {
    QVector<Item> items;
    Item adobe = Item::create(ItemType::Login);
    adobe.title = "Adobe"; adobe.setField("website", "https://www.adobe.com/login"); adobe.setField("username", "t@x.com"); adobe.setField("password", "pw");
    Item notadobe = Item::create(ItemType::Login);
    notadobe.title = "Not Adobe"; notadobe.setField("website", "notadobe.com");
    Item github = Item::create(ItemType::Login);
    github.title = "GitHub"; github.setField("website", "github.com");
    Item note = Item::create(ItemType::SecureNote);
    note.title = "adobe notes";
    items << adobe << notadobe << github << note;

    ForegroundContext ctx;
    ctx.url = "https://auth.adobe.com/signin";
    ctx.windowTitle = "Sign in - Adobe - Google Chrome";
    auto m = AutofillEngine::match(items, ctx);
    CHECK(!m.isEmpty());
    CHECK(!m.isEmpty() && m[0].itemId == adobe.id && m[0].score >= 100);
    for (const auto& x : m) CHECK(x.itemId != note.id);
    // notadobe must not match by host suffix rule
    bool hasNot = false;
    for (const auto& x : m) if (x.itemId == notadobe.id && x.score >= 100) hasNot = true;
    CHECK(!hasNot);
    CHECK(AutofillEngine::hostMatches("adobe.com", "login.adobe.com"));
    CHECK(!AutofillEngine::hostMatches("adobe.com", "notadobe.com"));
    CHECK(canonicalHost("HTTPS://WWW.Adobe.com/x?y") == "adobe.com");

    const auto actions = AutofillEngine::expandSequence("{USERNAME}{TAB}{PASSWORD}{DELAY 100}{ENTER}", adobe);
    CHECK(actions.size() == 5);
    CHECK(actions[0].kind == AutofillEngine::Action::Text && actions[0].text == "t@x.com");
    CHECK(actions[1].kind == AutofillEngine::Action::Key && actions[1].key == "TAB");
    CHECK(actions[2].text == "pw");
    CHECK(actions[3].kind == AutofillEngine::Action::Delay && actions[3].ms == 100);
    CHECK(actions[4].key == "ENTER");
}

TEST(item_json_roundtrip) {
    Item i = Item::create(ItemType::Card);
    i.title = "Visa";
    i.favorite = true;
    i.setField("number", "4111 1111 1111 1234");
    i.setField("cvv", "123");
    Item j = Item::fromJson(i.toJson());
    CHECK(j.id == i.id && j.type == ItemType::Card && j.title == "Visa" && j.favorite);
    CHECK(j.field("cvv") == "123");
    CHECK(j.subtitle() == QString::fromUtf8("•••• 1234"));
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (!initSecureRuntime()) { std::fprintf(stderr, "sodium init failed\n"); return 2; }
    int failedTests = 0;
    for (const Test& t : tests()) {
        const int before = g_failures;
        std::printf("[ RUN  ] %s\n", t.name);
        t.fn();
        if (g_failures == before) std::printf("[  OK  ] %s\n", t.name);
        else { ++failedTests; std::printf("[ FAIL ] %s\n", t.name); }
    }
    std::printf("\n%d checks, %d failures, %d/%zu tests failed\n", g_checks, g_failures, failedTests, tests().size());
    return g_failures == 0 ? 0 : 1;
}
