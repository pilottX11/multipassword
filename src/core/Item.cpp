#include "core/Item.h"

#include <QJsonArray>
#include <QUrl>
#include <QUuid>

namespace mp {

QString itemTypeKey(ItemType t) {
    switch (t) {
        case ItemType::Login: return QStringLiteral("login");
        case ItemType::Card: return QStringLiteral("card");
        case ItemType::Identity: return QStringLiteral("identity");
        case ItemType::SecureNote: return QStringLiteral("note");
        case ItemType::CryptoWallet: return QStringLiteral("wallet");
    }
    return QStringLiteral("login");
}

QString itemTypeLabel(ItemType t) {
    switch (t) {
        case ItemType::Login: return QStringLiteral("Login");
        case ItemType::Card: return QStringLiteral("Card");
        case ItemType::Identity: return QStringLiteral("Identity");
        case ItemType::SecureNote: return QStringLiteral("Secure Note");
        case ItemType::CryptoWallet: return QStringLiteral("Crypto Wallet");
    }
    return QStringLiteral("Login");
}

ItemType itemTypeFromKey(const QString& k) {
    for (ItemType t : allItemTypes())
        if (itemTypeKey(t) == k) return t;
    return ItemType::Login;
}

QList<ItemType> allItemTypes() {
    return {ItemType::Login, ItemType::Card, ItemType::Identity,
            ItemType::SecureNote, ItemType::CryptoWallet};
}

const QVector<FieldDef>& fieldSchema(ItemType type) {
    static const QVector<FieldDef> login = {
        {"username", "Username", FieldKind::Text, true},
        {"password", "Password", FieldKind::Secret, true},
        {"website", "Website", FieldKind::Url, false},
        {"totp", "One-time password (TOTP)", FieldKind::Totp, false},
        {"notes", "Notes", FieldKind::Multiline, false},
    };
    static const QVector<FieldDef> card = {
        {"cardholder", "Cardholder name", FieldKind::Text, true},
        {"number", "Card number", FieldKind::Secret, true},
        {"expiry", "Expiry (MM/YY)", FieldKind::Text, true},
        {"cvv", "Security code (CVV)", FieldKind::Secret, true},
        {"pin", "PIN", FieldKind::Secret, false},
        {"notes", "Notes", FieldKind::Multiline, false},
    };
    static const QVector<FieldDef> identity = {
        {"firstName", "First name", FieldKind::Text, true},
        {"lastName", "Last name", FieldKind::Text, true},
        {"email", "Email", FieldKind::Email, true},
        {"phone", "Phone", FieldKind::Text, true},
        {"address", "Address", FieldKind::Multiline, false},
        {"birthday", "Date of birth", FieldKind::Date, false},
        {"idNumber", "ID / Passport number", FieldKind::Secret, false},
        {"notes", "Notes", FieldKind::Multiline, false},
    };
    static const QVector<FieldDef> note = {
        {"notes", "Notes", FieldKind::Multiline, false},
    };
    static const QVector<FieldDef> wallet = {
        {"network", "Network / Chain", FieldKind::Text, false},
        {"address", "Public address", FieldKind::Text, false},
        {"seedPhrase", "Recovery seed phrase", FieldKind::SeedPhrase, false},
        {"passphrase", "BIP39 passphrase (25th word)", FieldKind::Secret, false},
        {"privateKey", "Private key", FieldKind::Secret, false},
        {"derivationPath", "Derivation path", FieldKind::Text, false},
        {"walletPassword", "Wallet app password", FieldKind::Secret, false},
        {"notes", "Notes", FieldKind::Multiline, false},
    };
    switch (type) {
        case ItemType::Login: return login;
        case ItemType::Card: return card;
        case ItemType::Identity: return identity;
        case ItemType::SecureNote: return note;
        case ItemType::CryptoWallet: return wallet;
    }
    return login;
}

QJsonObject Folder::toJson() const {
    return {{"id", id}, {"name", name}};
}

Folder Folder::fromJson(const QJsonObject& o) {
    Folder f;
    f.id = o.value("id").toString();
    f.name = o.value("name").toString();
    return f;
}

QString Item::subtitle() const {
    switch (type) {
        case ItemType::Login: return field("username");
        case ItemType::Card: {
            QString n = field("number");
            n.remove(' ');
            if (n.size() >= 4) return QStringLiteral("•••• ") + n.right(4);
            return field("cardholder");
        }
        case ItemType::Identity: {
            QString e = field("email");
            if (!e.isEmpty()) return e;
            return (field("firstName") + " " + field("lastName")).trimmed();
        }
        case ItemType::SecureNote: {
            QString n = field("notes");
            n = n.section('\n', 0, 0).trimmed();
            return n.left(40);
        }
        case ItemType::CryptoWallet: {
            QString a = field("address");
            if (a.size() > 14) return a.left(6) + "…" + a.right(4);
            if (!a.isEmpty()) return a;
            return field("network");
        }
    }
    return {};
}

QString Item::website() const {
    return field("website");
}

QJsonObject Item::toJson() const {
    QJsonObject o;
    o["id"] = id;
    o["type"] = itemTypeKey(type);
    o["title"] = title;
    o["folderId"] = folderId;
    o["favorite"] = favorite;
    o["trashed"] = trashed;
    o["created"] = created.toUTC().toString(Qt::ISODateWithMs);
    o["modified"] = modified.toUTC().toString(Qt::ISODateWithMs);
    if (passwordChanged.isValid())
        o["passwordChanged"] = passwordChanged.toUTC().toString(Qt::ISODateWithMs);
    QJsonObject f;
    for (auto it = fields.constBegin(); it != fields.constEnd(); ++it) f[it.key()] = it.value();
    o["fields"] = f;
    return o;
}

Item Item::fromJson(const QJsonObject& o) {
    Item i;
    i.id = o.value("id").toString();
    if (i.id.isEmpty()) i.id = newId();
    i.type = itemTypeFromKey(o.value("type").toString());
    i.title = o.value("title").toString();
    i.folderId = o.value("folderId").toString();
    i.favorite = o.value("favorite").toBool(false);
    i.trashed = o.value("trashed").toBool(false);
    i.created = QDateTime::fromString(o.value("created").toString(), Qt::ISODateWithMs);
    i.modified = QDateTime::fromString(o.value("modified").toString(), Qt::ISODateWithMs);
    i.passwordChanged = QDateTime::fromString(o.value("passwordChanged").toString(), Qt::ISODateWithMs);
    if (!i.created.isValid()) i.created = QDateTime::currentDateTimeUtc();
    if (!i.modified.isValid()) i.modified = i.created;
    const QJsonObject f = o.value("fields").toObject();
    for (auto it = f.constBegin(); it != f.constEnd(); ++it) i.fields[it.key()] = it.value().toString();
    return i;
}

Item Item::create(ItemType type) {
    Item i;
    i.id = newId();
    i.type = type;
    i.created = QDateTime::currentDateTimeUtc();
    i.modified = i.created;
    return i;
}

QString Item::newId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString canonicalHost(const QString& urlOrHost) {
    QString s = urlOrHost.trimmed();
    if (s.isEmpty()) return {};
    QString probe = s;
    if (!probe.contains("://")) probe.prepend("https://");
    QUrl u(probe);
    QString host = u.host().toLower();
    if (host.isEmpty()) host = s.toLower();
    if (host.startsWith("www.")) host = host.mid(4);
    return host;
}

}  // namespace mp
