/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct AppCommandCtx;
using BuildMenuCtx = AppCommandCtx;

struct MenuDef;

HMENU BuildMenuFromDef(MenuDef* menuDefs, HMENU menu, BuildMenuCtx* ctx);
void RemoveBadMenuSeparators(HMENU menu);
HMENU BuildMenu(MainWindow* win);
void OnWindowContextMenu(MainWindow* win, int x, int y);
void OnAboutContextMenu(MainWindow* win, int x, int y);
HMENU BuildHomeContextMenu(MainWindow* win, Str path);
TempStr HomeContextMenuRowsTemp(MainWindow* win, int invoke = 0);
bool CommandUsesContextMenuPoint(int cmdId);
void ForgetFileFromFrequentlyRead(MainWindow* win, Str filePath);
int CmdIdFromVirtualZoom(float virtualZoom);
void UpdateAppMenu(MainWindow* win, HMENU m);
void ToggleMenuBar(MainWindow* win, bool showTemporarily);
float ZoomMenuItemToZoom(int menuItemId);

int GetMenuBarRebarHeight(MainWindow*);
void CreateMenuBarRebar(MainWindow*);
void DestroyMenuBarRebar(MainWindow*);
void ShowMenuBarRebar(MainWindow*);
void RebuildMenuBarButtons(MainWindow*);
bool IsShowingMenuBarRebar(MainWindow*);
bool HandleMenuBarCommand(MainWindow*, int cmdId);
bool ActivateMenuBarByAccel(MainWindow*, WCHAR accel);
void UpdateCustomMenuBarMenuSelect(MainWindow*, WPARAM, LPARAM);

struct MenuAccelText {
    Str display;
    int underlineOff = -1;
    int underlineLen = 0;
};

MenuAccelText ParseMenuAccelTextTemp(Str s);
