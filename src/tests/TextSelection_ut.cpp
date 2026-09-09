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
    TextSel result;
    FillSelectionRects(&result, 1, coords, dimof(coords), 0, 10, {0, 0, 200, 200});
    utassert(result.len == 4);
    utassert(result.rects[0] == Rect(50, 100, 32, 10));
    utassert(result.rects[1] == Rect(56, 115, 32, 10));
    utassert(result.rects[2] == Rect(50, 130, 32, 10));
    utassert(result.rects[3] == Rect(56, 145, 10, 10));
    free(result.pages);
    free(result.rects);
    free(result.quads);

    Rect superscript[] = {{10, 100, 12, 10}, {20, 97, 8, 6}, {28, 100, 12, 10}};
    result = {};
    FillSelectionRects(&result, 1, superscript, dimof(superscript), 0, dimof(superscript), {0, 0, 200, 200});
    utassert(result.len == 1);
    utassert(result.rects[0] == Rect(10, 97, 30, 13));
    free(result.pages);
    free(result.rects);
    free(result.quads);

    // 45-degree run: keep per-glyph quads instead of one axis-aligned union
    Rect rotCoords[] = {{10, 10, 20, 20}, {20, 20, 20, 20}};
    QuadF rotQuads[] = {
        {PointF(10, 20), PointF(24, 10), PointF(20, 30), PointF(34, 20)},
        {PointF(20, 30), PointF(34, 20), PointF(30, 40), PointF(44, 30)},
    };
    result = {};
    FillSelectionRects(&result, 1, rotCoords, dimof(rotCoords), 0, dimof(rotCoords), {0, 0, 200, 200}, rotQuads);
    utassert(result.len == 2);
    utassert(result.quads);
    utassert(result.quads[0].ul.x == 10 && result.quads[0].ur.y == 10);
    utassert(result.quads[1].ul.x == 20 && result.quads[1].lr.x == 44);
    free(result.pages);
    free(result.rects);
    free(result.quads);
}
