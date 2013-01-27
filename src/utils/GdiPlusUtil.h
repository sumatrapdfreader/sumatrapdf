/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef GdiPlusUtil_h
#define GdiPlusUtil_h

// used for communicating with DrawCloseButton()
#define BUTTON_HOVER_TEXT L"1"

// note: must write "using namespace Gdiplus;" before #include "GdiPlusUtil.h"
// this is to make sure we don't accidentally do that just by including this file

typedef RectF (* TextMeasureAlgorithm)(Graphics *g, Font *f, const WCHAR *s, size_t len);

RectF    MeasureTextAccurate(Graphics *g, Font *f, const WCHAR *s, size_t len);
RectF    MeasureTextAccurate2(Graphics *g, Font *f, const WCHAR *s, size_t len);
RectF    MeasureTextStandard(Graphics *g, Font *f, const WCHAR *s, size_t len);
RectF    MeasureTextQuick(Graphics *g, Font *f, const WCHAR *s, size_t len);
RectF    MeasureText(Graphics *g, Font *f, const WCHAR *s, size_t len=-1, TextMeasureAlgorithm algo=NULL);
REAL     GetSpaceDx(Graphics *g, Font *f, TextMeasureAlgorithm algo=NULL);
int      StringLenForWidth(Graphics *g, Font *f, const WCHAR *s, size_t len, float dx, TextMeasureAlgorithm algo=NULL);
void     DrawCloseButton(DRAWITEMSTRUCT *dis);

void     GetBaseTransform(Matrix& m, RectF pageRect, float zoom, int rotation);

const WCHAR * GfxFileExtFromData(const char *data, size_t len);
bool          IsGdiPlusNativeFormat(const char *data, size_t len);
Bitmap *      BitmapFromData(const char *data, size_t len);
Size          BitmapSizeFromData(const char *data, size_t len);

#endif
