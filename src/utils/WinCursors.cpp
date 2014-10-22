/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "WinCursors.h"

static LPWSTR knownCursorIds[] = {
    IDC_ARROW,
    IDC_IBEAM,
    IDC_HAND,
    IDC_SIZEALL,
    IDC_SIZEWE,
    IDC_SIZENS,
    IDC_NO,
    IDC_CROSS
};

static HCURSOR cachedCursors[dimof(knownCursorIds)] = { };

HCURSOR GetCursor(LPWSTR id)
{
    int cursorIdx = -1;
    for (int i = 0; i < dimof(knownCursorIds); i++) {
        if (id == knownCursorIds[i]) {
            cursorIdx = i;
            break;
        }
    }
    CrashIf(cursorIdx == -1);
    if (NULL == cachedCursors[cursorIdx]) {
        cachedCursors[cursorIdx] = LoadCursor(NULL, id);
        CrashIf(cachedCursors[cursorIdx] == NULL);
    }
    return cachedCursors[cursorIdx];
}

void SetCursor(LPWSTR id)
{
    SetCursor(GetCursor(id));
}
