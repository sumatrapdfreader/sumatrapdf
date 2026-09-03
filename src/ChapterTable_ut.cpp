/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "ChapterTable.h"

#include "base/UtAssert.h"

void ChapterTable_UnitTests() {
    // Init(3): 3 placeholder chapters, 1 page each, none laid out
    {
        ChapterTable t;
        t.Init(3);
        utassert(t.ChapterCount() == 3);
        utassert(t.TotalPages() == 3);
        utassert(!t.IsLaidOut(1));
        utassert(!t.IsLaidOut(2));
        utassert(!t.IsLaidOut(3));
        utassert(t.PageCount(1) == 1);
        utassert(t.PageCount(2) == 1);
        utassert(t.PageCount(3) == 1);
    }

    // SetPageCount grows the total and marks the chapter laid out; location /
    // page-number round trips across the shifted chapter boundaries
    {
        ChapterTable t;
        t.Init(3);
        int gen0 = t.Generation();
        t.SetPageCount(1, 4);
        utassert(t.IsLaidOut(1));
        utassert(t.PageCount(1) == 4);
        utassert(t.TotalPages() == 6); // 4 + 1 + 1
        utassert(t.Generation() == gen0 + 1);

        // chapter 1: pages 1..4 -> pageNo 1..4
        for (int p = 1; p <= 4; p++) {
            int pageNo = t.PageNoFromLocation({1, p});
            utassert(pageNo == p);
            Location loc = t.LocationFromPageNo(pageNo);
            utassert(loc.chapter == 1 && loc.page == p);
        }
        // chapter 2, page 1 -> pageNo 5
        utassert(t.PageNoFromLocation({2, 1}) == 5);
        Location loc = t.LocationFromPageNo(5);
        utassert(loc.chapter == 2 && loc.page == 1);
        // chapter 3, page 1 -> pageNo 6 (last page)
        utassert(t.PageNoFromLocation({3, 1}) == 6);
        loc = t.LocationFromPageNo(6);
        utassert(loc.chapter == 3 && loc.page == 1);
    }

    // generation increments only when the count actually changes
    {
        ChapterTable t;
        t.Init(2);
        int gen0 = t.Generation();
        t.SetPageCount(1, 1); // same as placeholder: no numeric change
        utassert(t.Generation() == gen0);
        utassert(t.IsLaidOut(1));
        t.SetPageCount(1, 5); // real change
        utassert(t.Generation() == gen0 + 1);
        t.SetPageCount(1, 5); // set again to the same value
        utassert(t.Generation() == gen0 + 1);
    }

    // out-of-range lookups
    {
        ChapterTable t;
        t.Init(2);
        t.SetPageCount(1, 3); // total = 4
        utassert(!t.LocationFromPageNo(0).IsValid());
        utassert(!t.LocationFromPageNo(5).IsValid());
        utassert(t.PageNoFromLocation({0, 1}) == 0);
        utassert(t.PageNoFromLocation({3, 1}) == 0); // no chapter 3
        // page beyond the chapter's count clamps to the last page
        utassert(t.PageNoFromLocation({1, 99}) == 3);
    }

    // Reset: unlays every chapter again and bumps the generation
    {
        ChapterTable t;
        t.Init(2);
        t.SetPageCount(1, 4);
        t.SetPageCount(2, 3);
        int gen0 = t.Generation();
        t.Reset();
        utassert(t.Generation() == gen0 + 1);
        utassert(t.ChapterCount() == 2);
        utassert(t.TotalPages() == 2);
        utassert(!t.IsLaidOut(1));
        utassert(!t.IsLaidOut(2));
        utassert(t.PageCount(1) == 1);
        utassert(t.PageCount(2) == 1);
    }

    // nChapters <= 1 still yields exactly one working chapter
    {
        ChapterTable t;
        t.Init(0);
        utassert(t.ChapterCount() == 1);
        utassert(t.TotalPages() == 1);
        t.SetPageCount(1, 10);
        utassert(t.TotalPages() == 10);
        utassert(t.PageNoFromLocation({1, 10}) == 10);
        Location loc = t.LocationFromPageNo(10);
        utassert(loc.chapter == 1 && loc.page == 10);
    }
}
