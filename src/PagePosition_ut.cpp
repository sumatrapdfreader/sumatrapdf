/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "PagePosition.h"

#include "base/UtAssert.h"

void PagePosition_UnitTests() {
    // plain int
    {
        StoredPagePos pos = ParseStoredPagePos(StrL("12"));
        utassert(pos.pageNo == 12);
        utassert(len(pos.bookmark) == 0);
    }

    // bookmark
    {
        StoredPagePos pos = ParseStoredPagePos(StrL("bm:3:5:20"));
        utassert(len(pos.bookmark) > 0);
        utassert(str::Eq(pos.bookmark, StrL("3:5:20")));
    }

    // garbage falls back to page 1
    {
        StoredPagePos pos = ParseStoredPagePos(StrL("not a number"));
        utassert(pos.pageNo == 1);
        utassert(len(pos.bookmark) == 0);

        pos = ParseStoredPagePos(StrL(""));
        utassert(pos.pageNo == 1);
        utassert(len(pos.bookmark) == 0);

        pos = ParseStoredPagePos(StrL("0"));
        utassert(pos.pageNo == 1);

        pos = ParseStoredPagePos(StrL("-5"));
        utassert(pos.pageNo == 1);
    }

    // formatting round trips
    {
        utassert(str::Eq(FormatStoredPagePosTemp(12), StrL("12")));
        utassert(str::Eq(FormatStoredBookmarkTemp(StrL("3:5:20")), StrL("bm:3:5:20")));
    }
}
