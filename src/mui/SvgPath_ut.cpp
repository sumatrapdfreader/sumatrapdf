/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: only meant to be #included from SvgPath.cpp,
// not compiled on its own

#include "utils/BaseUtil.h"
#include "SvgPath.h"
#include "utils/VecSegmented.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

using namespace Gdiplus;

static void SvgPath00() {
    const char* paths[] = {"M0 0  L10 13 L0 ,26 Z", "M10 0 L0,  13 L10 26 z"};
    for (size_t i = 0; i < dimof(paths); i++) {
        GraphicsPath* gp = svg::GraphicsPathFromPathData(paths[i]);
        utassert(gp);
        ::delete gp;
    }
}

void SvgPath_UnitTests() {
    SvgPath00();
}
