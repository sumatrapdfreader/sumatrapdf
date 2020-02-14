/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

extern bool gAddCrashMeMenu;

#define SEP_ITEM "-----"

struct MenuDef {
    const char* title = nullptr;
    UINT id = 0;
    int flags = 0;
};

// value associated with menu item for owner-drawn purposes
struct MenuOwnerDrawInfo {
    const WCHAR* text = nullptr;
    // copy of MENUITEMINFO fields
    UINT fType = 0;
    UINT fState = 0;
    HBITMAP hbmpChecked = nullptr;
    HBITMAP hbmpUnchecked = nullptr;
    HBITMAP hbmpItem = nullptr;
};

void FreeAllMenuDrawInfos();
void FreeMenuOwnerDrawInfo(MenuOwnerDrawInfo*);
void MarkMenuOwnerDraw(HMENU);
void FreeMenuOwnerDrawInfoData(HMENU);
void MenuOwnerDrawnMesureItem(HWND, MEASUREITEMSTRUCT*);
void MenuOwnerDrawnDrawItem(HWND, DRAWITEMSTRUCT*);
HFONT GetMenuFont();

HMENU BuildMenuFromMenuDef(MenuDef menuDefs[], int menuLen, HMENU menu, int flagFilter = 0);
HMENU BuildMenu(WindowInfo* win);
void OnWindowContextMenu(WindowInfo* win, int x, int y);
void OnAboutContextMenu(WindowInfo* win, int x, int y);
void OnMenuZoom(WindowInfo* win, UINT menuId);
void OnMenuCustomZoom(WindowInfo* win);
UINT MenuIdFromVirtualZoom(float virtualZoom);
void UpdateAppMenu(WindowInfo* win, HMENU m);
void ShowHideMenuBar(WindowInfo* win, bool showTemporarily = false);
