/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// ng: MacTypes.h (pulled in by CoreFoundation) defines Point, Rect and Size,
// which are our types. Include this immediately before an Apple framework
// header and base/MacTypesShow.h immediately after it.

#pragma push_macro("Point")
#pragma push_macro("Rect")
#pragma push_macro("Size")
#undef Point
#undef Rect
#undef Size
#define Point MacQuickDrawPoint
#define Rect MacQuickDrawRect
#define Size MacQuickDrawSize
