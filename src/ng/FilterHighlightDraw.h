/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// ng: orig draws the filter's matched letters with a highlight underlay
// through Gfx text runs (`DrawMaybeHighlightedText` /
// `DrawTreeItemFilterHighlight`). gpui has no text-run api, so the same mask
// drives a flex line of `TextEl` spans instead. `ResolveTreeFilterItemColors`
// is gone with the win32 TreeView custom draw: gpui rows carry their own
// colors.

struct StrVec;

namespace gpui {
struct Ctx;
struct El;
struct Rgba;
} // namespace gpui

#include "FilterUtil.h"

template <typename T>
struct Vec;

// orig's per-byte match mask from DrawMaybeHighlightedText: out[i] != 0 for a
// byte of `text` that one of `filterWords` matched. `out` is resized to
// len(text).
void MarkFilterHighlights(Str text, const StrVec& filterWords, Vec<u8>& out, bool matchWholeWord = false);

// One row of text with the matched letters highlighted. boldOffset / boldLen
// is orig's DrawTreeItemFilterHighlight bold range (the favorites file-name
// prefix); < 0 means none.
gpui::El* FilterHighlightText(gpui::Ctx* cx, Str text, const StrVec& filterWords, gpui::Rgba fg, float fontSize,
                              int boldOffset = -1, int boldLen = 0);
