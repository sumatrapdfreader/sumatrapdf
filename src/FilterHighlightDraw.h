/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct StrVec;

template <typename T>
struct Vec;

void DrawMaybeHighlightedText(HDC hdc, Rect rc, Str text, const StrVec& filterWords, Vec<u8>& highlighted,
                              COLORREF colBg, bool isRtl, bool matchWholeWord, uint drawFmt);

// TreeView post-paint: repaint the label with multi-word match underlays
// (command-palette style). `font` should be the tree's font (WM_GETFONT) so
// extents match the control's text; pass nullptr to keep the HDC font.
void DrawTreeItemFilterHighlight(HDC hdc, Rect labelRect, Str text, const StrVec& filterWords, COLORREF bgCol,
                                 COLORREF txtCol, HFONT font);

// Colors for clearing/redrawing a TreeView label after default paint.
// Selected+focus: system highlight. Selected unfocused: themed accent of
// treeBg (not COLOR_BTNFACE — unreadable with light text in dark mode).
// Non-selected: sample the painted row / treeBg / theme control bg.
// treeBg/treeTxt are TreeView::bgColor/textColor (may be unset).
// itemRc is the full row rect (TreeView_GetItemRect with textOnly=FALSE).
void ResolveTreeFilterItemColors(HDC hdc, Rect itemRc, COLORREF treeBg, COLORREF treeTxt, bool isSelected,
                                 bool hasFocus, COLORREF* bgOut, COLORREF* txtOut);

void SplitFilterToWords(Str filter, StrVec& words);
bool FilterMatches(Str str, const StrVec& words);
