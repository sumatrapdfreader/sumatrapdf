/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct AppCommandCtx;
using BuildMenuCtx = AppCommandCtx;

struct MenuOwnerDrawInfo;

struct MenuDef {
    Str title;
    UINT_PTR idOrSubmenu = 0;
};

constexpr const char* kMenuSeparator = "-----";

void FreeAllMenuDrawInfos();
void FreeMenuOwnerDrawInfo(MenuOwnerDrawInfo*);
void MarkMenuOwnerDraw(HMENU, bool isMenuBar = false);
void FreeMenuOwnerDrawInfoData(HMENU);
void MenuCustomDrawMesureItem(HWND, MEASUREITEMSTRUCT*);
void MenuCustomDrawItem(HWND, DRAWITEMSTRUCT*);

HMENU BuildMenuFromDef(MenuDef* menuDefs, HMENU menu, BuildMenuCtx* ctx);
void RemoveBadMenuSeparators(HMENU menu);
HMENU BuildMenu(MainWindow* win);
void OnWindowContextMenu(MainWindow* win, int x, int y);
void OnAboutContextMenu(MainWindow* win, int x, int y);
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