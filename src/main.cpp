// multipassword — entry point.

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QStyleFactory>

#include "core/SecureMemory.h"
#include "core/Settings.h"
#include "core/Vault.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

int main(int argc, char* argv[]) {
    QApplication::setAttribute(Qt::AA_DontUseNativeMenuBar);
    QApplication app(argc, argv);
    QApplication::setApplicationName("multipassword");
    QApplication::setOrganizationName("multipassword");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setQuitOnLastWindowClosed(false);

    if (!mp::initSecureRuntime()) {
        QMessageBox::critical(nullptr, "multipassword", "Failed to initialise the cryptographic runtime (libsodium).");
        return 1;
    }

    // Single instance: a second launch just raises the first one (the global
    // hotkey can only be owned by one process anyway).
    const QString serverName = QStringLiteral("multipassword-%1").arg(qEnvironmentVariable("USERNAME", "user"));
    {
        QLocalSocket probe;
        probe.connectToServer(serverName);
        if (probe.waitForConnected(200)) {
            probe.write("raise");
            probe.waitForBytesWritten(200);
            return 0;
        }
    }
    QLocalServer::removeServer(serverName);
    QLocalServer server;
    server.setSocketOptions(QLocalServer::UserAccessOption);
    server.listen(serverName);

    app.setStyle(QStyleFactory::create("Fusion"));
    QFont f = app.font();
    f.setFamily(mp::theme::fontFamily().section(',', 0, 0));
    f.setPixelSize(13);
    app.setFont(f);
    app.setStyleSheet(mp::theme::stylesheet());

    // Optional: multipassword --vault <file.mpv> opens/creates that vault and
    // remembers it as the default.
    {
        QCommandLineParser parser;
        parser.setApplicationDescription("multipassword - encrypted password & crypto seed manager");
        parser.addHelpOption();
        parser.addVersionOption();
        QCommandLineOption vaultOpt(QStringLiteral("vault"), "Vault file to open or create.", "file");
        parser.addOption(vaultOpt);
        parser.process(app);
        if (parser.isSet(vaultOpt)) {
            const QString p = QFileInfo(parser.value(vaultOpt)).absoluteFilePath();
            mp::Settings::instance().setVaultPath(p);
        }
    }

    // Make sure the vault directory exists and is private to the user.
    QFileInfo vaultInfo(mp::Settings::instance().vaultPath());
    QDir().mkpath(vaultInfo.absolutePath());

    mp::Vault vault;
    mp::MainWindow window(&vault);

    QObject::connect(&server, &QLocalServer::newConnection, &window, [&] {
        if (QLocalSocket* s = server.nextPendingConnection()) {
            s->deleteLater();
            window.showNormal();
            window.raise();
            window.activateWindow();
        }
    });

    if (!window.unlockInteractive()) return 0;
    window.show();
    return app.exec();
}
