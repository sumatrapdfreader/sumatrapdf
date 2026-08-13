/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct StrVec;
struct Gfx;
struct PlatformFont;

template <typename T>
struct Vec;

void DrawMaybeHighlightedText(Gfx* gfx, Rect rc, Str text, const StrVec& filterWords, Vec<u8>& highlighted,
                              COLORREF colBg, bool isRtl, bool matchWholeWord, u32 drawFlags, PlatformFont* font,
                              COLORREF colText = kColorUnset);

void DrawTreeItemFilterHighlight(HDC hdc, Rect labelRect, Str text, const StrVec& filterWords, COLORREF bgCol,
                                 COLORREF txtCol, HFONT font);

void ResolveTreeFilterItemColors(HDC hdc, Rect itemRc, COLORREF treeBg, COLORREF treeTxt, bool isSelected,
                                 bool hasFocus, COLORREF* bgOut, COLORREF* txtOut);

void SplitFilterToWords(Str filter, StrVec& words);
bool FilterMatches(Str str, const StrVec& words);
