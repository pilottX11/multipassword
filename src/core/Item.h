#pragma once
// Vault data model.
//
// An Item is a typed record (Login, Card, Identity, Secure Note, Crypto
// Wallet). Every item has a common envelope (id, title, favourite, folder,
// timestamps) plus a map of typed fields. Sensitive fields are flagged so
// the UI masks them and the autofill engine knows what to type.

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QVector>

namespace mp {

enum class ItemType {
    Login,
    Card,
    Identity,
    SecureNote,
    CryptoWallet,
};

QString itemTypeKey(ItemType t);           // stable string used on disk
QString itemTypeLabel(ItemType t);         // human label: "Login", "Crypto Wallet"
ItemType itemTypeFromKey(const QString& k);
QList<ItemType> allItemTypes();

enum class FieldKind {
    Text,
    Secret,     // masked in UI
    Url,
    Email,
    Multiline,
    SeedPhrase, // masked, word-grid display, BIP39 validated
    Totp,       // stored secret, rendered as rotating code
    Date,
    Number,
};

struct FieldDef {
    QString key;      // storage key, e.g. "username"
    QString label;    // "Username"
    FieldKind kind;
    bool autofill = false;  // participates in auto-type sequence
};

// Ordered field schema for a given item type.
const QVector<FieldDef>& fieldSchema(ItemType type);

struct Folder {
    QString id;
    QString name;

    QJsonObject toJson() const;
    static Folder fromJson(const QJsonObject& o);
};

struct Item {
    QString id;
    ItemType type = ItemType::Login;
    QString title;
    QString folderId;
    bool favorite = false;
    bool trashed = false;
    QDateTime created;
    QDateTime modified;
    QDateTime passwordChanged;
    // Field values keyed by FieldDef::key. Also allows custom user fields
    // (keys prefixed with "custom:").
    QMap<QString, QString> fields;

    QString field(const QString& key) const { return fields.value(key); }
    void setField(const QString& key, const QString& value) { fields[key] = value; }

    // Secondary line shown in list (username / email / card number etc.)
    QString subtitle() const;
    // Best "website" value for matching/autofill; may be empty.
    QString website() const;

    QJsonObject toJson() const;
    static Item fromJson(const QJsonObject& o);

    static Item create(ItemType type);
    static QString newId();
};

// Extracts a canonical host from a URL-ish string: "https://www.Adobe.com/x"
// -> "adobe.com". Non-URL strings are lowercased & trimmed.
QString canonicalHost(const QString& urlOrHost);

}  // namespace mp
