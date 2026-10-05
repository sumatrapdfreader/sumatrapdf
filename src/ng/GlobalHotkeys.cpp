/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/UITask.h"

#include "Settings.h"
#include "DocController.h"
#include "Commands.h"
#include "VirtKeys.h"
#include "ShortcutParse.h"
#include "Notifications.h"
#include "MainWindow.h"
#include "SumatraPDF.h"
#include "Screenshot.h"
#include "AppSettings.h"
#include "gui/AppShell.h"

#include "GlobalHotkeys.h"

#if OS_WIN

#include "base/Win.h"

struct GlobalHotkeyInfo {
    int hotkeyId = 0;
    int cmdId = 0;
    Str key;
    Str cmd;
};

static Vec<GlobalHotkeyInfo> gGlobalHotkeys;
static HWND gGlobalHotkeysHwnd = nullptr;
static Vec<HWND> gActiveFrameHwndMRU;

constexpr int kGlobalHotkeyBaseId = 0x6000;

// ng: orig looks for another SUMATRA_PDF_FRAME window; our windows use gpui's
// shared class name, so a named mutex decides which process owns the hotkeys
static bool IsOtherSumatraProcessRunning() {
    static int cached = -1;
    if (cached >= 0) {
        return cached != 0;
    }
    HANDLE h = CreateMutexW(nullptr, TRUE, L"SumatraPDF-ng-GlobalHotkeys");
    cached = (h && GetLastError() == ERROR_ALREADY_EXISTS) ? 1 : 0;
    return cached != 0;
}

static UINT ShortcutToHotkeyMod(const KeyShortcut& sc) {
    UINT mod = 0;
    if (sc.alt) {
        mod |= MOD_ALT;
    }
    if (sc.ctrl) {
        mod |= MOD_CONTROL;
    }
    if (sc.shift) {
        mod |= MOD_SHIFT;
    }
    return mod;
}

void GlobalHotkeysOnActivate(HWND hwnd) {
    if (!hwnd) {
        return;
    }
    VecRemove(gActiveFrameHwndMRU, hwnd);
    VecInsertAt(gActiveFrameHwndMRU, 0, hwnd);
}

static MainWindow* GetTargetWindowForGlobalHotkey() {
    for (HWND hwnd : gActiveFrameHwndMRU) {
        MainWindow* win = AppShellWindowFromHwnd(hwnd);
        if (IsMainWindowValidAndNotClosing(win)) {
            return win;
        }
    }
    for (MainWindow* win : gWindows) {
        if (IsMainWindowValidAndNotClosing(win)) {
            return win;
        }
    }
    return nullptr;
}

HWND GetGlobalHotkeysHwnd() {
    return gGlobalHotkeysHwnd;
}

void RegisterGlobalHotkeys(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return;
    }
    if (!VecContains(gActiveFrameHwndMRU, hwnd)) {
        VecAppend(gActiveFrameHwndMRU, hwnd);
    }
    if (IsOtherSumatraProcessRunning()) {
        return;
    }
    if (gGlobalHotkeysHwnd && gGlobalHotkeysHwnd != hwnd) {
        UnregisterGlobalHotkeys(gGlobalHotkeysHwnd);
    }
    gGlobalHotkeysHwnd = hwnd;
    if (!gSettings || !gSettings->shortcuts) {
        return;
    }

    int idx = 0;
    for (Shortcut* sc : *gSettings->shortcuts) {
        if (str::IsEmptyOrWhiteSpace(sc->key)) {
            continue;
        }
        bool isGlobal = IsGlobalShortcut(sc->key);
        if (!isGlobal && str::EqI(sc->cmd, StrL("CmdScreenshot"))) {
            isGlobal = true;
        }
        if (!isGlobal) {
            continue;
        }

        KeyShortcut ks;
        if (!ParseShortcutString(sc->key, ks)) {
            continue;
        }
        UINT mod = ShortcutToHotkeyMod(ks);
        UINT vk = ks.vk;
        int hotkeyId = kGlobalHotkeyBaseId + idx++;
        BOOL ok = RegisterHotKey(hwnd, hotkeyId, mod, vk);
        if (!ok) {
            MaybeDelayedWarningNotification(fmt("Couldn't register '%s' global hotkey for '%s'", sc->key, sc->cmd));
            continue;
        }
        int cmdId = sc->cmdId;
        if (cmdId <= 0) {
            cmdId = GetCommandIdByName(sc->cmd);
        }
        GlobalHotkeyInfo info{hotkeyId, cmdId, sc->key, sc->cmd};
        VecAppend(gGlobalHotkeys, info);
        logf("RegisterGlobalHotkeys: '%s' -> %s (%d)\n", sc->key, sc->cmd, cmdId);
    }
}

