/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Detect a "(Surname et al., 2020)" / "Surname (2020)" pattern at pagePos (page
// coordinates). On success returns true and fills *surnameOut with a
// freshly-allocated UTF-8 surname (caller frees) and *yearOut.
// srcRectOut (optional): on success, set to a stable per-occurrence source
// key — the matched citation's glyph span on the page (surname through year).
// Lets callers tell two occurrences of the same citation apart, including
// two markers on the same text line (different x/dx → reposition).
bool DetectCitationInPageText(WStr text, const Rect* coords, int textLen, Point pagePos, Str* surnameOut, int* yearOut,
                              Rect* srcRectOut = nullptr);

bool FindSurnameInPageText(WStr text, const Rect* coords, int textLen, WStr surnameW, int year, float* xOut,
                           float* yOut);

bool DetectNumericCitationInPageText(WStr text, const Rect* coords, int textLen, Point pagePos, int* numOut,
                                     Rect* srcRectOut = nullptr);

bool FindNumericReferenceInPageText(WStr text, const Rect* coords, int textLen, int num, float* xOut, float* yOut);
