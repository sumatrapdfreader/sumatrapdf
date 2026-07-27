/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct StrVec;

template <typename T>
class Vec;

void DrawMaybeHighlightedText(HDC hdc, RECT rc, Str text, const StrVec& filterWords, Vec<u8>& highlighted,
                              COLORREF colBg, bool isRtl, bool matchWholeWord, uint drawFmt);

// TreeView post-paint: repaint the label with multi-word match underlays
// (command-palette style). `font` should be the tree's font (WM_GETFONT) so
// extents match the control's text; pass nullptr to keep the HDC font.
void DrawTreeItemFilterHighlight(HDC hdc, RECT labelRect, Str text, const StrVec& filterWords, COLORREF bgCol,
                                 COLORREF txtCol, HFONT font);

void SplitFilterToWords(Str filter, StrVec& words);
bool FilterMatches(Str str, const StrVec& words);