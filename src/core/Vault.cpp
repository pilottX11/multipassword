#include "core/Vault.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <sodium.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace mp {

QString vaultErrorMessage(VaultError e) {
    switch (e) {
        case VaultError::None: return {};
        case VaultError::FileNotFound: return QStringLiteral("Vault file not found.");
        case VaultError::IoError: return QStringLiteral("Could not read or write the vault file.");
        case VaultError::InvalidFormat: return QStringLiteral("The file is not a valid multipassword vault.");
        case VaultError::WrongPassword: return QStringLiteral("Incorrect master password.");
        case VaultError::OutOfMemory: return QStringLiteral("Not enough memory to derive the key.");
        case VaultError::Locked: return QStringLiteral("The vault is locked.");
    }
    return {};
}

Vault::Vault(QObject* parent) : QObject(parent) {}

Vault::~Vault() { lock(); }

void Vault::resetState() {
    m_vaultKey.clear();
    for (Item& it : m_items) {
        for (auto& v : it.fields) wipe(v);
        wipe(it.title);
    }
    m_items.clear();
    m_folders.clear();
    wipeObject(m_header);
    m_dirty = false;
}

VaultError Vault::create(const QString& path, const SecureBytes& password,
                         const crypto::KdfParams& params) {
    lock();
    auto nv = vaultformat::createHeader(password, params);
    if (!nv) return VaultError::OutOfMemory;
    m_header = nv->header;
    m_vaultKey = std::move(nv->vaultKey);
    m_path = path;
    m_unlocked = true;
    m_dirty = true;
    VaultError err = save();
    if (err != VaultError::None) {
        lock();
        return err;
    }
    emit lockedChanged(true);
    emit changed();
    return VaultError::None;
}

VaultError Vault::open(const QString& path, const SecureBytes& password) {
    lock();
    QFile f(path);
    if (!f.exists()) return VaultError::FileNotFound;
    if (!f.open(QIODevice::ReadOnly)) return VaultError::IoError;
    QByteArray raw = f.readAll();
    f.close();
    SecureBytes blob = SecureBytes::fromQByteArray(raw);
    wipe(raw);

    auto header = vaultformat::parseHeader(blob.data(), blob.size());
    if (!header) return VaultError::InvalidFormat;

    auto key = vaultformat::unwrapVaultKey(*header, password);
    if (!key) {
        // Distinguish OOM from wrong password by re-checking the KDF alone.
        auto probe = crypto::deriveKey(password, header->salt, crypto::kSaltBytes, header->kdf);
        return probe ? VaultError::WrongPassword : VaultError::OutOfMemory;
    }

    auto payload = vaultformat::decodeVaultBody(*header, *key, blob.data(), blob.size());
    if (!payload) return VaultError::InvalidFormat;

    m_header = *header;
    m_vaultKey = std::move(*key);
    m_path = path;
    if (!loadPayload(*payload)) {
        resetState();
        return VaultError::InvalidFormat;
    }
    m_unlocked = true;
    m_dirty = false;
    emit lockedChanged(true);
    emit changed();
    return VaultError::None;
}

VaultError Vault::save() {
    if (!m_unlocked) return VaultError::Locked;
    SecureBytes payload = serializePayload();
    SecureBytes file = vaultformat::encodeVaultFile(m_header, m_vaultKey, payload);
    VaultError err = writeFileAtomically(file);
    if (err == VaultError::None) {
        m_dirty = false;
        emit savedToDisk();
    }
    return err;
}

