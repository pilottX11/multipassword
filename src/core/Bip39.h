#pragma once
// BIP-39 mnemonic support (English wordlist).
//   * validation with checksum verification (12/15/18/21/24 words)
//   * generation from CSPRNG entropy
//   * normalisation (whitespace, case) for storage
//
// Seed phrases are extremely sensitive: they are the private key. The UI
// treats FieldKind::SeedPhrase as masked-by-default and the clipboard
// manager auto-clears it.

#include <QString>
#include <QStringList>

#include "core/SecureMemory.h"

namespace mp::bip39 {

const QStringList& wordlist();  // 2048 words, loaded from resources

struct Validation {
    bool valid = false;
    int wordCount = 0;
    QStringList unknownWords;
    bool checksumOk = false;
    QString message;
};

// Splits on any whitespace, lowercases, and validates.
Validation validate(const QString& phrase);
QStringList splitWords(const QString& phrase);
QString normalize(const QString& phrase);  // single-space joined, lowercase

// Generates a new mnemonic with `words` in {12, 15, 18, 21, 24}.
QString generate(int words = 12);

// Suggests wordlist entries with the given prefix (for edit auto-complete).
QStringList suggestions(const QString& prefix, int max = 8);

}  // namespace mp::bip39
