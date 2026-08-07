/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Flatten per-glyph ink boxes to uniform top-aligned line rows. mupdf reports
// tight per-glyph boxes whose tops vary within a line; the detectors below key
// off coords[i].y as a line coordinate, so callers must pass coords through
// this first (grouping by baseline = y+dy). `out` needs textLen rects and must
// not alias `coords`. Synthetic top-aligned input is left effectively
// unchanged (each line already has a single top).
void NormalizeGlyphLines(const Rect* coords, Rect* out, int glyphCount);

int StripWatermarkGlyphs(WStr text, const Rect* coords, WCHAR* outText, Rect* outCoords);

RectF LandscapeBox(RectF mediabox, float destX, float destY, WStr text, const Rect* coords);

RectF DetectEquationBox(WStr text, const Rect* coords, RectF mediabox, float destX, float destY);

RectF DetectEntryBox(WStr text, const Rect* coords, RectF mediabox, float destX, float destY,
                     RectF* continuationOut = nullptr);
