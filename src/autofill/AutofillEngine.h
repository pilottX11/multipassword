#pragma once
// Matches vault items against the currently focused application/window so
// the right credentials can be auto-typed.
//
// Matching rules (in order of confidence):
//   1. Item website host == host of the browser URL (exact / subdomain)
//   2. Item website host appears in the window title
//   3. Item title (lower-cased) appears in the window title / URL
//
// Nothing here talks to the OS; see AutoType for the platform layer.

#include <QString>
#include <QVector>

#include "core/Item.h"

namespace mp {

struct ForegroundContext {
    QString windowTitle;
    QString processName;   // "chrome.exe"
    QString url;           // best-effort, empty if not a browser
    quintptr nativeHandle = 0;
};

struct AutofillMatch {
    QString itemId;
    int score = 0;        // higher = better
    QString reason;
};

class AutofillEngine {
public:
    static QVector<AutofillMatch> match(const QVector<Item>& items, const ForegroundContext& ctx);

    // Expands a sequence like "{USERNAME}{TAB}{PASSWORD}{ENTER}" into a list
    // of typing actions. Unknown placeholders are looked up in item.fields.
    struct Action {
        enum Kind { Text, Key, Delay } kind = Text;
        QString text;   // for Text
        QString key;    // for Key: TAB, ENTER, SPACE, ESC, BACKSPACE
        int ms = 0;     // for Delay
    };
    static QVector<Action> expandSequence(const QString& sequence, const Item& item);

    // Host-suffix match: "login.adobe.com" matches "adobe.com" but not
    // "notadobe.com".
    static bool hostMatches(const QString& itemHost, const QString& pageHost);
};

}  // namespace mp
