/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef SvgPath_h
#define SvgPath_h

namespace svg {

#include "GdiPlusUtil.h"

GraphicsPath *GraphicsPathFromPathData(const char *s);

}

#endif
