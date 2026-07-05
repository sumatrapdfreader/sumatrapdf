/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"

#include "Settings.h"


bool IsSingle(DisplayMode mode) {
    return DisplayMode::SinglePage == mode || DisplayMode::Continuous == mode;
}

bool IsContinuous(DisplayMode mode) {
    switch (mode) {
        case DisplayMode::Continuous:
        case DisplayMode::ContinuousFacing:
        case DisplayMode::ContinuousBookView:
            return true;
    }
    return false;
}

bool IsFacing(DisplayMode mode) {
    return DisplayMode::Facing == mode || DisplayMode::ContinuousFacing == mode;
}

bool IsBookView(DisplayMode mode) {
    return DisplayMode::BookView == mode || DisplayMode::ContinuousBookView == mode;
}

bool IsValidZoom(float zoom) {
    if ((kZoomMin - 0.01f <= zoom) && (zoom <= kZoomMax + 0.01f)) {
        return true;
    }
    if (kZoomFitPage == zoom) {
        return true;
    }
    if (kZoomFitWidth == zoom) {
        return true;
    }
    if (kZoomFitContent == zoom) {
        return true;
    }
    if (kZoomShrinkToFit == zoom) {
        return true;
    }
    if (kZoomFitByOrientation == zoom) {
        return true;
    }
    return false;
}

// must match order of enum DisplayMode
static SeqStrings displayModeNames =
    "automatic\0"
    "single page\0"
    "facing\0"
    "book view\0"
    "continuous\0"
    "continuous facing\0"
    "continuous book view\0";

Str DisplayModeToString(DisplayMode mode) {
    int idx = (int)mode;
    Str s = SeqStrByIndex(displayModeNames, idx);
    if (!s) {
        ReportIf(true);
        return StrL("unknown display mode");
    }
    return s;
}

DisplayMode DisplayModeFromString(Str s, DisplayMode defVal) {
    // for consistency ("continuous" is used instead in the settings instead for brevity)
    if (str::EqIS(s, StrL("continuous single page"))) {
        return DisplayMode::Continuous;
    }
    int idx = SeqStrIndexIS(displayModeNames, s);
    if (idx < 0) {
        return defVal;
    }
    return (DisplayMode)idx;
}

float ZoomFromString(Str s, float defVal) {
    if (str::EqIS(s, StrL("fit page"))) {
        return kZoomFitPage;
    }
    if (str::EqIS(s, StrL("fit width"))) {
        return kZoomFitWidth;
    }
    if (str::EqIS(s, StrL("fit content"))) {
        return kZoomFitContent;
    }
    if (str::EqIS(s, StrL("shrink to fit"))) {
        return kZoomShrinkToFit;
    }
    if (str::EqIS(s, StrL("fit by orientation"))) {
        return kZoomFitByOrientation;
    }
    float zoom;
    if (!str::IsNull(str::Parse(s, "%f", &zoom)) && IsValidZoom(zoom)) {
        return zoom;
    }
    return defVal;
}

void ZoomToString(Str* dst, float zoom, FileState* fileState) {
    float prevZoom = dst->s ? ZoomFromString(dst->s, kInvalidZoom) : kInvalidZoom;
    if (prevZoom == zoom) {
        return;
    }
    if (!IsValidZoom(zoom) && fileState) {
        logf("Invalid ds->zoom: %g\n", zoom);
        TempStr ext = path::GetExtTemp(fileState->filePath);
        if (len(ext) > 0) {
            logf("File type: %s\n", ext);
        }
        logf("DisplayMode: %s\n", fileState->displayMode);
        logf("PageNo: %d\n", fileState->pageNo);
    }
    ReportIf(!IsValidZoom(zoom));
    if (kZoomFitPage == zoom) {
        str::ReplaceWithCopy(dst, "fit page");
    } else if (kZoomFitWidth == zoom) {
        str::ReplaceWithCopy(dst, "fit width");
    } else if (kZoomFitContent == zoom) {
        str::ReplaceWithCopy(dst, "fit content");
    } else if (kZoomShrinkToFit == zoom) {
        str::ReplaceWithCopy(dst, "shrink to fit");
    } else if (kZoomFitByOrientation == zoom) {
        str::ReplaceWithCopy(dst, "fit by orientation");
    } else {
        str::ReplaceWithCopy(dst, fmt("%g", zoom));
    }
}
