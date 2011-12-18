/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef GdiPlusUtil_h
#define GdiPlusUtil_h

#include "BaseUtil.h"

using namespace Gdiplus;

RectF MeasureTextAccurate(Graphics *g, Font *f, const WCHAR *s, size_t len);
RectF MeasureTextStandard(Graphics *g, Font *f, const WCHAR *s, size_t len);
RectF MeasureText(Graphics *g, Font *f, const WCHAR *s, size_t len);
REAL GetSpaceDx(Graphics *g, Font *f);

#endif
