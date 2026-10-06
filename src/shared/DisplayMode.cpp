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

static const struct {
    float value;
    Str name;
} zoomModes[] = {
    {kZoomFitPage, StrL("fit page")},
    {kZoomFitWidth, StrL("fit width")},
    {kZoomFitHeight, StrL("fit height")},
    {kZoomFitContent, StrL("fit content")},
    {kZoomFitVisible, StrL("fit visible")},
    {kZoomShrinkToFit, StrL("shrink to fit")},
    {kZoomFitByOrientation, StrL("fit by orientation")},
};

bool IsValidZoom(float zoom) {
    if (kZoomMin - 0.01f <= zoom && zoom <= kZoomMax + 0.01f) {
        return true;
    }
    for (const auto& mode : zoomModes) {
        if (zoom == mode.value) {
            return true;
        }
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
    // Accept the legacy long name for continuous mode.
    int idx = str::EqIS(s, StrL("continuous single page")) ? (int)DisplayMode::Continuous
                                                           : SeqStrIndexIS(displayModeNames, s);
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

bool GetPageAspectView(RectF page, DisplayMode* modeOut, float* zoomOut) {
    if (page.dx <= 0 || page.dy <= 0 || !modeOut || !zoomOut) {
        return false;
    }
    if (page.dx > page.dy) {
        *modeOut = DisplayMode::SinglePage;
        *zoomOut = kZoomFitPage;
    } else {
        *modeOut = DisplayMode::Continuous;
        *zoomOut = kZoomFitWidth;
    }
    return true;
}

DisplayMode DisplayModeFromString(Str s, DisplayMode defVal) {
    DisplayMode mode;
    return TryParseDisplayMode(s, &mode) ? mode : defVal;
}

float ZoomFromString(Str s, float defVal) {
    for (const auto& mode : zoomModes) {
        if (str::EqIS(s, mode.name)) {
            return mode.value;
        }
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
    for (const auto& mode : zoomModes) {
        if (zoom == mode.value) {
            str::ReplaceWithCopy(dst, mode.name);
            return;
        }
    }
    str::ReplaceWithCopy(dst, fmt("%g", zoom));
}

#if IS_DEBUG
bool DisplayMode_UnitTestZoom() {
    const float values[] = {kZoomFitPage,    kZoomFitWidth,    kZoomFitHeight,       kZoomFitContent,
                            kZoomFitVisible, kZoomShrinkToFit, kZoomFitByOrientation};
    for (float value : values) {
        Str name;
        ZoomToString(&name, value, nullptr);
        bool ok = IsValidZoom(value) && ZoomFromString(name, kInvalidZoom) == value;
        str::Free(name);
        if (!ok) {
            return false;
        }
    }
    Str numeric;
    ZoomToString(&numeric, 125, nullptr);
    bool ok = str::Eq(numeric, StrL("125"));
    str::Free(numeric);
    return ok && ZoomFromString(StrL("  FIT HEIGHT  "), 0) == kZoomFitHeight && ZoomFromString(StrL("125"), 0) == 125 &&
           ZoomFromString(StrL("unknown"), 125) == 125 && ZoomFromString(StrL("0"), 125) == 125 && !IsValidZoom(0) &&
           !IsValidZoom(kInvalidZoom) && IsValidZoom(kZoomMin) && IsValidZoom(kZoomMax);
}
#endif
