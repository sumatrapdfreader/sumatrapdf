/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "GlobalPrefs.h"

#include "utils/Log.h"

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
    return false;
}

// must match order of enum DisplayMode
static const char* displayModeNames =
    "automatic\0"
    "single page\0"
    "facing\0"
    "book view\0"
    "continuous\0"
    "continuous facing\0"
    "continuous book view\0";

const char* DisplayModeToString(DisplayMode mode) {
    int idx = (int)mode;
    const char* s = seqstrings::IdxToStr(displayModeNames, idx);
    if (!s) {
        CrashIf(true);
        return "unknown display mode";
    }
    return s;
}

DisplayMode DisplayModeFromString(const char* s, DisplayMode defVal) {
    // for consistency ("continuous" is used instead in the settings instead for brevity)
    if (str::EqIS(s, "continuous single page")) {
        return DisplayMode::Continuous;
    }
    int idx = seqstrings::StrToIdxIS(displayModeNames, s);
    if (idx < 0) {
        return defVal;
    }
    return (DisplayMode)idx;
}

float ZoomFromString(const char* s, float defVal) {
    if (str::EqIS(s, "fit page")) {
        return kZoomFitPage;
    }
    if (str::EqIS(s, "fit width")) {
        return kZoomFitWidth;
    }
    if (str::EqIS(s, "fit content")) {
        return kZoomFitContent;
    }
    float zoom;
    if (str::Parse(s, "%f", &zoom) && IsValidZoom(zoom)) {
        return zoom;
    }
    return defVal;
}

void ZoomToString(char** dst, float zoom, FileState* stateForIssue2140) {
    float prevZoom = *dst ? ZoomFromString(*dst, kInvalidZoom) : kInvalidZoom;
    if (prevZoom == zoom) {
        return;
    }
    if (!IsValidZoom(zoom) && stateForIssue2140) {
        // TODO: does issue 2140 still occur?
        logf("Invalid ds->zoom: %g\n", zoom);
        TempStr ext = path::GetExtTemp(stateForIssue2140->filePath);
        if (!str::IsEmpty(ext)) {
            logf("File type: %s\n", ext);
        }
        logf("DisplayMode: %s\n", stateForIssue2140->displayMode);
        logf("PageNo: %d\n", stateForIssue2140->pageNo);
    }
    CrashIf(!IsValidZoom(zoom));
    str::FreePtr(dst);
    if (kZoomFitPage == zoom) {
        *dst = str::Dup("fit page");
    } else if (kZoomFitWidth == zoom) {
        *dst = str::Dup("fit width");
    } else if (kZoomFitContent == zoom) {
        *dst = str::Dup("fit content");
    } else {
        *dst = str::Format("%g", zoom);
    }
}
