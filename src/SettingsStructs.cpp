/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"
#include "utils/Log.h"

#include "SettingsStructs.h"
#include "GlobalPrefs.h"

namespace prefs {
namespace conv {

#define DM_AUTOMATIC_STR "automatic"
#define DM_SINGLE_PAGE_STR "single page"
#define DM_FACING_STR "facing"
#define DM_BOOK_VIEW_STR "book view"
#define DM_CONTINUOUS_STR "continuous"
#define DM_CONTINUOUS_FACING_STR "continuous facing"
#define DM_CONTINUOUS_BOOK_VIEW_STR "continuous book view"

#define STR_FROM_ENUM(val)      \
    if (val == mode)            \
        return TEXT(val##_STR); \
    else                        \
        NoOp()

const WCHAR* FromDisplayMode(DisplayMode mode) {
    STR_FROM_ENUM(DM_AUTOMATIC);
    STR_FROM_ENUM(DM_SINGLE_PAGE);
    STR_FROM_ENUM(DM_FACING);
    STR_FROM_ENUM(DM_BOOK_VIEW);
    STR_FROM_ENUM(DM_CONTINUOUS);
    STR_FROM_ENUM(DM_CONTINUOUS_FACING);
    STR_FROM_ENUM(DM_CONTINUOUS_BOOK_VIEW);
    CrashIf(true);
    return L"unknown display mode!?";
}

#undef STR_FROM_ENUM

#define IS_STR_ENUM(enumName)               \
    if (str::EqIS(s, TEXT(enumName##_STR))) \
        return enumName;                    \
    else                                    \
        NoOp()

DisplayMode ToDisplayMode(const WCHAR* s, DisplayMode defVal) {
    IS_STR_ENUM(DM_AUTOMATIC);
    IS_STR_ENUM(DM_SINGLE_PAGE);
    IS_STR_ENUM(DM_FACING);
    IS_STR_ENUM(DM_BOOK_VIEW);
    IS_STR_ENUM(DM_CONTINUOUS);
    IS_STR_ENUM(DM_CONTINUOUS_FACING);
    IS_STR_ENUM(DM_CONTINUOUS_BOOK_VIEW);
    // for consistency ("continuous" is used instead in the settings instead for brevity)
    if (str::EqIS(s, L"continuous single page"))
        return DM_CONTINUOUS;
    return defVal;
}

#undef IS_STR_ENUM

void FromZoom(char** dst, float zoom, DisplayState* stateForIssue2140) {
    float prevZoom = *dst ? ToZoom(*dst, INVALID_ZOOM) : INVALID_ZOOM;
    if (prevZoom == zoom)
        return;
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
    if (ZOOM_FIT_PAGE == zoom)
        *dst = str::Dup("fit page");
    else if (ZOOM_FIT_WIDTH == zoom)
        *dst = str::Dup("fit width");
    else if (ZOOM_FIT_CONTENT == zoom)
        *dst = str::Dup("fit content");
    else
        *dst = str::Format("%g", zoom);
}

float ToZoom(const char* s, float defVal) {
    if (str::EqIS(s, "fit page"))
        return ZOOM_FIT_PAGE;
    if (str::EqIS(s, "fit width"))
        return ZOOM_FIT_WIDTH;
    if (str::EqIS(s, "fit content"))
        return ZOOM_FIT_CONTENT;
    float zoom;
    if (str::Parse(s, "%f", &zoom) && IsValidZoom(zoom))
        return zoom;
    return defVal;
}

}; // namespace conv
}; // namespace prefs
