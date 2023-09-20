/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct BuildMenuCtx;
struct MenuOwnerDrawInfo;

struct MenuDef {
    const char* title = nullptr;
    UINT_PTR idOrSubmenu = 0;
};

constexpr const char* kMenuSeparator = "-----";

extern bool gShowDebugMenu;
extern bool gAddCrashMeMenu;

void FreeAllMenuDrawInfos();
void FreeMenuOwnerDrawInfo(MenuOwnerDrawInfo*);
void MarkMenuOwnerDraw(HMENU);
void FreeMenuOwnerDrawInfoData(HMENU);
void MenuCustomDrawMesureItem(HWND, MEASUREITEMSTRUCT*);
void MenuCustomDrawItem(HWND, DRAWITEMSTRUCT*);
HFONT GetMenuFont();

HMENU BuildMenuFromMenuDef(MenuDef* menuDefs, HMENU menu, BuildMenuCtx* ctx);
void RemoveBadMenuSeparators(HMENU menu);
HMENU BuildMenu(MainWindow* win);
void OnWindowContextMenu(MainWindow* win, int x, int y);
void OnAboutContextMenu(MainWindow* win, int x, int y);
int MenuIdFromVirtualZoom(float virtualZoom);
void UpdateAppMenu(MainWindow* win, HMENU m);
void ToggleMenuBar(MainWindow* win, bool showTemporarily = false);
float ZoomMenuItemToZoom(int menuItemId);
void ShowFileInFolder(const char* path);
