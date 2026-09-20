#include "core/PasswordStrength.h"

#include <QSet>
#include <cmath>
#include <cstring>

namespace mp {

static const char* kCommon[] = {
    "password", "123456", "12345678", "qwerty", "abc123", "letmein", "welcome",
    "admin", "iloveyou", "monkey", "dragon", "football", "baseball", "master",
    "sunshine", "princess", "login", "passw0rd", "shadow", "trustno1", "111111",
    "000000", "qwertyuiop", "asdfgh", "zxcvbn", "secret", "hello", "freedom",
    "whatever", "michael", "jennifer", "superman", "batman", "starwars", "bitcoin",
    "ethereum", "wallet", "crypto", "seed", "multipassword",
};

StrengthResult estimateStrength(const QString& password) {
    StrengthResult r;
    if (password.isEmpty()) {
        r.label = QStringLiteral("Empty");
        return r;
    }
    bool lower = false, upper = false, digit = false, symbol = false, unicode = false;
    for (QChar c : password) {
        if (c.isLower()) lower = true;
        else if (c.isUpper()) upper = true;
        else if (c.isDigit()) digit = true;
        else if (c.unicode() < 128) symbol = true;
        else unicode = true;
    }
    int pool = 0;
    if (lower) pool += 26;
    if (upper) pool += 26;
    if (digit) pool += 10;
    if (symbol) pool += 33;
    if (unicode) pool += 100;
    if (pool == 0) pool = 26;

    double bits = password.size() * std::log2(static_cast<double>(pool));

    // Penalties -------------------------------------------------------------
    const QString lowerPw = password.toLower();
    for (const char* w : kCommon) {
        if (lowerPw.contains(QLatin1String(w))) {
            bits -= std::log2(26.0) * static_cast<double>(std::strlen(w)) * 0.8;
            r.warning = QStringLiteral("Contains a very common word.");
        }
    }
    // Repeated characters ("aaaa")
    int repeats = 0;
    for (int i = 1; i < password.size(); ++i)
        if (password[i] == password[i - 1]) ++repeats;
    bits -= repeats * 2.5;
    // Sequences ("abcd", "1234", "dcba")
    int seq = 0;
    for (int i = 2; i < password.size(); ++i) {
        const int d1 = password[i].unicode() - password[i - 1].unicode();
        const int d2 = password[i - 1].unicode() - password[i - 2].unicode();
        if ((d1 == 1 && d2 == 1) || (d1 == -1 && d2 == -1)) ++seq;
    }
    bits -= seq * 2.0;
    // Keyboard walks
    static const QStringList walks = {"qwerty", "asdfgh", "zxcvbn", "qwertz", "azerty", "1qaz", "2wsx"};
    for (const QString& w : walks)
        if (lowerPw.contains(w)) bits -= 8;
    // Low unique-char ratio
    QSet<QChar> uniq;
    for (QChar c : password) uniq.insert(c);
    if (uniq.size() < password.size() / 2) bits *= 0.7;

    if (bits < 0) bits = 0;
    r.entropyBits = bits;

    if (bits < 28) { r.score = 0; r.label = QStringLiteral("Very weak"); }
    else if (bits < 40) { r.score = 1; r.label = QStringLiteral("Weak"); }
    else if (bits < 60) { r.score = 2; r.label = QStringLiteral("Fair"); }
    else if (bits < 80) { r.score = 3; r.label = QStringLiteral("Strong"); }
    else { r.score = 4; r.label = QStringLiteral("Very strong"); }

    if (r.warning.isEmpty()) {
        if (password.size() < 12) r.warning = QStringLiteral("Use at least 12 characters.");
        else if (!(lower && upper && digit && symbol)) r.warning = QStringLiteral("Mix upper/lower case, digits and symbols.");
    }
    return r;
}

}  // namespace mp
