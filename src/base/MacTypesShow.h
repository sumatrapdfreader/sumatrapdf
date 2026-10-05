/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// ng: pair of base/MacTypesHide.h. Restores Point, Rect and Size.

#undef Point
#undef Rect
#undef Size
#pragma pop_macro("Point")
#pragma pop_macro("Rect")
#pragma pop_macro("Size")
