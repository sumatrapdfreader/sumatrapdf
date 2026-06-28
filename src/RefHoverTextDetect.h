/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Pure-function plain-text citation detectors for PDFs without hyperref links.
// Engine-independent so the heuristics can be unit-tested with synthetic glyph
// arrays (see src/utils/tests/RefHover_ut.cpp).
//
// Both functions take the engine->GetTextForPage out-ptrs:
//   text     — per-glyph WCHAR array
//   coords   — per-glyph Rect array, parallel to `text`
//   textLen  — glyph count

// Detect a "(Surname et al., 2020)" / "Surname (2020)" pattern at pagePos (page
// coordinates). On success returns true and fills *surnameOut with a
// freshly-allocated UTF-8 surname (caller frees) and *yearOut.
// srcRectOut (optional): on success, set to a stable per-occurrence source
// key — the matched citation's glyph span on the page (surname through year).
// Lets callers tell two occurrences of the same citation apart, including
// two markers on the same text line (different x/dx → reposition).
bool DetectCitationInPageText(WStr text, const Rect* coords, int textLen, Point pagePos, Str* surnameOut, int* yearOut,
                              Rect* srcRectOut = nullptr);

// Search a page's glyph arrays for a bibliography entry whose line starts
// with (or contains, near the line start) `surnameW` and whose entry text
// contains `year`. Returns true on hit and fills xOut/yOut with the entry's
// anchor (top-left of the matching line's first glyph).
bool FindSurnameInPageText(WStr text, const Rect* coords, int textLen, WStr surnameW, int year, float* xOut,
                           float* yOut);

// Detect a numeric "[N]" citation marker (IEEE / numbered reference style) at
// pagePos (page coordinates). Handles lists / ranges ("[1, 2]", "[3-5]") by
// picking the number token nearest the cursor. On success returns true and
// fills *numOut with the reference number.
// srcRectOut (optional): see DetectCitationInPageText — stable per-occurrence
// "[N]" bracket span set on success.
bool DetectNumericCitationInPageText(WStr text, const Rect* coords, int textLen, Point pagePos, int* numOut,
                                     Rect* srcRectOut = nullptr);

// Search a page's glyph arrays for a bibliography entry whose line starts with
// "[num]" at the page's leftmost text column. Returns true on hit and fills
// xOut/yOut with the entry's anchor (top-left of the "[" glyph).
bool FindNumericReferenceInPageText(WStr text, const Rect* coords, int textLen, int num, float* xOut, float* yOut);
