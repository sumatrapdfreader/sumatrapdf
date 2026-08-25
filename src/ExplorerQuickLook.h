/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

constexpr int kCopyDataQuickLook = 0x514C6F6B; // 'QLok'

void ExplorerQuickLookApplyFromSettings();
bool RunExplorerQuickLookAgentLoop();
void ShowExplorerQuickLook(Str path);
bool HandleExplorerQuickLookCopyData(COPYDATASTRUCT* cds);
void ApplyExplorerQuickLookChrome(MainWindow* win);
MainWindow* FindExplorerQuickLookWindow();
void ExplorerQuickLookRemoveRunKey();
bool SendExplorerQuickLookToExisting(HWND hwnd, Str path);
