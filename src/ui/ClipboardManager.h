#pragma once
// Copies secrets to the clipboard and clears them again after a timeout,
// but only if the clipboard still holds what we put there (so we never wipe
// something the user copied afterwards).

#include <QObject>
#include <QString>

class QTimer;

namespace mp {

class ClipboardManager : public QObject {
    Q_OBJECT
public:
    explicit ClipboardManager(QObject* parent = nullptr);

    // sensitive=true schedules auto-clear (per Settings::clipboardClearSeconds).
    void copy(const QString& text, bool sensitive, const QString& what = QString());
    void clearNow();

signals:
    void copied(const QString& what, int clearAfterSeconds);
    void cleared();

private:
    QTimer* m_timer;
    QString m_lastHash;
    static QString hashOf(const QString& s);
};

}  // namespace mp
