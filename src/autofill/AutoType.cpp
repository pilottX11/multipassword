#include "autofill/AutoType.h"

#include <QThread>

#include "core/SecureMemory.h"

#ifdef _WIN32
#include <windows.h>
#include <objbase.h>   // COM basics (WIN32_LEAN_AND_MEAN leaves them out)
#include <oleauto.h>
#include <uiautomation.h>
#include <string>
#include <vector>
#endif

namespace mp {

AutoType::AutoType(QObject* parent) : QObject(parent) {}
AutoType::~AutoType() { unregisterHotkey(); }

bool AutoType::isSupported() {
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

#ifdef _WIN32
namespace {

struct ComGuard {
    bool needUninit = false;
    ComGuard() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        needUninit = SUCCEEDED(hr);  // S_OK or S_FALSE both need balancing
    }
    ~ComGuard() { if (needUninit) CoUninitialize(); }
};

template <typename T>
struct ComPtr {
    T* p = nullptr;
    ComPtr() = default;
    ~ComPtr() { if (p) p->Release(); }
    T** operator&() { return &p; }
    T* operator->() const { return p; }
    explicit operator bool() const { return p != nullptr; }
};

QString windowText(HWND hwnd) {
    const int len = GetWindowTextLengthW(hwnd);
    if (len <= 0) return {};
    std::wstring buf(static_cast<std::size_t>(len) + 1, L'\0');
    GetWindowTextW(hwnd, buf.data(), len + 1);
    buf.resize(static_cast<std::size_t>(len));
    return QString::fromStdWString(buf);
}

QString processNameOf(HWND hwnd) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid) return {};
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return {};
    wchar_t path[MAX_PATH * 2];
    DWORD size = static_cast<DWORD>(std::size(path));
    QString name;
    if (QueryFullProcessImageNameW(h, 0, path, &size)) {
        QString full = QString::fromWCharArray(path, static_cast<int>(size));
        name = full.section('\\', -1).toLower();
    }
    CloseHandle(h);
    return name;
}

// Walks the UI Automation tree of a browser window looking for the address
// bar (an Edit control). Chrome/Edge name it "Address and search bar",
// Firefox "Search with Google or enter address" / "Search or enter address",
// Brave/Opera/Vivaldi follow Chromium. We accept any Edit whose value looks
// like a URL as a fallback.
QString browserUrl(HWND hwnd) {
    ComGuard com;
    ComPtr<IUIAutomation> uia;
    if (FAILED(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IUIAutomation, reinterpret_cast<void**>(&uia))) || !uia)
        return {};
    ComPtr<IUIAutomationElement> root;
    if (FAILED(uia->ElementFromHandle(hwnd, &root)) || !root) return {};

    VARIANT editType;
    editType.vt = VT_I4;
    editType.lVal = UIA_EditControlTypeId;
    ComPtr<IUIAutomationCondition> cond;
    if (FAILED(uia->CreatePropertyCondition(UIA_ControlTypePropertyId, editType, &cond)) || !cond) return {};

    ComPtr<IUIAutomationElementArray> edits;
    if (FAILED(root->FindAll(TreeScope_Descendants, cond.p, &edits)) || !edits) return {};
    int count = 0;
    edits->get_Length(&count);

    QString fallback;
    for (int i = 0; i < count && i < 64; ++i) {
        ComPtr<IUIAutomationElement> el;
        if (FAILED(edits->GetElement(i, &el)) || !el) continue;
        BSTR nameB = nullptr;
        el->get_CurrentName(&nameB);
        QString name = nameB ? QString::fromWCharArray(nameB).toLower() : QString();
        if (nameB) SysFreeString(nameB);

        ComPtr<IUIAutomationValuePattern> vp;
        if (FAILED(el->GetCurrentPatternAs(UIA_ValuePatternId, IID_IUIAutomationValuePattern,
                                           reinterpret_cast<void**>(&vp))) || !vp)
            continue;
        BSTR valB = nullptr;
        if (FAILED(vp->get_CurrentValue(&valB)) || !valB) continue;
        QString val = QString::fromWCharArray(valB);
        SysFreeString(valB);
        val = val.trimmed();
        if (val.isEmpty()) continue;

        const bool isAddressBar = name.contains("address") || name.contains("url") ||
                                  name.contains("search or enter");
        if (isAddressBar) return val;
        if (fallback.isEmpty() && (val.contains("://") || val.contains('.')) && !val.contains(' '))
            fallback = val;
    }
    return fallback;
}

