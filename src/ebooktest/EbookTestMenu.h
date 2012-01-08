/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef EbookTestMenu_h
#define EbookTestMenu_h

#include "BaseUtil.h"

struct MenuDef {
    const char *title;
    int         id;
};

HMENU BuildMenuFromMenuDef(MenuDef menuDefs[], int menuLen, HMENU menu);
HMENU BuildMenu();

#endif
