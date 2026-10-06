/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "DocController.h"
#include "gui/UIModels.h"
#include "EngineBase.h"
#include "TextSelection.h"

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

void TextSelection_UnitTests() {
    Rect coords[] = {
        {50, 100, 12, 10}, {60, 100, 12, 10}, {70, 100, 12, 10}, {56, 115, 12, 10},
        {66, 115, 12, 10}, {76, 115, 12, 10}, {50, 130, 12, 10}, {60, 130, 12, 10},
        {70, 130, 12, 10}, {56, 145, 12, 10}, {66, 145, 12, 10}, {76, 145, 12, 10},
    };
    Vec<TextSel> result;
    FillSelectionRects(&result, 1, coords, dimof(coords), 0, 10, {0, 0, 200, 200});
    utassert(len(result) == 4);
    utassert(result[0].rect == Rect(50, 100, 32, 10));
    utassert(result[1].rect == Rect(56, 115, 32, 10));
    utassert(result[2].rect == Rect(50, 130, 32, 10));
    utassert(result[3].rect == Rect(56, 145, 10, 10));

    Rect superscript[] = {{10, 100, 12, 10}, {20, 97, 8, 6}, {28, 100, 12, 10}};
    VecClear(result);
    FillSelectionRects(&result, 1, superscript, dimof(superscript), 0, dimof(superscript), {0, 0, 200, 200});
    utassert(len(result) == 1);
    utassert(result[0].rect == Rect(10, 97, 30, 13));

    // 45-degree run: keep per-glyph quads instead of one axis-aligned union
    Rect rotCoords[] = {{10, 10, 20, 20}, {20, 20, 20, 20}};
    QuadF rotQuads[] = {
        {PointF(10, 20), PointF(24, 10), PointF(20, 30), PointF(34, 20)},
        {PointF(20, 30), PointF(34, 20), PointF(30, 40), PointF(44, 30)},
    };
    VecClear(result);
    FillSelectionRects(&result, 1, rotCoords, dimof(rotCoords), 0, dimof(rotCoords), {0, 0, 200, 200}, rotQuads);
    utassert(len(result) == 2);
    utassert(!result[0].quad.IsEmpty());
    utassert(result[0].quad.ul.x == 10 && result[0].quad.ur.y == 10);
    utassert(result[1].quad.ul.x == 20 && result[1].quad.lr.x == 44);

    FillSelectionRects(&result, 2, coords, dimof(coords), 0, 10, {0, 0, 200, 200});
    utassert(len(result) == 6 && result[0].pageNo == 1 && result[2].pageNo == 2);
    utassert(!result[0].quad.IsEmpty() && result[2].quad.IsEmpty());
    VecClear(result);
    FillSelectionRects(&result, 3, superscript, dimof(superscript), 0, dimof(superscript), {0, 0, 200, 200});
    utassert(len(result) == 1 && result[0].pageNo == 3 && result[0].quad.IsEmpty());

    // small rotated glyphs: distinct quads, but the rounded int bboxes coincide,
    // so the "all glyphs of a word share one bbox" (DjVu) rule must not apply
    Rect tinyCoords[] = {{10, 9, 3, 3}, {10, 9, 3, 3}};
    QuadF tinyQuads[] = {
        {PointF(10.0f, 10.0f), PointF(10.7f, 9.3f), PointF(11.4f, 11.4f), PointF(12.1f, 10.7f)},
        {PointF(10.7f, 9.3f), PointF(11.4f, 8.6f), PointF(12.1f, 10.7f), PointF(12.8f, 10.0f)},
    };
    // over the left half of glyph 1
    utassert(FindClosestGlyphIn(nullptr, 1, tinyCoords, tinyQuads, 2, 11.575, 9.825) == 1);
    // over the right half of glyph 0: glyph 1 is the first one to select
    utassert(FindClosestGlyphIn(nullptr, 1, tinyCoords, tinyQuads, 2, 11.225, 10.175) == 1);
}
