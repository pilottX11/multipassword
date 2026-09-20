#include "core/Bip39.h"

#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QTextStream>

#include "core/Crypto.h"

namespace mp::bip39 {

const QStringList& wordlist() {
    static const QStringList list = [] {
        QStringList l;
        QFile f(QStringLiteral(":/wordlists/bip39_english.txt"));
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream ts(&f);
            while (!ts.atEnd()) {
                const QString w = ts.readLine().trimmed();
                if (!w.isEmpty()) l.push_back(w);
            }
        }
        return l;
    }();
    return list;
}

static const QHash<QString, int>& index() {
    static const QHash<QString, int> idx = [] {
        QHash<QString, int> h;
        const QStringList& l = wordlist();
        for (int i = 0; i < l.size(); ++i) h.insert(l[i], i);
        return h;
    }();
    return idx;
}

QStringList splitWords(const QString& phrase) {
    static const QRegularExpression ws(QStringLiteral("\\s+"));
    QStringList out;
    for (const QString& w : phrase.trimmed().toLower().split(ws, Qt::SkipEmptyParts))
        out.push_back(w);
    return out;
}

QString normalize(const QString& phrase) {
    return splitWords(phrase).join(' ');
}

Validation validate(const QString& phrase) {
    Validation v;
    const QStringList words = splitWords(phrase);
    v.wordCount = words.size();
    if (words.isEmpty()) {
        v.message = QStringLiteral("Empty seed phrase.");
        return v;
    }
    if (v.wordCount % 3 != 0 || v.wordCount < 12 || v.wordCount > 24) {
        v.message = QStringLiteral("A seed phrase must have 12, 15, 18, 21 or 24 words (has %1).").arg(v.wordCount);
    }
    const auto& idx = index();
    if (idx.isEmpty()) {
        v.message = QStringLiteral("Wordlist not available.");
        return v;
    }
    for (const QString& w : words)
        if (!idx.contains(w)) v.unknownWords.push_back(w);
    if (!v.unknownWords.isEmpty()) {
        v.message = QStringLiteral("Unknown word(s): %1").arg(v.unknownWords.join(", "));
        return v;
    }
    if (!v.message.isEmpty()) return v;

    // Reassemble bits: each word = 11 bits. ENT + CS bits, CS = ENT/32.
    const int totalBits = v.wordCount * 11;
    const int csBits = totalBits / 33;
    const int entBits = totalBits - csBits;
    SecureBytes entropy(static_cast<std::size_t>(entBits / 8));
    // Bit-pack
    std::vector<std::uint8_t> bits(static_cast<std::size_t>(totalBits), 0);
    for (int i = 0; i < words.size(); ++i) {
        const int val = idx.value(words[i]);
        for (int b = 0; b < 11; ++b)
            bits[static_cast<std::size_t>(i * 11 + b)] = static_cast<std::uint8_t>((val >> (10 - b)) & 1);
    }
    for (int i = 0; i < entBits; ++i)
        entropy.data()[i / 8] = static_cast<std::uint8_t>(entropy.data()[i / 8] | (bits[static_cast<std::size_t>(i)] << (7 - (i % 8))));
    SecureBytes hash = crypto::sha256(entropy.data(), entropy.size());
    bool ok = true;
    for (int i = 0; i < csBits; ++i) {
        const int hbit = (hash.data()[i / 8] >> (7 - (i % 8))) & 1;
        if (hbit != bits[static_cast<std::size_t>(entBits + i)]) { ok = false; break; }
    }
    for (auto& b : bits) b = 0;
    v.checksumOk = ok;
    v.valid = ok;
    v.message = ok ? QStringLiteral("Valid BIP-39 seed phrase (%1 words).").arg(v.wordCount)
                   : QStringLiteral("Checksum mismatch — a word is probably wrong or out of order.");
    return v;
}

QString generate(int words) {
    if (words != 12 && words != 15 && words != 18 && words != 21 && words != 24) words = 12;
    const int entBits = words * 11 * 32 / 33;
    SecureBytes entropy = crypto::randomBytes(static_cast<std::size_t>(entBits / 8));
    SecureBytes hash = crypto::sha256(entropy.data(), entropy.size());
    const int csBits = entBits / 32;

    std::vector<std::uint8_t> bits;
    bits.reserve(static_cast<std::size_t>(entBits + csBits));
    for (int i = 0; i < entBits; ++i) bits.push_back(static_cast<std::uint8_t>((entropy.data()[i / 8] >> (7 - (i % 8))) & 1));
    for (int i = 0; i < csBits; ++i) bits.push_back(static_cast<std::uint8_t>((hash.data()[i / 8] >> (7 - (i % 8))) & 1));

    const QStringList& list = wordlist();
    QStringList out;
    for (int w = 0; w < words; ++w) {
        int val = 0;
        for (int b = 0; b < 11; ++b) val = (val << 1) | bits[static_cast<std::size_t>(w * 11 + b)];
        out.push_back(list.at(val));
    }
    for (auto& b : bits) b = 0;
    return out.join(' ');
}

QStringList suggestions(const QString& prefix, int max) {
    QStringList out;
    const QString p = prefix.trimmed().toLower();
    if (p.isEmpty()) return out;
    for (const QString& w : wordlist()) {
        if (w.startsWith(p)) {
            out.push_back(w);
            if (out.size() >= max) break;
        }
    }
    return out;
}

}  // namespace mp::bip39
