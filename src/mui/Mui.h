/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct TxtNode;

using Gdiplus::FontStyle;
using Gdiplus::Graphics;

namespace mui {

#include "TextRender.h"

// set a consistent mode on a Graphics so that measuring and drawing text give
// the same results everywhere
void InitGraphicsMode(Graphics* g);

} // namespace mui
