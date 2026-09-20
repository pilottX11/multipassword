#include "autofill/AutofillEngine.h"

#include <QRegularExpression>
#include <algorithm>

namespace mp {

bool AutofillEngine::hostMatches(const QString& itemHost, const QString& pageHost) {
    if (itemHost.isEmpty() || pageHost.isEmpty()) return false;
    if (itemHost == pageHost) return true;
    // subdomain: pageHost ends with "." + itemHost
    if (pageHost.endsWith("." + itemHost)) return true;
    // item stored a deeper host than the page ("login.adobe.com" vs "adobe.com")
    if (itemHost.endsWith("." + pageHost)) return true;
    return false;
}

QVector<AutofillMatch> AutofillEngine::match(const QVector<Item>& items, const ForegroundContext& ctx) {
    QVector<AutofillMatch> out;
    const QString title = ctx.windowTitle.toLower();
    const QString pageHost = canonicalHost(ctx.url);

    for (const Item& item : items) {
        if (item.trashed) continue;
        if (item.type != ItemType::Login && item.type != ItemType::Card && item.type != ItemType::Identity)
            continue;
        int score = 0;
        QString reason;
        const QString itemHost = canonicalHost(item.website());
        if (!itemHost.isEmpty()) {
            if (hostMatches(itemHost, pageHost)) {
                score = 100;
                reason = QStringLiteral("URL matches %1").arg(itemHost);
            } else if (!title.isEmpty() && title.contains(itemHost)) {
                score = 60;
                reason = QStringLiteral("Window title mentions %1").arg(itemHost);
            } else {
                // bare brand name from host: "adobe" from "adobe.com"
                const QString brand = itemHost.section('.', 0, 0);
                if (brand.size() >= 3 && title.contains(brand)) {
                    score = 40;
                    reason = QStringLiteral("Window title mentions %1").arg(brand);
                }
            }
        }
        const QString lowTitle = item.title.trimmed().toLower();
        if (score == 0 && lowTitle.size() >= 3 &&
            (title.contains(lowTitle) || pageHost.contains(lowTitle))) {
            score = 30;
            reason = QStringLiteral("Title matches");
        }
        if (score > 0) {
            if (item.favorite) score += 5;
            out.push_back({item.id, score, reason});
        }
    }
    std::stable_sort(out.begin(), out.end(),
                     [](const AutofillMatch& a, const AutofillMatch& b) { return a.score > b.score; });
    return out;
}

QVector<AutofillEngine::Action> AutofillEngine::expandSequence(const QString& sequence, const Item& item) {
    QVector<Action> actions;
    static const QRegularExpression tok(QStringLiteral("\\{([A-Za-z0-9_:\\- ]+)\\}|([^{]+)"));
    auto it = tok.globalMatch(sequence);
    while (it.hasNext()) {
        const auto m = it.next();
        if (!m.captured(2).isEmpty()) {
            actions.push_back({Action::Text, m.captured(2), {}, 0});
            continue;
        }
        const QString name = m.captured(1);
        const QString upper = name.toUpper();
        if (upper == "TAB" || upper == "ENTER" || upper == "SPACE" || upper == "ESC" || upper == "BACKSPACE") {
            actions.push_back({Action::Key, {}, upper, 0});
        } else if (upper.startsWith("DELAY")) {
            // {DELAY 500} or {DELAY:500}
            QString num = name.mid(5);
            num.remove(':'); num.remove(' ');
            actions.push_back({Action::Delay, {}, {}, std::clamp(num.toInt(), 0, 5000)});
        } else if (upper == "USERNAME") {
            QString v = item.field("username");
            if (v.isEmpty()) v = item.field("email");
            if (v.isEmpty()) v = item.field("cardholder");
            actions.push_back({Action::Text, v, {}, 0});
        } else if (upper == "PASSWORD") {
            actions.push_back({Action::Text, item.field("password"), {}, 0});
        } else if (upper == "TITLE") {
            actions.push_back({Action::Text, item.title, {}, 0});
        } else if (upper == "URL") {
            actions.push_back({Action::Text, item.website(), {}, 0});
        } else {
            // generic field lookup, case-insensitive on key
            QString val;
            for (auto f = item.fields.constBegin(); f != item.fields.constEnd(); ++f)
                if (f.key().compare(name, Qt::CaseInsensitive) == 0) { val = f.value(); break; }
            actions.push_back({Action::Text, val, {}, 0});
        }
    }
    return actions;
}

}  // namespace mp
