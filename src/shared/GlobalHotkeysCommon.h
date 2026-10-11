/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by GlobalHotkeysCommon_win.cpp and each app's GlobalHotkeys.cpp ---

#if OS_WIN
struct GlobalHotkeyInfo {
    int hotkeyId = 0;
    int cmdId = 0;
    Str key;
    Str cmd;
};
extern Vec<GlobalHotkeyInfo> gGlobalHotkeys;
extern HWND gGlobalHotkeysHwnd;
extern Vec<HWND> gActiveFrameHwndMRU;
#endif

constexpr int kGlobalHotkeyBaseId = 0x6000;
