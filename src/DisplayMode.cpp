/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"

#include "Settings.h"
#include "DisplayMode.h"

bool IsSingle(DisplayMode mode) {
    return DisplayMode::SinglePage == mode || DisplayMode::Continuous == mode;
}

bool IsContinuous(DisplayMode mode) {
    return mode == DisplayMode::Continuous || mode == DisplayMode::ContinuousFacing ||
           mode == DisplayMode::ContinuousBookView;
}

bool IsFacing(DisplayMode mode) {
    return DisplayMode::Facing == mode || DisplayMode::ContinuousFacing == mode;
}

bool IsBookView(DisplayMode mode) {
    return DisplayMode::BookView == mode || DisplayMode::ContinuousBookView == mode;
}

// The highest zoom the user can ask for. kZoomMaxDefault (6400%) is enough for
// documents meant to be read, but not for ones meant to be examined, like large
// maps, so the largest level in the ZoomLevels setting raises it (issue #1195).
// Loading the settings is the only thing that changes it.
// The ceiling on that, kZoomMaxAllowed: a page is laid out in pixels as int and
// the coordinate math is float, so at 1000000% a 612pt wide page is 6.1 million
// pixels, which both still represent exactly. A document is more than one page,
// though, so DisplayModel lowers this further to what its canvas can hold
float kZoomMax = kZoomMaxDefault;

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
    if (kZoomFitHeight == zoom) {
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
    if (len(s) == 0) {
        ReportIf(true);
        return StrL("unknown display mode");
    }
    return s;
}

// Fills *modeOut and returns true when s is a recognized layout name.
// Empty / unknown strings return false (Fullscreen.DisplayMode uses this
// so an unset setting means "don't change").
bool TryParseDisplayMode(Str s, DisplayMode* modeOut) {
    if (len(s) == 0) {
        return false;
    }
    // for consistency ("continuous" is used instead in the settings instead for brevity)
    if (str::EqIS(s, StrL("continuous single page"))) {
        if (modeOut) {
            *modeOut = DisplayMode::Continuous;
        }
        return true;
    }
    int idx = SeqStrIndexIS(displayModeNames, s);
    if (idx < 0) {
        return false;
    }
    if (modeOut) {
        *modeOut = (DisplayMode)idx;
    }
    return true;
}

// DefaultDisplayMode = page aspect: not a live layout, only a first-open
// picker (portrait -> continuous + fit width, landscape -> single page +
// fit page). Must not be added to displayModeNames / the DisplayMode enum.
bool IsPageAspectDisplayMode(Str s) {
    return str::EqIS(s, StrL("page aspect"));
}

DisplayMode DisplayModeFromString(Str s, DisplayMode defVal) {
    DisplayMode mode;
    if (TryParseDisplayMode(s, &mode)) {
        return mode;
    }
    return defVal;
}

float ZoomFromString(Str s, float defVal) {
    if (str::EqIS(s, StrL("fit page"))) {
        return kZoomFitPage;
    }
    if (str::EqIS(s, StrL("fit width"))) {
        return kZoomFitWidth;
    }
    if (str::EqIS(s, StrL("fit height"))) {
        return kZoomFitHeight;
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
    float prevZoom = dst->s ? ZoomFromString(Str(dst->s), kInvalidZoom) : kInvalidZoom;
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
        logf("PageNo: %s\n", fileState->pageNo);
    }
    ReportIf(!IsValidZoom(zoom));
    if (kZoomFitPage == zoom) {
        str::ReplaceWithCopy(dst, StrL("fit page"));
    } else if (kZoomFitWidth == zoom) {
        str::ReplaceWithCopy(dst, StrL("fit width"));
    } else if (kZoomFitHeight == zoom) {
        str::ReplaceWithCopy(dst, StrL("fit height"));
    } else if (kZoomFitContent == zoom) {
        str::ReplaceWithCopy(dst, StrL("fit content"));
    } else if (kZoomShrinkToFit == zoom) {
        str::ReplaceWithCopy(dst, StrL("shrink to fit"));
    } else if (kZoomFitByOrientation == zoom) {
        str::ReplaceWithCopy(dst, StrL("fit by orientation"));
    } else {
        str::ReplaceWithCopy(dst, fmt("%g", zoom));
    }
}
