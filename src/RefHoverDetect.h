/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Pure-function region detectors used by RefHover to decide what slice of the
// destination page to render into the hover popup. Kept engine-independent so
// the heuristics can be unit-tested with synthetic glyph arrays (see
// src/base/tests/RefHover_ut.cpp).
//
// All three functions take:
//   text     — per-glyph WCHAR array (engine->GetTextForPage's first out-ptr)
//   coords   — per-glyph Rect array, parallel to `text` (second out-ptr)
//   textLen  — glyph count
//   mediabox — page bounds in PDF user space
//   destX, destY — link's destination coordinates (PDF user space)
//
// Returned RectF is in PDF user space, clipped to mediabox.

// Flatten per-glyph ink boxes to uniform top-aligned line rows. mupdf reports
// tight per-glyph boxes whose tops vary within a line; the detectors below key
// off coords[i].y as a line coordinate, so callers must pass coords through
// this first (grouping by baseline = y+dy). `out` needs textLen rects and must
// not alias `coords`. Synthetic top-aligned input is left effectively
// unchanged (each line already has a single top).
void NormalizeGlyphLines(const Rect* coords, Rect* out, int glyphCount);

// Remove diagonal draft / "under review" watermark glyphs (oversized + sitting
// on sparse baselines) from a page's raw glyph arrays, so 2-column gutter and
// entry-bound detection see clean text. Run on the engine's raw coords *before*
// NormalizeGlyphLines. `outText`/`outCoords` need room for glyphCount entries;
// returns the kept-glyph count, written to the front of the out arrays.
int StripWatermarkGlyphs(WStr text, const Rect* coords, WCHAR* outText, Rect* outCoords);

// Landscape view: full page width strip anchored at destY, extending downward
// to the last text glyph or a recognised caption block. Fallback when no
// recognisable entry or equation is found.
RectF LandscapeBox(RectF mediabox, float destX, float destY, WStr text, const Rect* coords);

// Equation cross-ref: tight one-line box around a "(N)" or "(N.M)" label
// sitting at the right column edge near destY. Returns empty rect when no
// equation label is detected.
RectF DetectEquationBox(WStr text, const Rect* coords, RectF mediabox, float destX, float destY);

// Bibliography / glossary / abbreviation entry box. Tries bracket-style
// ("[Foo+09]"), hanging-indent author-year, and single-line description-list
// layouts. Falls back to LandscapeBox when the destination doesn't look like
// a list entry.
//
// continuationOut, if non-null, is set to a second box when a bracket-style
// entry runs off the bottom of its 2-column-layout column with no natural
// close and continues at the top of the next column (e.g. "[63]"-style
// entries wrapping across a column break); empty when there's no such
// continuation. Ignored (left untouched) by callers that don't need it.
RectF DetectEntryBox(WStr text, const Rect* coords, RectF mediabox, float destX, float destY,
                     RectF* continuationOut = nullptr);
