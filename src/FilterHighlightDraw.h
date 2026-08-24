/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct StrVec;
struct Gfx;
struct PlatformFont;

#include "FilterUtil.h"

template <typename T>
struct Vec;

void DrawMaybeHighlightedText(Gfx* gfx, Rect rc, Str text, const StrVec& filterWords, Vec<u8>& highlighted, Color colBg,
                              bool isRtl, bool matchWholeWord, u32 drawFlags, PlatformFont* font,
                              Color colText = kColorUnset);

void DrawTreeItemFilterHighlight(Gfx* gfx, Rect labelRect, Str text, const StrVec& filterWords, Color bgCol,
                                 Color txtCol, PlatformFont* font, int boldTextOffset = -1, int boldTextLen = 0);

void ResolveTreeFilterItemColors(HDC hdc, Rect itemRc, Color treeBg, Color treeTxt, bool isSelected, bool hasFocus,
                                 Color* bgOut, Color* txtOut);
