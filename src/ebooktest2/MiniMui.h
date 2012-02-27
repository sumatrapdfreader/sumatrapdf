/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Mui_h
#define Mui_h

// as little of mui as possible to make ../PageLayout compile

#include "BaseUtil.h"

using namespace Gdiplus;

namespace mui {

Font *GetCachedFont(const WCHAR *name, float size, FontStyle style);
Graphics *AllocGraphicsForMeasureText();
void FreeGraphicsForMeasureText(Graphics *g);

};

void InitGraphicsMode(Graphics *g);

#endif
