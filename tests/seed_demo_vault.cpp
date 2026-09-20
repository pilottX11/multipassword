// Developer tool: creates a vault pre-filled with sample data so the UI can
// be exercised without typing. NOT part of the shipped application.
//   mp_seed_demo <vault.mpv> <master-password>
#include <QCoreApplication>
#include <cstdio>

#include "core/Bip39.h"
#include "core/SecureMemory.h"
#include "core/Vault.h"

using namespace mp;

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc < 3) { std::fprintf(stderr, "usage: mp_seed_demo <vault.mpv> <password>\n"); return 2; }
    if (!initSecureRuntime()) return 3;
    Vault v;
    SecureBytes pw{std::string_view(argv[2])};
    if (v.create(QString::fromLocal8Bit(argv[1]), pw, crypto::KdfParams::fast()) != VaultError::None) {
        std::fprintf(stderr, "create failed\n");
        return 1;
    }
    const Folder work = v.addFolder("Work");
    const Folder social = v.addFolder("Social");
    const Folder personal = v.addFolder("Personal");

    struct L { const char* title; const char* user; const char* site; const char* folder; bool fav; };
    const L logins[] = {
        {"Adobe", "thomas@gmail.com", "adobe.com", work.id.toUtf8().constData(), true},
        {"Apple", "thomas@icloud.com", "apple.com", personal.id.toUtf8().constData(), false},
        {"Dribbble", "thomas@gmail.com", "dribbble.com", work.id.toUtf8().constData(), false},
        {"Etsy", "thomas@gmail.com", "etsy.com", personal.id.toUtf8().constData(), false},
        {"Facebook", "thomas@gmail.com", "facebook.com", social.id.toUtf8().constData(), false},
        {"Google", "thomas@gmail.com", "google.com", personal.id.toUtf8().constData(), true},
        {"IMDb", "thomas@gmail.com", "imdb.com", personal.id.toUtf8().constData(), false},
        {"InVision", "thomas@gmail.com", "invisionapp.com", work.id.toUtf8().constData(), false},
        {"Telegram", "+1 202 555 0158", "telegram.org", social.id.toUtf8().constData(), false},
        {"GitHub", "thomas-dev", "github.com", work.id.toUtf8().constData(), false},
        {"Netflix", "thomas@gmail.com", "netflix.com", personal.id.toUtf8().constData(), false},
        {"Spotify", "thomas@gmail.com", "spotify.com", personal.id.toUtf8().constData(), false},
    };
    for (const L& l : logins) {
        Item i = Item::create(ItemType::Login);
        i.title = l.title;
        i.folderId = l.folder;
        i.favorite = l.fav;
        i.setField("username", l.user);
        i.setField("password", QStringLiteral("Xk9#mP2$vL8@qR4!"));
        i.setField("website", l.site);
        if (QString(l.title) == "Adobe")
            i.setField("notes", "Great experiences have the power to inspire, transform and move the world forward. And every great experience starts with creativity.");
        if (QString(l.title) == "GitHub") i.setField("totp", "JBSWY3DPEHPK3PXP");
        v.addItem(i);
    }
    Item card = Item::create(ItemType::Card);
    card.title = "Visa Platinum";
    card.folderId = personal.id;
    card.setField("cardholder", "Thomas Anderson");
    card.setField("number", "4111 1111 1111 1234");
    card.setField("expiry", "09/28");
    card.setField("cvv", "123");
    v.addItem(card);
    Item id = Item::create(ItemType::Identity);
    id.title = "Thomas Anderson";
    id.setField("firstName", "Thomas");
    id.setField("lastName", "Anderson");
    id.setField("email", "thomas@gmail.com");
    id.setField("phone", "+1 202 555 0158");
    v.addItem(id);
    Item note = Item::create(ItemType::SecureNote);
    note.title = "Wi-Fi";
    note.setField("notes", "Home network: Matrix-5G\nPassword: follow-the-white-rabbit");
    v.addItem(note);
    Item wallet = Item::create(ItemType::CryptoWallet);
    wallet.title = "Ledger Nano";
    wallet.folderId = personal.id;
    wallet.setField("network", "Bitcoin / Ethereum");
    wallet.setField("address", "0x71C7656EC7ab88b098defB751B7401B5f6d8976F");
    wallet.setField("seedPhrase", bip39::generate(24));
    wallet.setField("derivationPath", "m/44'/60'/0'/0/0");
    v.addItem(wallet);
    if (v.save() != VaultError::None) { std::fprintf(stderr, "save failed\n"); return 1; }
    std::printf("seeded %d items into %s\n", static_cast<int>(v.items().size()), argv[1]);
    return 0;
}
