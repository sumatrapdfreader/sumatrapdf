/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"

#include "Notifications.h"
#include "ShortcutParse.h"
#include "Settings.h"
#include "Commands.h"
#include "MainWindow.h"
#include "SumatraPDF.h"
#include "ScreenshotCapture.h"
#include "AppSettings.h"

#include "GlobalHotkeys.h"

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

static bool IsOtherSumatraProcessRunning() {
    DWORD myPid = GetCurrentProcessId();
    HWND hwnd = nullptr;
    while ((hwnd = FindWindowEx(HWND_DESKTOP, hwnd, L"SUMATRA_PDF_FRAME", nullptr)) != nullptr) {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != myPid) {
            return true;
        }
    }
    return false;
}

static UINT AccelFVirtToHotkeyMod(BYTE fVirt) {
    UINT mod = 0;
    if (fVirt & FALT) {
        mod |= MOD_ALT;
    }
    if (fVirt & FCONTROL) {
        mod |= MOD_CONTROL;
    }
    if (fVirt & FSHIFT) {
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
        MainWindow* win = FindMainWindowByHwnd(hwnd);
        if (IsMainWindowValidAndNotClosing(win) && IsWindow(win->hwndFrame)) {
            return win;
        }
    }
    for (MainWindow* win : gWindows) {
        if (IsMainWindowValidAndNotClosing(win) && IsWindow(win->hwndFrame)) {
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

        ACCEL accel{};
        if (!ParseShortcutString(sc->key, accel)) {
            continue;
        }
        UINT mod = AccelFVirtToHotkeyMod(accel.fVirt);
        UINT vk = accel.key;
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
        hwnd = gWindows[0]->hwndFrame;
    }
    if (hwnd && IsWindow(hwnd)) {
        UnregisterGlobalHotkeys(hwnd);
        RegisterGlobalHotkeys(hwnd);
    }
}

void GlobalHotkeysOnDestroy(HWND hwnd) {
    VecRemove(gActiveFrameHwndMRU, hwnd);
    if (hwnd == gGlobalHotkeysHwnd) {
        UnregisterGlobalHotkeys(hwnd);
        gGlobalHotkeysHwnd = nullptr;
        for (MainWindow* w : gWindows) {
            if (w->hwndFrame != hwnd && IsWindow(w->hwndFrame)) {
                RegisterGlobalHotkeys(w->hwndFrame);
                break;
            }
        }
    }
}

bool HandleGlobalHotkey(int hotkeyId) {
    for (const auto& hk : gGlobalHotkeys) {
        if (hk.hotkeyId == hotkeyId) {
            MainWindow* targetWin = GetTargetWindowForGlobalHotkey();
            if (targetWin) {
                HwndSendCommand(targetWin->hwndFrame, hk.cmdId);
            } else {
                int origId = hk.cmdId;
                CustomCommand* cc = FindCustomCommand(hk.cmdId);
                if (cc) {
                    origId = cc->origId;
                }
                if (origId == CmdScreenshot) {
                    TakeScreenshots(nullptr);
                }
            }
            return true;
        }
    }
    return false;
}
