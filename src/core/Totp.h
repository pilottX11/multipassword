#pragma once
// RFC 6238 TOTP (SHA-1, 6 digits, 30 s) for stored 2FA secrets.
// Accepts raw base32 secrets ("JBSWY3DPEHPK3PXP") or otpauth:// URIs.

#include <QString>
#include <optional>

#include "core/SecureMemory.h"

namespace mp::totp {

struct Params {
    SecureBytes secret;   // raw key bytes
    int digits = 6;
    int period = 30;
};

std::optional<Params> parse(const QString& secretOrUri);
std::optional<SecureBytes> base32Decode(const QString& s);

// Code for the given unix time (seconds). Returns zero-padded string.
QString code(const Params& p, qint64 unixTime);
int secondsRemaining(const Params& p, qint64 unixTime);

}  // namespace mp::totp
