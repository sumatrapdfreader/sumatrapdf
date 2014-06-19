/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef TableOfContents_h
#define TableOfContents_h

#include <CommCtrl.h>

class WindowInfo;

void CreateToc(WindowInfo *win);
void ClearTocBox(WindowInfo *win);
void ToggleTocBox(WindowInfo *win);
void LoadTocTree(WindowInfo *win);
void UpdateTocColors(WindowInfo *win);
void UpdateTocSelection(WindowInfo *win, int currPageNo);
void UpdateTocExpansionState(WindowInfo *win, HTREEITEM hItem);

#endif
