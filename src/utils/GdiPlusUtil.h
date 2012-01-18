/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef GdiPlusUtil_h
#define GdiPlusUtil_h

#include "BaseUtil.h"

// not: must write "using namespace Gdipluls" before #include "GdiPlusUtil.h"
// this is to make sure we don't accidentally do that just by including this file
RectF    MeasureTextAccurate(Graphics *g, Font *f, const WCHAR *s, size_t len);
RectF    MeasureTextAccurate2(Graphics *g, Font *f, const WCHAR *s, size_t len);
RectF    MeasureTextStandard(Graphics *g, Font *f, const WCHAR *s, size_t len);
RectF    MeasureText(Graphics *g, Font *f, const WCHAR *s, size_t len);
REAL     GetSpaceDx(Graphics *g, Font *f);
Bitmap * BitmapFromData(void *data, size_t len);
const TCHAR *GfxFileExtFromData(char *data, size_t len);

#endif