void UnregisterGlobalHotkeys(HWND hwnd) {
    if (!hwnd) {
        return;
    }
    for (const auto& hk : gGlobalHotkeys) {
        UnregisterHotKey(hwnd, hk.hotkeyId);
    }
    VecReset(gGlobalHotkeys);
    if (hwnd == gGlobalHotkeysHwnd) {
        gGlobalHotkeysHwnd = nullptr;
    }
}

void ReRegisterGlobalHotkeys() {
    HWND hwnd = gGlobalHotkeysHwnd;
    if (!hwnd && len(gWindows) > 0) {
        hwnd = AppShellNativeHwnd(gWindows[0]);
    }
    if (hwnd && IsWindow(hwnd)) {
        UnregisterGlobalHotkeys(hwnd);
        RegisterGlobalHotkeys(hwnd);
    }
}

void GlobalHotkeysOnDestroy(HWND hwnd) {
    VecRemove(gActiveFrameHwndMRU, hwnd);
    if (hwnd != gGlobalHotkeysHwnd) {
        return;
    }
    UnregisterGlobalHotkeys(hwnd);
    gGlobalHotkeysHwnd = nullptr;
    for (MainWindow* w : gWindows) {
        HWND other = AppShellNativeHwnd(w);
        if (other && other != hwnd && IsWindow(other)) {
            RegisterGlobalHotkeys(other);
            break;
        }
    }
}

struct GlobalHotkeyRun {
    MainWindow* win = nullptr;
    int cmdId = 0;
};

// ng: orig posts a WM_COMMAND to the frame. The hotkey arrives inside gpui's
// message pump, so the command runs from the uitask queue instead
static void RunGlobalHotkeyCmd(GlobalHotkeyRun* d) {
    AutoDelete delD(d);
    if (d->win) {
        if (IsMainWindowValidAndNotClosing(d->win)) {
            ExecuteCmd(d->win, d->cmdId);
        }
        return;
    }
    TakeScreenshots(nullptr);
}

bool HandleGlobalHotkey(int hotkeyId) {
    for (const auto& hk : gGlobalHotkeys) {
        if (hk.hotkeyId != hotkeyId) {
            continue;
        }
        logf("HandleGlobalHotkey: %s (%d)\n", hk.cmd, hk.cmdId);
        MainWindow* targetWin = GetTargetWindowForGlobalHotkey();
        auto* d = new GlobalHotkeyRun;
        d->cmdId = hk.cmdId;
        if (targetWin) {
            d->win = targetWin;
        } else {
            int origId = hk.cmdId;
            CustomCommand* cc = FindCustomCommand(hk.cmdId);
            if (cc) {
                origId = cc->origId;
            }
            if (origId != CmdScreenshot) {
                delete d;
                return true;
            }
        }
        uitask::Post(MkFunc0<GlobalHotkeyRun>(RunGlobalHotkeyCmd, d), "GlobalHotkey");
        return true;
    }
    return false;
}

#elif OS_LINUX

#define Pixmap X11Pixmap
#include <X11/Xlib.h>
#undef Pixmap
#include <X11/keysym.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

struct LinuxGlobalHotkey {
    KeyShortcut shortcut;
    int cmdId = 0;
    int keycode = 0;
    Str key;
    Str cmd;
};

struct LinuxHotkeyRun {
    int cmdId = 0;
};

struct LinuxHotkeyWarning {
    Str text;
};

static Vec<LinuxGlobalHotkey> gLinuxHotkeys;
static Mutex gLinuxHotkeysMutex;
static ThreadHandle gLinuxHotkeyThread = {};
static int gLinuxHotkeyWake[2] = {-1, -1};
static int gX11HotkeyError = 0;
static Display* gX11HotkeyDisplay = nullptr;
static int (*gPrevX11ErrorHandler)(Display*, XErrorEvent*) = nullptr;

static void FreeLinuxHotkeys() {
    for (LinuxGlobalHotkey& hk : gLinuxHotkeys) {
        str::Free(hk.key);
        str::Free(hk.cmd);
    }
    VecReset(gLinuxHotkeys);
}

