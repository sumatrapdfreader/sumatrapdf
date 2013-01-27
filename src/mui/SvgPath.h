/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef SvgPath_h
#define SvgPath_h

namespace svg {

using namespace Gdiplus;
#include "GdiPlusUtil.h"

GraphicsPath *GraphicsPathFromPathData(const char *s);

}

#endif
