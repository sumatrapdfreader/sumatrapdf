/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;

#if OS_WIN
void RegisterGlobalHotkeys(HWND hwnd);
void UnregisterGlobalHotkeys(HWND hwnd);
bool HandleGlobalHotkey(int hotkeyId);
void GlobalHotkeysOnActivate(HWND hwnd);
void GlobalHotkeysOnDestroy(HWND hwnd);
HWND GetGlobalHotkeysHwnd();
#endif

// re-reads the Shortcuts from the settings; a no-op where the OS has no
// global hotkeys
void ReRegisterGlobalHotkeys();