VaultError Vault::writeFileAtomically(const SecureBytes& data) {
    QFileInfo info(m_path);
    QDir dir = info.dir();
    if (!dir.exists() && !dir.mkpath(".")) return VaultError::IoError;

    // 1. write to a temp file in the same directory
    const QString tmpPath = m_path + ".tmp";
    {
        QFile tmp(tmpPath);
        if (!tmp.open(QIODevice::WriteOnly | QIODevice::Truncate)) return VaultError::IoError;
        const qint64 n = tmp.write(reinterpret_cast<const char*>(data.data()),
                                   static_cast<qint64>(data.size()));
        if (n != static_cast<qint64>(data.size())) { tmp.close(); QFile::remove(tmpPath); return VaultError::IoError; }
        if (!tmp.flush()) { tmp.close(); QFile::remove(tmpPath); return VaultError::IoError; }
#ifdef _WIN32
        FlushFileBuffers(reinterpret_cast<HANDLE>(_get_osfhandle(tmp.handle())));
#else
        ::fsync(tmp.handle());
#endif
        tmp.close();
    }

    // 2. keep the previous vault as .bak, then atomically move tmp -> vault
#ifdef _WIN32
    const std::wstring wTmp = tmpPath.toStdWString();
    const std::wstring wDst = m_path.toStdWString();
    const std::wstring wBak = (m_path + ".bak").toStdWString();
    if (QFile::exists(m_path)) {
        // ReplaceFile is atomic and preserves ACLs; it also writes the backup.
        if (!ReplaceFileW(wDst.c_str(), wTmp.c_str(), wBak.c_str(),
                          REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr)) {
            // Fallback: MoveFileEx with replace
            if (!MoveFileExW(wTmp.c_str(), wDst.c_str(),
                             MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                QFile::remove(tmpPath);
                return VaultError::IoError;
            }
        }
    } else {
        if (!MoveFileExW(wTmp.c_str(), wDst.c_str(), MOVEFILE_WRITE_THROUGH)) {
            QFile::remove(tmpPath);
            return VaultError::IoError;
        }
    }
#else
    if (QFile::exists(m_path)) {
        QFile::remove(m_path + ".bak");
        QFile::copy(m_path, m_path + ".bak");
    }
    if (::rename(tmpPath.toLocal8Bit().constData(), m_path.toLocal8Bit().constData()) != 0) {
        QFile::remove(tmpPath);
        return VaultError::IoError;
    }
    int dfd = ::open(dir.absolutePath().toLocal8Bit().constData(), O_RDONLY);
    if (dfd >= 0) { ::fsync(dfd); ::close(dfd); }
#endif
    return VaultError::None;
}

VaultError Vault::changeMasterPassword(const SecureBytes& current, const SecureBytes& next) {
    if (!m_unlocked) return VaultError::Locked;
    // Verify the current password by unwrapping the key again.
    auto check = vaultformat::unwrapVaultKey(m_header, current);
    if (!check) return VaultError::WrongPassword;
    if (!check->constantTimeEquals(m_vaultKey)) return VaultError::WrongPassword;

    auto newHeader = vaultformat::rewrapHeader(m_vaultKey, next, crypto::KdfParams::recommended());
    if (!newHeader) return VaultError::OutOfMemory;
    vaultformat::Header old = m_header;
    m_header = *newHeader;
    VaultError err = save();
    if (err != VaultError::None) m_header = old;
    wipeObject(old);
    return err;
}

void Vault::lock() {
    const bool was = m_unlocked;
    m_unlocked = false;
    resetState();
    if (was) {
        emit lockedChanged(false);
        emit changed();
    }
}

// --- items --------------------------------------------------------------------

const Item* Vault::findItem(const QString& id) const {
    for (const Item& i : m_items)
        if (i.id == id) return &i;
    return nullptr;
}

Item* Vault::findItemMutable(const QString& id) {
    for (Item& i : m_items)
        if (i.id == id) return &i;
    return nullptr;
}

void Vault::addItem(const Item& item) {
    Item copy = item;
    if (copy.id.isEmpty()) copy.id = Item::newId();
    copy.modified = QDateTime::currentDateTimeUtc();
    if (!copy.created.isValid()) copy.created = copy.modified;
    m_items.push_back(copy);
    markDirty();
}

void Vault::updateItem(const Item& item) {
    Item* existing = findItemMutable(item.id);
    if (!existing) { addItem(item); return; }
    if (existing->field("password") != item.field("password"))
        existing->passwordChanged = QDateTime::currentDateTimeUtc();
    const QDateTime created = existing->created;
    *existing = item;
    existing->created = created;
    existing->modified = QDateTime::currentDateTimeUtc();
    markDirty();
}

void Vault::setFavorite(const QString& id, bool fav) {
    if (Item* i = findItemMutable(id)) {
        if (i->favorite == fav) return;
        i->favorite = fav;
        markDirty();
    }
}

void Vault::moveToTrash(const QString& id) {
    if (Item* i = findItemMutable(id)) {
        i->trashed = true;
        i->favorite = false;
        i->modified = QDateTime::currentDateTimeUtc();
        markDirty();
    }
}

void Vault::restoreFromTrash(const QString& id) {
    if (Item* i = findItemMutable(id)) {
        i->trashed = false;
        i->modified = QDateTime::currentDateTimeUtc();
        markDirty();
    }
}

void Vault::deletePermanently(const QString& id) {
    for (int k = 0; k < m_items.size(); ++k) {
        if (m_items[k].id == id) {
            for (auto& v : m_items[k].fields) wipe(v);
            m_items.removeAt(k);
            markDirty();
            return;
        }
    }
}

void Vault::emptyTrash() {
    bool any = false;
    for (int k = m_items.size() - 1; k >= 0; --k) {
        if (m_items[k].trashed) {
            for (auto& v : m_items[k].fields) wipe(v);
            m_items.removeAt(k);
            any = true;
        }
    }
    if (any) markDirty();
}

// --- folders ------------------------------------------------------------------

const Folder* Vault::findFolder(const QString& id) const {
    for (const Folder& f : m_folders)
        if (f.id == id) return &f;
    return nullptr;
}

Folder Vault::addFolder(const QString& name) {
    Folder f;
    f.id = Item::newId();
    f.name = name.trimmed();
    m_folders.push_back(f);
    markDirty();
    return f;
}

void Vault::renameFolder(const QString& id, const QString& name) {
    for (Folder& f : m_folders) {
        if (f.id == id) { f.name = name.trimmed(); markDirty(); return; }
    }
}

void Vault::removeFolder(const QString& id) {
    for (int k = 0; k < m_folders.size(); ++k) {
        if (m_folders[k].id == id) {
            m_folders.removeAt(k);
            for (Item& i : m_items)
                if (i.folderId == id) i.folderId.clear();
            markDirty();
            return;
        }
    }
}

// --- serialization ------------------------------------------------------------

SecureBytes Vault::serializePayload() const {
    QJsonObject root;
    root["schema"] = 1;
    root["app"] = "multipassword";
    QJsonArray folders;
    for (const Folder& f : m_folders) folders.push_back(f.toJson());
    root["folders"] = folders;
    QJsonArray items;
    for (const Item& i : m_items) items.push_back(i.toJson());
    root["items"] = items;
    QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Compact);
    SecureBytes out = SecureBytes::fromQByteArray(json);
    wipe(json);
    return out;
}

bool Vault::loadPayload(const SecureBytes& json) {
    QByteArray raw = json.toQByteArray();
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    wipe(raw);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return false;
    QJsonObject root = doc.object();
    if (root.value("schema").toInt(0) != 1) return false;
    m_items.clear();
    m_folders.clear();
    for (const auto& v : root.value("folders").toArray())
        m_folders.push_back(Folder::fromJson(v.toObject()));
    for (const auto& v : root.value("items").toArray())
        m_items.push_back(Item::fromJson(v.toObject()));
    return true;
}

std::optional<SecureBytes> Vault::exportEncrypted() const {
    if (!m_unlocked) return std::nullopt;
    return vaultformat::encodeVaultFile(m_header, m_vaultKey, serializePayload());
}

void Vault::markDirty() {
    m_dirty = true;
    emit changed();
}

QString defaultVaultPath() {
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) base = QDir::homePath() + "/.multipassword";
    return QDir(base).filePath("vault.mpv");
}

}  // namespace mp
