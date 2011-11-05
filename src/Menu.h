/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Menu_h
#define Menu_h

#include "BaseUtil.h"
#include "SumatraPDF.h"
#include "resource.h"

// those are defined here instead of resource.h to avoid
// having them overwritten by dialog editor
#define IDM_VIEW_LAYOUT_FIRST           IDM_VIEW_SINGLE_PAGE
#define IDM_VIEW_LAYOUT_LAST            IDM_VIEW_CONTINUOUS
#define IDM_ZOOM_FIRST                  IDM_ZOOM_FIT_PAGE
#define IDM_ZOOM_LAST                   IDM_ZOOM_CUSTOM
// note: IDM_VIEW_SINGLE_PAGE - IDM_VIEW_CONTINUOUS and also
//       IDM_ZOOM_FIT_PAGE - IDM_ZOOM_CUSTOM must be in a continuous range!
CASSERT(IDM_VIEW_LAYOUT_LAST - IDM_VIEW_LAYOUT_FIRST == 3, view_layout_range);
CASSERT(IDM_ZOOM_LAST - IDM_ZOOM_FIRST == 17, zoom_range);

struct MenuDef {
    const char *title;
    int         id;
    int         flags;
};

class WindowInfo;

HMENU BuildMenuFromMenuDef(MenuDef menuDefs[], int menuLen, HMENU menu, bool forChm=false);
HMENU BuildMenu(WindowInfo *win);
void OnContextMenu(WindowInfo* win, int x, int y);
void OnAboutContextMenu(WindowInfo* win, int x, int y);
void OnMenuZoom(WindowInfo* win, UINT menuId);
void OnMenuCustomZoom(WindowInfo* win);
UINT MenuIdFromVirtualZoom(float virtualZoom);
void UpdateMenu(WindowInfo *win, HMENU m);
bool IsFileCloseMenuEnabled();

extern MenuDef menuDefFavorites[];

/* Define if you want to display an additional debug menu */
#ifdef DEBUG
#define SHOW_DEBUG_MENU_ITEMS
#endif

#endif