static KeySym ShortcutKeySym(const KeyShortcut& shortcut) {
    int vk = shortcut.vk;
    if (vk >= 'A' && vk <= 'Z') {
        return (KeySym)(XK_A + vk - 'A');
    }
    if (vk >= '0' && vk <= '9') {
        return (KeySym)(XK_0 + vk - '0');
    }
    if (vk >= VK_F1 && vk <= VK_F24) {
        return (KeySym)(XK_F1 + vk - VK_F1);
    }
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        return (KeySym)(XK_KP_0 + vk - VK_NUMPAD0);
    }
    switch (vk) {
        case VK_BACK:
            return XK_BackSpace;
        case VK_TAB:
            return XK_Tab;
        case VK_RETURN:
            return XK_Return;
        case VK_PAUSE:
            return XK_Pause;
        case VK_CAPITAL:
            return XK_Caps_Lock;
        case VK_ESCAPE:
            return XK_Escape;
        case VK_SPACE:
            return XK_space;
        case VK_PRIOR:
            return XK_Page_Up;
        case VK_NEXT:
            return XK_Page_Down;
        case VK_END:
            return XK_End;
        case VK_HOME:
            return XK_Home;
        case VK_LEFT:
            return XK_Left;
        case VK_UP:
            return XK_Up;
        case VK_RIGHT:
            return XK_Right;
        case VK_DOWN:
            return XK_Down;
        case VK_SNAPSHOT:
            return XK_Print;
        case VK_INSERT:
            return XK_Insert;
        case VK_DELETE:
            return XK_Delete;
        case VK_MULTIPLY:
            return XK_KP_Multiply;
        case VK_ADD:
            return XK_KP_Add;
        case VK_SEPARATOR:
            return XK_KP_Separator;
        case VK_SUBTRACT:
            return XK_KP_Subtract;
        case VK_DECIMAL:
            return XK_KP_Decimal;
        case VK_DIVIDE:
            return XK_KP_Divide;
        case VK_OEM_1:
            return XK_semicolon;
        case VK_OEM_PLUS:
            return XK_equal;
        case VK_OEM_COMMA:
            return XK_comma;
        case VK_OEM_MINUS:
            return XK_minus;
        case VK_OEM_PERIOD:
            return XK_period;
        case VK_OEM_2:
            return XK_slash;
        case VK_OEM_3:
            return XK_grave;
        case VK_OEM_4:
            return XK_bracketleft;
        case VK_OEM_5:
            return XK_backslash;
        case VK_OEM_6:
            return XK_bracketright;
        case VK_OEM_7:
            return XK_apostrophe;
        default:
            return NoSymbol;
    }
}

static unsigned int ShortcutXMods(const KeyShortcut& shortcut) {
    unsigned int mods = 0;
    if (shortcut.ctrl) {
        mods |= ControlMask;
    }
    if (shortcut.shift) {
        mods |= ShiftMask;
    }
    if (shortcut.alt) {
        mods |= Mod1Mask;
    }
    return mods;
}

static unsigned int NumLockMask(Display* display) {
    unsigned int mask = 0;
    XModifierKeymap* map = XGetModifierMapping(display);
    if (!map) {
        return 0;
    }
    KeyCode numLock = XKeysymToKeycode(display, XK_Num_Lock);
    for (int mod = 0; mod < 8; mod++) {
        for (int i = 0; i < map->max_keypermod; i++) {
            if (map->modifiermap[mod * map->max_keypermod + i] == numLock) {
                mask = (unsigned int)(1 << mod);
            }
        }
    }
    XFreeModifiermap(map);
    return mask;
}

static int OnX11HotkeyError(Display* display, XErrorEvent* ev) {
    if (display == gX11HotkeyDisplay) {
        gX11HotkeyError = ev->error_code;
        return 0;
    }
    if (gPrevX11ErrorHandler) {
        return gPrevX11ErrorHandler(display, ev);
    }
    return 0;
}

static void ShowLinuxHotkeyWarning(LinuxHotkeyWarning* d) {
    AutoDelete delD(d);
    MaybeDelayedWarningNotification(d->text);
    str::Free(d->text);
}

static void WarnLinuxHotkey(const LinuxGlobalHotkey& hk) {
    auto* d = new LinuxHotkeyWarning;
    d->text = str::Dup(fmt("Couldn't register '%s' global hotkey for '%s'", hk.key, hk.cmd));
    uitask::Post(MkFunc0(ShowLinuxHotkeyWarning, d), "GlobalHotkeyWarning");
}

static void RunLinuxGlobalHotkey(LinuxHotkeyRun* d) {
    AutoDelete delD(d);
    MainWindow* target = nullptr;
    for (MainWindow* win : gWindows) {
        if (IsMainWindowValidAndNotClosing(win)) {
            target = win;
            break;
        }
    }
    if (target) {
        ExecuteCmd(target, d->cmdId);
        return;
    }
    int cmdId = d->cmdId;
    CustomCommand* custom = FindCustomCommand(cmdId);
    if (custom) {
        cmdId = custom->origId;
    }
    if (cmdId == CmdScreenshot) {
        TakeScreenshots(nullptr);
    }
}

