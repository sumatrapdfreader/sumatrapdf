/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "ChapterTable.h"

int ChapterTable::CountLocked(int idx) {
    int before = idx == 0 ? 0 : entries[idx - 1].endPage;
    return entries[idx].endPage - before;
}

void ChapterTable::ResetLocked() {
    for (int i = 0; i < len(entries); i++) {
        entries[i] = {i + 1, false};
    }
    AtomicIntInc(&generation);
}

// nChapters <= 1 still creates one chapter, so callers can always route page
// lookups through the table even for a single-chapter document
void ChapterTable::Init(int nChapters) {
    int n = nChapters < 1 ? 1 : nChapters;
    AutoUnlockMutex scope(&mutex);
    VecResize(entries, n);
    ResetLocked();
}

void ChapterTable::SetPageCount(int chapter, int n) {
    AutoUnlockMutex scope(&mutex);
    if (chapter < 1 || chapter > len(entries)) {
        ReportIf(true);
        return;
    }
    if (n < 1) {
        ReportIf(true);
        n = 1;
    }
    int idx = chapter - 1;
    int delta = n - CountLocked(idx);
    entries[idx].laidOut = true;
    if (delta == 0) {
        return;
    }
    for (int i = idx; i < len(entries); i++) {
        entries[i].endPage += delta;
    }
    AtomicIntInc(&generation);
}

int ChapterTable::ChapterCount() {
    AutoUnlockMutex scope(&mutex);
    return len(entries);
}

int ChapterTable::TotalPages() {
    AutoUnlockMutex scope(&mutex);
    int n = len(entries);
    return n == 0 ? 0 : entries[n - 1].endPage;
}

int ChapterTable::PageCount(int chapter) {
    AutoUnlockMutex scope(&mutex);
    if (chapter < 1 || chapter > len(entries)) {
        ReportIf(true);
        return 0;
    }
    return CountLocked(chapter - 1);
}

bool ChapterTable::IsLaidOut(int chapter) {
    AutoUnlockMutex scope(&mutex);
    if (chapter < 1 || chapter > len(entries)) {
        ReportIf(true);
        return false;
    }
    return entries[chapter - 1].laidOut;
}

Location ChapterTable::LocationFromPageNo(int pageNo) {
    AutoUnlockMutex scope(&mutex);
    int n = len(entries);
    if (pageNo < 1 || n == 0 || pageNo > entries[n - 1].endPage) {
        return kInvalidLocation;
    }
    // smallest chapter index whose cumulative total reaches pageNo
    int lo = 0, hi = n - 1;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (entries[mid].endPage >= pageNo) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    int before = lo == 0 ? 0 : entries[lo - 1].endPage;
    return {lo + 1, pageNo - before};
}

int ChapterTable::PageNoFromLocation(Location loc) {
    AutoUnlockMutex scope(&mutex);
    int chapter = loc.chapter;
    if (chapter < 1 || chapter > len(entries)) {
        return 0;
    }
    int idx = chapter - 1;
    int count = CountLocked(idx);
    int page = ClampI(loc.page, 1, count);
    int before = idx == 0 ? 0 : entries[idx - 1].endPage;
    return before + page;
}

int ChapterTable::Generation() {
    return AtomicIntGet(&generation);
}

// the pages changed without their count changing, e.g. reordered
void ChapterTable::BumpGeneration() {
    AtomicIntInc(&generation);
}

void ChapterTable::Reset() {
    AutoUnlockMutex scope(&mutex);
    ResetLocked();
}
