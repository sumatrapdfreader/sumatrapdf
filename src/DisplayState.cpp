/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "DisplayState.h"
#include "StrUtil.h"

void normalizeRotation(int *rotation)
{
    assert(rotation);
    if (!rotation) return;
    *rotation = *rotation % 360;
    if (*rotation < 0)
        *rotation += 360;
}

bool validRotation(int rotation)
{
    normalizeRotation(&rotation);
    if ((0 == rotation) || (90 == rotation) ||
        (180 == rotation) || (270 == rotation))
        return true;
    return false;
}

bool ValidZoomVirtual(float zoomVirtual)
{
    if ((ZOOM_FIT_PAGE == zoomVirtual) || (ZOOM_FIT_WIDTH == zoomVirtual) ||
        (ZOOM_FIT_CONTENT == zoomVirtual) || (ZOOM_ACTUAL_SIZE == zoomVirtual))
        return true;
    if ((zoomVirtual < ZOOM_MIN) || (zoomVirtual > ZOOM_MAX)) {
        DBG_OUT("ValidZoomVirtual() invalid zoom: %.4f\n", zoomVirtual);
        return false;
    }
    return true;
}

#define STR_FROM_ENUM(val) \
    if (val == var) \
        return val##_STR;

const char *DisplayModeNameFromEnum(DisplayMode var)
{
    STR_FROM_ENUM(DM_AUTOMATIC)
    STR_FROM_ENUM(DM_SINGLE_PAGE)
    STR_FROM_ENUM(DM_FACING)
    STR_FROM_ENUM(DM_BOOK_VIEW)
    STR_FROM_ENUM(DM_CONTINUOUS)
    STR_FROM_ENUM(DM_CONTINUOUS_FACING)
    STR_FROM_ENUM(DM_CONTINUOUS_BOOK_VIEW)
    return NULL;
}

#define IS_STR_ENUM(enumName) \
    if (Str::Eq(txt, enumName##_STR)) { \
        *resOut = enumName; \
        return true; \
    }

bool DisplayModeEnumFromName(const char *txt, DisplayMode *resOut)
{
    IS_STR_ENUM(DM_AUTOMATIC)
    IS_STR_ENUM(DM_SINGLE_PAGE)
    IS_STR_ENUM(DM_FACING)
    IS_STR_ENUM(DM_BOOK_VIEW)
    IS_STR_ENUM(DM_CONTINUOUS)
    IS_STR_ENUM(DM_CONTINUOUS_FACING)
    IS_STR_ENUM(DM_CONTINUOUS_BOOK_VIEW)
    assert(0);
    return false;
}
