/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Hack: we need NOMINMAX to make chrome code compile but we also need
// min/max for gdi+ headers, so we import min/max from stl
#include <algorithm>
using std::min;
using std::max;

#include "Resource.h"

#include "EbookTestMenu.h"
#include "StrUtil.h"

#define SEP_ITEM "-----"

MenuDef menuDefFile[] = {
    { "&Open\tCtrl+O",                IDM_OPEN },
    //{ "&Close\tCtrl+W",              IDM_CLOSE },
    { "Toggle bbox",                  IDM_TOGGLE_BBOX },
    { SEP_ITEM,                       0,       },
    { "E&xit\tCtrl+Q",                IDM_EXIT }
};

HMENU BuildMenuFromMenuDef(MenuDef menuDefs[], int menuLen, HMENU menu)
{
    assert(menu);

    for (int i = 0; i < menuLen; i++) {
        MenuDef md = menuDefs[i];
        const char *title = md.title;

        if (str::Eq(title, SEP_ITEM)) {
            AppendMenu(menu, MF_SEPARATOR, 0, NULL);
        } else {
            ScopedMem<TCHAR> tmp(str::conv::FromUtf8(title));
            AppendMenu(menu, MF_STRING, (UINT_PTR)md.id, tmp);
        }
    }

    return menu;
}

HMENU BuildMenu()
{
    HMENU mainMenu = CreateMenu();
    HMENU m;
    m = BuildMenuFromMenuDef(menuDefFile, dimof(menuDefFile), CreateMenu());
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _T("&File"));
    return mainMenu;
}
