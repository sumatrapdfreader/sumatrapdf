/* Copyright 2006-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef GdiPlusUtil_h
#define GdiPlusUtil_h

#include "BaseUtil.h"

// a simple cache of font metrics for a single font
// we only cache metrics for characters in 32..255 range
struct FontMetricsCache {
    Font *  font;
    // cached dx/dy metrics for each character
    // cache is updated lazily, -1 means we don't yet
    // have the metrics for that character
    float   dx[256-32];
    float   dy[256-32];
};

void InitFontMetricsCache(FontMetricsCache *metrics, Graphics *gfx, Font *font);

// not: must write "using namespace Gdipluls" before #include "GdiPlusUtil.h"
// this is to make sure we don't accidentally do that just by including this file
RectF    MeasureTextAccurate(Graphics *g, Font *f, const WCHAR *s, size_t len);
RectF    MeasureTextAccurate2(Graphics *g, Font *f, const WCHAR *s, size_t len);
RectF    MeasureTextStandard(Graphics *g, Font *f, const WCHAR *s, size_t len);
RectF    MeasureText(Graphics *g, Font *f, const WCHAR *s, size_t len = -1);
RectF    MeasureText(Graphics *g, Font *f, FontMetricsCache *fontMetrics, const WCHAR *s, size_t len = -1);
REAL     GetSpaceDx(Graphics *g, Font *f);

const TCHAR *GfxFileExtFromData(char *data, size_t len);
Bitmap * BitmapFromData(void *data, size_t len);
Rect BitmapSizeFromData(char *data, size_t len);

#endif
