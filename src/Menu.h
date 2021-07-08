/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

extern bool gAddCrashMeMenu;

#define SEP_ITEM "-----"

struct MenuDef {
    const char* title{nullptr};
    UINT_PTR idOrSubmenu{0};
    int flags{0};
};

struct BuildMenuCtx {};

// value associated with menu item for owner-drawn purposes
struct MenuOwnerDrawInfo {
    const WCHAR* text{nullptr};
    // copy of MENUITEMINFO fields
    uint fType{0};
    uint fState{0};
    HBITMAP hbmpChecked{nullptr};
    HBITMAP hbmpUnchecked{nullptr};
    HBITMAP hbmpItem{nullptr};
};

void FreeAllMenuDrawInfos();
void FreeMenuOwnerDrawInfo(MenuOwnerDrawInfo*);
void MarkMenuOwnerDraw(HMENU);
void FreeMenuOwnerDrawInfoData(HMENU);
void MenuOwnerDrawnMesureItem(HWND, MEASUREITEMSTRUCT*);
void MenuOwnerDrawnDrawItem(HWND, DRAWITEMSTRUCT*);
HFONT GetMenuFont();

// TODO: use BuildMenuCtx
HMENU BuildMenuFromMenuDef(MenuDef* menuDefs, HMENU menu, int flagFilter);
HMENU BuildMenu(WindowInfo* win);
void OnWindowContextMenu(WindowInfo* win, int x, int y);
void OnAboutContextMenu(WindowInfo* win, int x, int y);
void OnMenuZoom(WindowInfo* win, int menuId);
void OnMenuCustomZoom(WindowInfo* win);
int MenuIdFromVirtualZoom(float virtualZoom);
void UpdateAppMenu(WindowInfo* win, HMENU m);
void ShowHideMenuBar(WindowInfo* win, bool showTemporarily = false);
