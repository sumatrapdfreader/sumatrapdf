/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "Notifications.h"
#include "ShortcutParse.h"
#include "Settings.h"
#include "Commands.h"
#include "MainWindow.h"
#include "SumatraPDF.h"
#include "AppSettings.h"
#include "GlobalHotkeys.h"
#include "GlobalHotkeysCommon.h"

Vec<GlobalHotkeyInfo> gGlobalHotkeys;

HWND gGlobalHotkeysHwnd = nullptr;

Vec<HWND> gActiveFrameHwndMRU;

void GlobalHotkeysOnActivate(HWND hwnd) {
    if (!hwnd) {
        return;
    }
    VecRemove(gActiveFrameHwndMRU, hwnd);
    VecInsertAt(gActiveFrameHwndMRU, 0, hwnd);
}

HWND GetGlobalHotkeysHwnd() {
    return gGlobalHotkeysHwnd;
}

// key and cmd are copies: a settings reload frees the shortcuts they come from
void AddGlobalHotkey(int hotkeyId, int cmdId, Str key, Str cmd) {
    GlobalHotkeyInfo info{hotkeyId, cmdId, str::Dup(key), str::Dup(cmd)};
    VecAppend(gGlobalHotkeys, info);
}

void UnregisterGlobalHotkeys(HWND hwnd) {
    if (!hwnd) {
        return;
    }
    for (auto& hk : gGlobalHotkeys) {
        UnregisterHotKey(hwnd, hk.hotkeyId);
        str::Free(hk.key);
        str::Free(hk.cmd);
    }
    VecReset(gGlobalHotkeys);
    if (hwnd == gGlobalHotkeysHwnd) {
        gGlobalHotkeysHwnd = nullptr;
    }
}
