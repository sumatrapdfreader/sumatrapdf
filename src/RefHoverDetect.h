/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Pure-function region detectors used by RefHover to decide what slice of the
// destination page to render into the hover popup. Kept engine-independent so
// the heuristics can be unit-tested with synthetic glyph arrays (see
// src/utils/tests/RefHover_ut.cpp).
//
// All three functions take:
//   text     — per-glyph WCHAR array (engine->GetTextForPage's first out-ptr)
//   coords   — per-glyph Rect array, parallel to `text` (second out-ptr)
//   textLen  — glyph count
//   mediabox — page bounds in PDF user space
//   destX, destY — link's destination coordinates (PDF user space)
//
// Returned RectF is in PDF user space, clipped to mediabox.

// Landscape view: full page width strip anchored at destY, extending downward
// to the last text glyph or a recognised caption block. Fallback when no
// recognisable entry or equation is found.
RectF LandscapeBox(RectF mediabox, float destX, float destY, const WCHAR* text, const Rect* coords, int textLen);

// Equation cross-ref: tight one-line box around a "(N)" or "(N.M)" label
// sitting at the right column edge near destY. Returns empty rect when no
// equation label is detected.
RectF DetectEquationBox(const WCHAR* text, const Rect* coords, int textLen, RectF mediabox, float destX, float destY);

// Bibliography / glossary / abbreviation entry box. Tries bracket-style
// ("[Foo+09]"), hanging-indent author-year, and single-line description-list
// layouts. Falls back to LandscapeBox when the destination doesn't look like
// a list entry.
RectF DetectEntryBox(const WCHAR* text, const Rect* coords, int textLen, RectF mediabox, float destX, float destY);