static void ApplyLinuxHotkeys(Display* display, Window root) {
    XUngrabKey(display, AnyKey, AnyModifier, root);
    unsigned int numLock = NumLockMask(display);
    unsigned int ignored[] = {0, LockMask, 0, 0};
    int ignoredCount = 2;
    if (numLock != 0) {
        ignored[ignoredCount++] = numLock;
        ignored[ignoredCount++] = LockMask | numLock;
    }
    ScopedMutex lock(&gLinuxHotkeysMutex);
    for (LinuxGlobalHotkey& hk : gLinuxHotkeys) {
        hk.keycode = (int)XKeysymToKeycode(display, ShortcutKeySym(hk.shortcut));
        if (hk.keycode == 0) {
            WarnLinuxHotkey(hk);
            continue;
        }
        unsigned int mods = ShortcutXMods(hk.shortcut);
        gX11HotkeyError = 0;
        for (int i = 0; i < ignoredCount; i++) {
            XGrabKey(display, hk.keycode, mods | ignored[i], root, False, GrabModeAsync, GrabModeAsync);
        }
        XSync(display, False);
        if (gX11HotkeyError != 0) {
            WarnLinuxHotkey(hk);
            XUngrabKey(display, hk.keycode, AnyModifier, root);
            hk.keycode = 0;
            continue;
        }
        logf("RegisterGlobalHotkeys: '%s' -> %s (%d)\n", hk.key, hk.cmd, hk.cmdId);
    }
    XFlush(display);
}

static void HandleLinuxHotkeyEvent(const XKeyEvent& ev) {
    int cmdId = 0;
    unsigned int mods = ev.state & (ShiftMask | ControlMask | Mod1Mask);
    {
        ScopedMutex lock(&gLinuxHotkeysMutex);
        for (const LinuxGlobalHotkey& hk : gLinuxHotkeys) {
            if (hk.keycode == (int)ev.keycode && ShortcutXMods(hk.shortcut) == mods) {
                cmdId = hk.cmdId;
                logf("HandleGlobalHotkey: %s (%d)\n", hk.cmd, hk.cmdId);
                break;
            }
        }
    }
    if (cmdId > 0) {
        auto* d = new LinuxHotkeyRun{cmdId};
        uitask::Post(MkFunc0(RunLinuxGlobalHotkey, d), "GlobalHotkey");
    }
}

static void LinuxHotkeyThread() {
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        logf("GlobalHotkeys: couldn't open X11 display\n");
        return;
    }
    gX11HotkeyDisplay = display;
    gPrevX11ErrorHandler = XSetErrorHandler(OnX11HotkeyError);
    Window root = DefaultRootWindow(display);
    ApplyLinuxHotkeys(display, root);
    pollfd fds[2] = {{ConnectionNumber(display), POLLIN, 0}, {gLinuxHotkeyWake[0], POLLIN, 0}};
    for (;;) {
        int n = poll(fds, dimof(fds), -1);
        if (n < 0) {
            continue;
        }
        if (fds[1].revents & POLLIN) {
            char buf[32];
            while (read(gLinuxHotkeyWake[0], buf, dimof(buf)) > 0) {
            }
            ApplyLinuxHotkeys(display, root);
        }
        if (fds[0].revents & POLLIN) {
            while (XPending(display) > 0) {
                XEvent ev{};
                XNextEvent(display, &ev);
                if (ev.type == KeyPress) {
                    HandleLinuxHotkeyEvent(ev.xkey);
                }
            }
        }
    }
}

void ReRegisterGlobalHotkeys() {
    {
        ScopedMutex lock(&gLinuxHotkeysMutex);
        FreeLinuxHotkeys();
        if (gSettings && gSettings->shortcuts) {
            for (Shortcut* sc : *gSettings->shortcuts) {
                bool isGlobal = IsGlobalShortcut(sc->key);
                if (!isGlobal && str::EqI(sc->cmd, StrL("CmdScreenshot"))) {
                    isGlobal = true;
                }
                if (!isGlobal || str::IsEmptyOrWhiteSpace(sc->key)) {
                    continue;
                }
                KeyShortcut shortcut;
                if (!ParseShortcutString(sc->key, shortcut)) {
                    continue;
                }
                int cmdId = sc->cmdId > 0 ? sc->cmdId : GetCommandIdByName(sc->cmd);
                LinuxGlobalHotkey hk{shortcut, cmdId, 0, str::Dup(sc->key), str::Dup(sc->cmd)};
                VecAppend(gLinuxHotkeys, hk);
            }
        }
    }

    if (!gLinuxHotkeyThread) {
        if (pipe(gLinuxHotkeyWake) != 0) {
            logf("GlobalHotkeys: couldn't create wake pipe\n");
            return;
        }
        fcntl(gLinuxHotkeyWake[0], F_SETFL, O_NONBLOCK);
        fcntl(gLinuxHotkeyWake[1], F_SETFL, O_NONBLOCK);
        gLinuxHotkeyThread = StartThread(MkFunc0Void(LinuxHotkeyThread), StrL("GlobalHotkeys"));
        return;
    }
    char wake = 1;
    write(gLinuxHotkeyWake[1], &wake, 1);
}

#else

// Cocoa has no global-hotkey abstraction in gpui yet.
void ReRegisterGlobalHotkeys() {}

#endif
