/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Search_h
#define Search_h

#define HIDE_FWDSRCHMARK_TIMER_ID                4
#define HIDE_FWDSRCHMARK_DELAY_IN_MS             400
#define HIDE_FWDSRCHMARK_DECAYINTERVAL_IN_MS     100
#define HIDE_FWDSRCHMARK_STEPS                   5

class WindowInfo;

#include "WindowInfo.h"

bool NeedsFindUI(WindowInfo *win);
void ClearSearchResult(WindowInfo& win);
bool OnInverseSearch(WindowInfo& win, int x, int y);
void PaintForwardSearchMark(WindowInfo& win, HDC hdc);
void OnMenuFindPrev(WindowInfo& win);
void OnMenuFindNext(WindowInfo& win);
void OnMenuFind(WindowInfo& win);
void OnMenuFindMatchCase(WindowInfo& win);

#endif
