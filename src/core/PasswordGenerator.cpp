#include "core/PasswordGenerator.h"

#include <QVector>

#include "core/Bip39.h"
#include "core/Crypto.h"

namespace mp {

static QString filtered(QString set, const PasswordOptions& o) {
    if (o.excludeAmbiguous) {
        for (QChar c : QStringLiteral("l1IO0|`'\"")) set.remove(c);
    }
    for (QChar c : o.customExclude) set.remove(c);
    return set;
}

QString generatePassword(const PasswordOptions& opts) {
    QVector<QString> classes;
    if (opts.lowercase) classes.push_back(filtered(QStringLiteral("abcdefghijklmnopqrstuvwxyz"), opts));
    if (opts.uppercase) classes.push_back(filtered(QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZ"), opts));
    if (opts.digits) classes.push_back(filtered(QStringLiteral("0123456789"), opts));
    if (opts.symbols) classes.push_back(filtered(QStringLiteral("!@#$%^&*()-_=+[]{};:,.<>?/~"), opts));
    classes.removeAll(QString());
    if (classes.isEmpty()) classes.push_back(QStringLiteral("abcdefghijklmnopqrstuvwxyz"));

    const int length = std::clamp(opts.length, 4, 256);
    QString all;
    for (const QString& c : classes) all += c;

    QString out;
    out.reserve(length);
    // Guarantee one char from each class...
    for (const QString& c : classes)
        out.append(c.at(static_cast<int>(crypto::randomUniform(static_cast<quint32>(c.size())))));
    // ...then fill the rest uniformly from the union.
    while (out.size() < length)
        out.append(all.at(static_cast<int>(crypto::randomUniform(static_cast<quint32>(all.size())))));
    // Fisher-Yates shuffle so guaranteed chars are not always first.
    for (int i = out.size() - 1; i > 0; --i) {
        const int j = static_cast<int>(crypto::randomUniform(static_cast<quint32>(i + 1)));
        std::swap(out[i], out[j]);
    }
    return out;
}

QString generatePassphrase(int words, const QString& separator, bool capitalize, bool appendDigit) {
    words = std::clamp(words, 3, 24);
    const QStringList& list = bip39::wordlist();
    QStringList parts;
    for (int i = 0; i < words; ++i) {
        QString w = list.at(static_cast<int>(crypto::randomUniform(static_cast<quint32>(list.size()))));
        if (capitalize) w[0] = w[0].toUpper();
        parts.push_back(w);
    }
    QString out = parts.join(separator);
    if (appendDigit) out += QString::number(crypto::randomUniform(10));
    return out;
}

}  // namespace mp
