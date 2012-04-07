/* Copyright 2006-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef GdiPlusUtil_h
#define GdiPlusUtil_h

// used for communicating with DrawCloseButton()
#define BUTTON_HOVER_TEXT _T("1")

// not: must write "using namespace Gdipluls" before #include "GdiPlusUtil.h"
// this is to make sure we don't accidentally do that just by including this file
RectF    MeasureTextAccurate(Graphics *g, Font *f, const WCHAR *s, size_t len);
RectF    MeasureTextAccurate2(Graphics *g, Font *f, const WCHAR *s, size_t len);
RectF    MeasureTextStandard(Graphics *g, Font *f, const WCHAR *s, size_t len);
RectF    MeasureText(Graphics *g, Font *f, const WCHAR *s, size_t len = -1);
REAL     GetSpaceDx(Graphics *g, Font *f);
int      StringLenForWidth(Graphics *g, Font *f, const WCHAR *s, size_t len, float dx);
void     DrawCloseButton(DRAWITEMSTRUCT *dis);

void     GetBaseTransform(Matrix& m, RectF pageRect, float zoom, int rotation);

const TCHAR * GfxFileExtFromData(char *data, size_t len);
Bitmap *      BitmapFromData(void *data, size_t len);
Rect          BitmapSizeFromData(char *data, size_t len);

unsigned char * SerializeRunLengthEncoded(HBITMAP hbmp, size_t *bmpBytesOut);

#endif