bool parseHotkey(const QString& seq, UINT& mods, UINT& vk) {
    mods = 0;
    vk = 0;
    const QStringList parts = seq.split('+', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return false;
    for (int i = 0; i < parts.size(); ++i) {
        const QString p = parts[i].trimmed();
        const QString u = p.toUpper();
        const bool last = (i == parts.size() - 1);
        if (!last) {
            if (u == "CTRL" || u == "CONTROL") mods |= MOD_CONTROL;
            else if (u == "ALT") mods |= MOD_ALT;
            else if (u == "SHIFT") mods |= MOD_SHIFT;
            else if (u == "WIN" || u == "META" || u == "SUPER") mods |= MOD_WIN;
            else return false;
        } else {
            if (u.size() == 1) {
                const QChar c = u.at(0);
                if (c.isLetterOrNumber()) vk = static_cast<UINT>(c.toUpper().unicode());
                else {
                    SHORT scan = VkKeyScanW(static_cast<WCHAR>(c.unicode()));
                    if (scan == -1) return false;
                    vk = static_cast<UINT>(scan & 0xFF);
                }
            } else if (u.startsWith('F') && u.size() <= 3) {
                bool ok = false;
                const int n = u.mid(1).toInt(&ok);
                if (!ok || n < 1 || n > 24) return false;
                vk = VK_F1 + static_cast<UINT>(n - 1);
            } else if (u == "SPACE") vk = VK_SPACE;
            else if (u == "INSERT" || u == "INS") vk = VK_INSERT;
            else if (u == "HOME") vk = VK_HOME;
            else if (u == "END") vk = VK_END;
            else if (u == "PAUSE") vk = VK_PAUSE;
            else return false;
        }
    }
    if (mods == 0) return false;  // refuse un-modified keys: would hijack typing
    return vk != 0;
}

void sendUnicodePacket(wchar_t ch) {
    INPUT in = {};
    in.type = INPUT_KEYBOARD;
    in.ki.wScan = ch;
    in.ki.dwFlags = KEYEVENTF_UNICODE;
    SendInput(1, &in, sizeof(INPUT));
    Sleep(3);
    in.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    SendInput(1, &in, sizeof(INPUT));
}

void sendKeyEvent(WORD vk, bool up) {
    INPUT in = {};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.wScan = static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
    in.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
    SendInput(1, &in, sizeof(INPUT));
}

// Types one character. Characters that exist on the active keyboard layout
// are sent as real virtual-key events (with Shift where needed): the key
// state travels with each message, so a target that lags behind (e.g. a
// spell-checker running at a word boundary) still decodes the right
// character. Anything else (emoji, non-Latin text on a Latin layout) falls
// back to a Unicode VK_PACKET, which some slow targets can mis-read.
void sendUnicodeChar(wchar_t ch) {
    const HKL layout = GetKeyboardLayout(GetWindowThreadProcessId(GetForegroundWindow(), nullptr));
    const SHORT scan = VkKeyScanExW(ch, layout);
    if (scan == -1 || ch == L'\n' || ch == L'\r' || ch == L'\t') {
        if (ch == L'\n') { sendKeyEvent(VK_RETURN, false); Sleep(3); sendKeyEvent(VK_RETURN, true); return; }
        if (ch == L'\t') { sendKeyEvent(VK_TAB, false); Sleep(3); sendKeyEvent(VK_TAB, true); return; }
        if (ch == L'\r') return;
        sendUnicodePacket(ch);
        return;
    }
    const WORD vk = static_cast<WORD>(scan & 0xFF);
    const int mods = (scan >> 8) & 0xFF;  // 1 = Shift, 2 = Ctrl, 4 = Alt
    if (mods & ~1) {
        // Needs Ctrl/Alt (AltGr) on this layout: use the packet route rather
        // than juggling AltGr state.
        sendUnicodePacket(ch);
        return;
    }
    bool shift = mods & 1;
    // Caps Lock inverts the case of letters; compensate so the vault value is
    // typed verbatim.
    if (iswalpha(ch) && (GetKeyState(VK_CAPITAL) & 1)) shift = !shift;
    if (shift) sendKeyEvent(VK_SHIFT, false);
    sendKeyEvent(vk, false);
    Sleep(3);
    sendKeyEvent(vk, true);
    if (shift) sendKeyEvent(VK_SHIFT, true);
}

void sendVk(WORD vk) {
    INPUT in[2] = {};
    in[0].type = INPUT_KEYBOARD;
    in[0].ki.wVk = vk;
    in[1] = in[0];
    in[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, in, sizeof(INPUT));
}

// Release any modifier keys the user may still be holding from the hotkey
// so they do not turn our keystrokes into shortcuts.
void releaseModifiers() {
    for (WORD vk : {VK_CONTROL, VK_LCONTROL, VK_RCONTROL, VK_MENU, VK_LMENU, VK_RMENU,
                    VK_SHIFT, VK_LSHIFT, VK_RSHIFT, VK_LWIN, VK_RWIN}) {
        if (GetAsyncKeyState(vk) & 0x8000) {
            INPUT in = {};
            in.type = INPUT_KEYBOARD;
            in.ki.wVk = vk;
            in.ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(1, &in, sizeof(INPUT));
        }
    }
}

bool forceForeground(HWND target) {
    if (GetForegroundWindow() == target) return true;
    const DWORD fgThread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    const DWORD ourThread = GetCurrentThreadId();
    if (fgThread && fgThread != ourThread) AttachThreadInput(ourThread, fgThread, TRUE);
    if (IsIconic(target)) ShowWindow(target, SW_RESTORE);
    SetForegroundWindow(target);
    BringWindowToTop(target);
    if (fgThread && fgThread != ourThread) AttachThreadInput(ourThread, fgThread, FALSE);
    for (int i = 0; i < 20; ++i) {
        if (GetForegroundWindow() == target) return true;
        Sleep(25);
    }
    return GetForegroundWindow() == target;
}

}  // namespace
#endif  // _WIN32

