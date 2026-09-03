/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "ChapterTable.h"

// caller holds mutex. Recomputes the cumulative-pages prefix sum from pageCounts.
void ChapterTable::RebuildLocked() {
    int n = len(pageCounts);
    VecResize(cumPages, n);
    int total = 0;
    for (int i = 0; i < n; i++) {
        total += pageCounts[i];
        cumPages[i] = total;
    }
}

// nChapters <= 1 still creates one chapter, so callers can always route page
// lookups through the table even for a single-chapter document
void ChapterTable::Init(int nChapters) {
    int n = nChapters < 1 ? 1 : nChapters;
    ScopedMutex scope(&mutex);
    VecResize(pageCounts, n);
    VecResize(laidOut, n);
    for (int i = 0; i < n; i++) {
        pageCounts[i] = 1;
        laidOut[i] = false;
    }
    RebuildLocked();
    AtomicIntInc(&generation);
}

void ChapterTable::SetPageCount(int chapter, int n) {
    ScopedMutex scope(&mutex);
    if (chapter < 1 || chapter > len(pageCounts)) {
        ReportIf(true);
        return;
    }
    if (n < 1) {
        ReportIf(true);
        n = 1;
    }
    int idx = chapter - 1;
    bool changed = pageCounts[idx] != n;
    pageCounts[idx] = n;
    laidOut[idx] = true;
    if (changed) {
        RebuildLocked();
        AtomicIntInc(&generation);
    }
}

int ChapterTable::ChapterCount() {
    ScopedMutex scope(&mutex);
    return len(pageCounts);
}

int ChapterTable::TotalPages() {
    ScopedMutex scope(&mutex);
    int n = len(cumPages);
    return n == 0 ? 0 : cumPages[n - 1];
}

int ChapterTable::PageCount(int chapter) {
    ScopedMutex scope(&mutex);
    if (chapter < 1 || chapter > len(pageCounts)) {
        ReportIf(true);
        return 0;
    }
    return pageCounts[chapter - 1];
}

bool ChapterTable::IsLaidOut(int chapter) {
    ScopedMutex scope(&mutex);
    if (chapter < 1 || chapter > len(laidOut)) {
        ReportIf(true);
        return false;
    }
    return laidOut[chapter - 1];
}

Location ChapterTable::LocationFromPageNo(int pageNo) {
    ScopedMutex scope(&mutex);
    int n = len(cumPages);
    if (pageNo < 1 || n == 0 || pageNo > cumPages[n - 1]) {
        return kInvalidLocation;
    }
    // smallest chapter index whose cumulative total reaches pageNo
    int lo = 0, hi = n - 1;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (cumPages[mid] >= pageNo) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    int before = lo == 0 ? 0 : cumPages[lo - 1];
    return {lo + 1, pageNo - before};
}

int ChapterTable::PageNoFromLocation(Location loc) {
    ScopedMutex scope(&mutex);
    int chapter = loc.chapter;
    if (chapter < 1 || chapter > len(pageCounts)) {
        return 0;
    }
    int idx = chapter - 1;
    int page = loc.page;
    if (page < 1) {
        page = 1;
    }
    int count = pageCounts[idx];
    if (page > count) {
        page = count;
    }
    int before = idx == 0 ? 0 : cumPages[idx - 1];
    return before + page;
}

int ChapterTable::Generation() {
    return AtomicIntGet(&generation);
}

void ChapterTable::Reset() {
    ScopedMutex scope(&mutex);
    int n = len(pageCounts);
    for (int i = 0; i < n; i++) {
        pageCounts[i] = 1;
        laidOut[i] = false;
    }
    RebuildLocked();
    AtomicIntInc(&generation);
}
