/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// un-comment this for experimental, unfinished theme support for menus
// by making them owner-drawn
// #define EXP_MENU_OWNER_DRAW 1

extern bool gAddCrashMeMenu;

#define SEP_ITEM "-----"

// those are defined here instead of resource.h to avoid
// having them overwritten by dialog editor
#define IDM_VIEW_LAYOUT_FIRST IDM_VIEW_SINGLE_PAGE
#define IDM_VIEW_LAYOUT_LAST IDM_VIEW_MANGA_MODE
#define IDM_ZOOM_FIRST IDM_ZOOM_FIT_PAGE
#define IDM_ZOOM_LAST IDM_ZOOM_CUSTOM
// note: IDM_VIEW_SINGLE_PAGE - IDM_VIEW_CONTINUOUS and also
//       IDM_ZOOM_FIT_PAGE - IDM_ZOOM_CUSTOM must be in a continuous range!
static_assert(IDM_VIEW_LAYOUT_LAST - IDM_VIEW_LAYOUT_FIRST == 4, "view layout ids are not in a continuous range");
static_assert(IDM_ZOOM_LAST - IDM_ZOOM_FIRST == 17, "zoom ids are not in a continuous range");

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
void UpdateMenu(WindowInfo* win, HMENU m);
void ShowHideMenuBar(WindowInfo* win, bool showTemporarily = false);