ForegroundContext AutoType::captureForeground(bool resolveBrowserUrl) {
    ForegroundContext ctx;
#ifdef _WIN32
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return ctx;
    ctx.nativeHandle = reinterpret_cast<quintptr>(hwnd);
    ctx.windowTitle = windowText(hwnd);
    ctx.processName = processNameOf(hwnd);
    static const QStringList browsers = {"chrome.exe", "msedge.exe", "firefox.exe", "brave.exe",
                                         "opera.exe", "vivaldi.exe", "chromium.exe", "librewolf.exe",
                                         "waterfox.exe", "arc.exe", "zen.exe"};
    if (resolveBrowserUrl && browsers.contains(ctx.processName)) ctx.url = browserUrl(hwnd);
#endif
    return ctx;
}

bool AutoType::registerHotkey(quintptr nativeWindowId, const QString& sequence) {
    unregisterHotkey();
#ifdef _WIN32
    UINT mods = 0, vk = 0;
    if (!parseHotkey(sequence, mods, vk)) return false;
    m_hwnd = nativeWindowId;
    m_hotkeyId = 0x4D50;  // 'MP'
    if (!RegisterHotKey(reinterpret_cast<HWND>(m_hwnd), m_hotkeyId, mods | MOD_NOREPEAT, vk)) {
        m_hotkeyId = 0;
        return false;
    }
    m_registered = true;
    return true;
#else
    Q_UNUSED(nativeWindowId);
    Q_UNUSED(sequence);
    return false;
#endif
}

void AutoType::unregisterHotkey() {
#ifdef _WIN32
    if (m_registered) UnregisterHotKey(reinterpret_cast<HWND>(m_hwnd), m_hotkeyId);
#endif
    m_registered = false;
    m_hotkeyId = 0;
}

bool AutoType::typeInto(const ForegroundContext& target, QVector<AutofillEngine::Action> actions) {
    bool ok = false;
#ifdef _WIN32
    HWND hwnd = reinterpret_cast<HWND>(target.nativeHandle);
    if (hwnd && IsWindow(hwnd) && forceForeground(hwnd)) {
        Sleep(120);
        releaseModifiers();
        ok = true;
        for (const auto& a : actions) {
            // Abort if focus moved elsewhere: never type a password into the
            // wrong window.
            if (GetForegroundWindow() != hwnd) { ok = false; break; }
            switch (a.kind) {
                case AutofillEngine::Action::Text: {
                    const std::wstring w = a.text.toStdWString();
                    for (wchar_t ch : w) {
                        sendUnicodeChar(ch);
                        Sleep(12);
                    }
                    break;
                }
                case AutofillEngine::Action::Key: {
                    WORD vk = 0;
                    if (a.key == "TAB") vk = VK_TAB;
                    else if (a.key == "ENTER") vk = VK_RETURN;
                    else if (a.key == "SPACE") vk = VK_SPACE;
                    else if (a.key == "ESC") vk = VK_ESCAPE;
                    else if (a.key == "BACKSPACE") vk = VK_BACK;
                    if (vk) { sendVk(vk); Sleep(30); }
                    break;
                }
                case AutofillEngine::Action::Delay:
                    Sleep(static_cast<DWORD>(a.ms));
                    break;
            }
        }
    }
#else
    Q_UNUSED(target);
#endif
    for (auto& a : actions) wipe(a.text);
    return ok;
}

}  // namespace mp
