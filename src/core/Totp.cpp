#include "core/Totp.h"

#include <QUrl>
#include <QUrlQuery>

#include "core/Crypto.h"

namespace mp::totp {

std::optional<SecureBytes> base32Decode(const QString& input) {
    QString s = input.toUpper();
    s.remove(' ');
    s.remove('-');
    s.remove('=');
    if (s.isEmpty()) return std::nullopt;
    static const QString alphabet = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZ234567");
    SecureBytes out;
    int buffer = 0, bitsLeft = 0;
    for (QChar c : s) {
        const int v = alphabet.indexOf(c);
        if (v < 0) return std::nullopt;
        buffer = (buffer << 5) | v;
        bitsLeft += 5;
        if (bitsLeft >= 8) {
            const std::uint8_t byte = static_cast<std::uint8_t>((buffer >> (bitsLeft - 8)) & 0xFF);
            out.append(&byte, 1);
            bitsLeft -= 8;
        }
    }
    if (out.empty()) return std::nullopt;
    return out;
}

std::optional<Params> parse(const QString& secretOrUri) {
    Params p;
    QString s = secretOrUri.trimmed();
    if (s.isEmpty()) return std::nullopt;
    if (s.startsWith(QStringLiteral("otpauth://"), Qt::CaseInsensitive)) {
        QUrl url(s);
        QUrlQuery q(url);
        auto sec = base32Decode(q.queryItemValue(QStringLiteral("secret")));
        if (!sec) return std::nullopt;
        p.secret = std::move(*sec);
        if (q.hasQueryItem(QStringLiteral("digits"))) p.digits = q.queryItemValue(QStringLiteral("digits")).toInt();
        if (q.hasQueryItem(QStringLiteral("period"))) p.period = q.queryItemValue(QStringLiteral("period")).toInt();
        if (p.digits < 6 || p.digits > 8) p.digits = 6;
        if (p.period < 10 || p.period > 300) p.period = 30;
        return p;
    }
    auto sec = base32Decode(s);
    if (!sec) return std::nullopt;
    p.secret = std::move(*sec);
    return p;
}

QString code(const Params& p, qint64 unixTime) {
    const quint64 counter = static_cast<quint64>(unixTime / p.period);
    std::uint8_t msg[8];
    for (int i = 0; i < 8; ++i) msg[i] = static_cast<std::uint8_t>(counter >> (56 - 8 * i));
    SecureBytes mac = crypto::hmacSha1(p.secret, msg, 8);
    const int offset = mac.data()[19] & 0x0F;
    const quint32 bin = ((mac.data()[offset] & 0x7F) << 24) | (mac.data()[offset + 1] << 16) |
                        (mac.data()[offset + 2] << 8) | mac.data()[offset + 3];
    quint32 mod = 1;
    for (int i = 0; i < p.digits; ++i) mod *= 10;
    return QString::number(bin % mod).rightJustified(p.digits, '0');
}

int secondsRemaining(const Params& p, qint64 unixTime) {
    return p.period - static_cast<int>(unixTime % p.period);
}

}  // namespace mp::totp
