/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"
#include "utils/Log.h"

#include "DisplayMode.h"
#include "SettingsStructs.h"
#include "GlobalPrefs.h"

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
    if ((ZOOM_MIN - 0.01f <= zoom) && (zoom <= ZOOM_MAX + 0.01f)) {
        return true;
    }
    if (ZOOM_FIT_PAGE == zoom) {
        return true;
    }
    if (ZOOM_FIT_WIDTH == zoom) {
        return true;
    }
    if (ZOOM_FIT_CONTENT == zoom) {
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
        return ZOOM_FIT_PAGE;
    }
    if (str::EqIS(s, "fit width")) {
        return ZOOM_FIT_WIDTH;
    }
    if (str::EqIS(s, "fit content")) {
        return ZOOM_FIT_CONTENT;
    }
    float zoom;
    if (str::Parse(s, "%f", &zoom) && IsValidZoom(zoom)) {
        return zoom;
    }
    return defVal;
}

void ZoomToString(char** dst, float zoom, DisplayState* stateForIssue2140) {
    float prevZoom = *dst ? ZoomFromString(*dst, INVALID_ZOOM) : INVALID_ZOOM;
    if (prevZoom == zoom) {
        return;
    }
    if (!IsValidZoom(zoom) && stateForIssue2140) {
        // TODO: does issue 2140 still occur?
        logf("Invalid ds->zoom: %g\n", zoom);
        const WCHAR* ext = path::GetExtNoFree(stateForIssue2140->filePath);
        if (!str::IsEmpty(ext)) {
            AutoFree extA(strconv::WstrToUtf8(ext));
            logf("File type: %s\n", extA.Get());
        }
        logf("DisplayMode: %S\n", stateForIssue2140->displayMode);
        logf("PageNo: %d\n", stateForIssue2140->pageNo);
    }
    CrashIf(!IsValidZoom(zoom));
    free(*dst);
    if (ZOOM_FIT_PAGE == zoom) {
        *dst = str::Dup("fit page");
    } else if (ZOOM_FIT_WIDTH == zoom) {
        *dst = str::Dup("fit width");
    } else if (ZOOM_FIT_CONTENT == zoom) {
        *dst = str::Dup("fit content");
    } else {
        *dst = str::Format("%g", zoom);
    }
}
