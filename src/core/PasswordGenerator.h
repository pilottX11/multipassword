#pragma once
#include <QString>

#include "core/SecureMemory.h"

namespace mp {

struct PasswordOptions {
    int length = 20;
    bool lowercase = true;
    bool uppercase = true;
    bool digits = true;
    bool symbols = true;
    bool excludeAmbiguous = false;  // drop l 1 I O 0 |
    QString customExclude;
};

// Generates a password with the OS CSPRNG (no modulo bias) and guarantees at
// least one character from every enabled class.
QString generatePassword(const PasswordOptions& opts);

// Diceware-style passphrase from the BIP39 wordlist (2048 words = 11 bits
// per word). 6 words ~= 66 bits of entropy.
QString generatePassphrase(int words, const QString& separator = QStringLiteral("-"),
                           bool capitalize = false, bool appendDigit = false);

}  // namespace mp
