/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/GuessFileType.h"

#include "gui/UIModels.h"

#include "base/SettingsUtil.h"
#include "Settings.h"
#include "AppSettings.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "Annotation.h"

// must be last to over-write assert()
#include "base/tests/UtAssert.h"

TempStr TestDocPathTemp(Str relPath);

// moving a bordered annotation must not grow its /Rect: the bounds include
// the border, so setting them as /Rect grew it on every move or nudge
static void SetRectMoveKeepsSize() {
    // creating an annotation reads the default author
    Settings* prevSettings = gSettings;
    if (!gSettings) {
        gSettings = NewSettings({});
    }
    TempStr path = TestDocPathTemp(StrL("docs/test/zlib.3.pdf"));
    EngineBase* engine = CreateEngineFromFile(path, nullptr, true);
    utassert(engine);
    if (!engine) {
        return;
    }
    AnnotCreateArgs args;
    args.annotType = AnnotationType::Square;
    args.borderWidth = 6;
    args.hasRect = true;
    args.rect = RectF(100, 100, 120, 60);
    Annotation* annot = EngineMupdfCreateAnnotation(engine, 1, PointF(100, 100), &args);
    utassert(annot);
    if (annot) {
        RectF rect = GetRect(annot);
        RectF bounds = GetBounds(annot);
        for (int i = 0; i < 10; i++) {
            RectF moved = GetBounds(annot);
            moved.x += 3;
            moved.y += 2;
            SetRect(annot, moved);
        }
        RectF rect2 = GetRect(annot);
        RectF bounds2 = GetBounds(annot);
        utassert(fabsf(rect2.dx - rect.dx) < 0.01f);
        utassert(fabsf(rect2.dy - rect.dy) < 0.01f);
        utassert(fabsf(bounds2.dx - bounds.dx) < 0.01f);
        utassert(fabsf(bounds2.dy - bounds.dy) < 0.01f);
        utassert(fabsf(rect2.x - rect.x - 30) < 0.01f);
        utassert(fabsf(rect2.y - rect.y - 20) < 0.01f);
    }
    SafeEngineRelease(&engine);
    if (!prevSettings) {
        DeleteSettings(gSettings);
        gSettings = nullptr;
    }
}

void Annotation_UnitTests() {
    SetRectMoveKeepsSize();
}
