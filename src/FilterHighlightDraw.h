/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct StrVec;
struct Gfx;
struct PlatformFont;

template <typename T>
struct Vec;

void DrawMaybeHighlightedText(Gfx* gfx, Rect rc, Str text, const StrVec& filterWords, Vec<u8>& highlighted, Color colBg,
                              bool isRtl, bool matchWholeWord, u32 drawFlags, PlatformFont* font,
                              Color colText = kColorUnset);

void DrawTreeItemFilterHighlight(HDC hdc, Rect labelRect, Str text, const StrVec& filterWords, Color bgCol,
                                 Color txtCol, HFONT font);

void ResolveTreeFilterItemColors(HDC hdc, Rect itemRc, Color treeBg, Color treeTxt, bool isSelected, bool hasFocus,
                                 Color* bgOut, Color* txtOut);

void SplitFilterToWords(Str filter, StrVec& words);
bool FilterMatches(Str str, const StrVec& words);
