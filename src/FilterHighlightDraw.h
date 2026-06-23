/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct StrVec;

template <typename T>
class Vec;

void DrawMaybeHighlightedText(HDC hdc, RECT rc, const char* text, const StrVec& filterWords, Vec<u8>& highlighted,
                              COLORREF colBg, bool isRtl, bool matchWholeWord, uint drawFmt);

void SplitFilterToWords(const char* filter, StrVec& words);
bool FilterMatches(const char* str, const StrVec& words);