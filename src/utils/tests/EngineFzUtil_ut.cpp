/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "utils/ScopedWin.h"

#include "wingui/TreeModel.h"

#include "Annotation.h"
#include "EngineBase.h"
#include "EngineFzUtil.h"

#include <float.h>
#include <math.h>

// must be last due to assert() over-write
#include "utils/UtAssert.h"

#define utassert_fequal(a, b) utassert(fabs(a - b) < FLT_EPSILON);

void EngineFzUtilTest() {
    float x, y, zoom = 0;
    int page;
    {
        page = resolve_link("#1", &x, &y, &zoom);
        utassert(page == 0);

        page = resolve_link("#1,2,3", &x, &y, &zoom);
        utassert(page == 0);
        utassert_fequal(x, 2);
        utassert_fequal(y, 3);
        utassert_fequal(zoom, 0);

        page = resolve_link("#1,2,3,4.56", &x, &y, &zoom);
        utassert(page == 0);
        utassert_fequal(x, 2);
        utassert_fequal(y, 3);
        utassert_fequal(zoom, 4.56);
    }
}
