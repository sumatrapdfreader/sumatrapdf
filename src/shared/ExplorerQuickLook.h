/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;

constexpr int kCopyDataQuickLook = 0x514C6F6B;

void ExplorerQuickLookApplyFromSettings();
bool RunExplorerQuickLookAgentLoop();
void ShowExplorerQuickLook(Str path);
void ApplyExplorerQuickLookChrome(MainWindow* win);
MainWindow* FindExplorerQuickLookWindow();
void ExplorerQuickLookRemoveRunKey();
bool ExplorerQuickLookOnKeyDown(MainWindow* win, int key, bool hasModifiers);

#if OS_WIN
bool HandleExplorerQuickLookCopyData(COPYDATASTRUCT* cds);
bool SendExplorerQuickLookToExisting(HWND hwnd, Str path);
#endif
