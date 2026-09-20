#pragma once
// In-memory vault: the decrypted collection of items and folders plus the
// key material needed to save it back to disk.
//
// Lifecycle:  create(path, password)  or  open(path, password)
//             ... edit ...  save() (atomic, fsync'd, keeps a .bak)
//             lock()  -> wipes key + items from memory
//
// The vault key is kept in locked memory (SecureBytes) for the whole time
// the vault is unlocked; the master password itself is never retained.

#include <QObject>
#include <QString>
#include <QVector>
#include <optional>

#include "core/Crypto.h"
#include "core/Item.h"
#include "core/SecureMemory.h"
#include "core/VaultFormat.h"

namespace mp {

enum class VaultError {
    None,
    FileNotFound,
    IoError,
    InvalidFormat,
    WrongPassword,
    OutOfMemory,
    Locked,
};
QString vaultErrorMessage(VaultError e);

class Vault : public QObject {
    Q_OBJECT
public:
    explicit Vault(QObject* parent = nullptr);
    ~Vault() override;

    // --- lifecycle --------------------------------------------------------
    VaultError create(const QString& path, const SecureBytes& password,
                      const crypto::KdfParams& params = crypto::KdfParams::recommended());
    VaultError open(const QString& path, const SecureBytes& password);
    VaultError save();
    VaultError changeMasterPassword(const SecureBytes& current, const SecureBytes& next);
    void lock();

    bool isUnlocked() const { return m_unlocked; }
    QString path() const { return m_path; }
    bool isDirty() const { return m_dirty; }

    // --- items --------------------------------------------------------------
    const QVector<Item>& items() const { return m_items; }
    const Item* findItem(const QString& id) const;
    Item* findItemMutable(const QString& id);
    void addItem(const Item& item);
    void updateItem(const Item& item);
    void setFavorite(const QString& id, bool fav);
    void moveToTrash(const QString& id);
    void restoreFromTrash(const QString& id);
    void deletePermanently(const QString& id);
    void emptyTrash();

    // --- folders ------------------------------------------------------------
    const QVector<Folder>& folders() const { return m_folders; }
    const Folder* findFolder(const QString& id) const;
    Folder addFolder(const QString& name);
    void renameFolder(const QString& id, const QString& name);
    void removeFolder(const QString& id);  // items are moved to "no folder"

    // --- serialization (exposed for tests) ----------------------------------
    SecureBytes serializePayload() const;
    bool loadPayload(const SecureBytes& json);

    // Convenience for tests / import-export: whole encrypted file image.
    std::optional<SecureBytes> exportEncrypted() const;

signals:
    void changed();      // any data change
    void lockedChanged(bool unlocked);
    void savedToDisk();

private:
    void markDirty();
    VaultError writeFileAtomically(const SecureBytes& data);
    void resetState();

    QString m_path;
    bool m_unlocked = false;
    bool m_dirty = false;
    vaultformat::Header m_header;
    SecureBytes m_vaultKey;
    QVector<Item> m_items;
    QVector<Folder> m_folders;
};

QString defaultVaultPath();

}  // namespace mp
