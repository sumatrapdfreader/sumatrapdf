/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// chapter-aware page location. 1-based in both fields; 0 means invalid
struct Location {
    int chapter = 0;
    int page = 0;

    bool IsValid() const { return chapter >= 1 && page >= 1; }
    bool operator==(const Location& o) const { return chapter == o.chapter && page == o.page; }
    bool operator!=(const Location& o) const { return !(*this == o); }
};

// single-chapter docs
inline Location LocFromPageNo(int pageNo) {
    return {1, pageNo};
}

constexpr Location kInvalidLocation{};

// per-chapter page counts for a document whose chapters may lay out lazily.
// An unlaid chapter counts as 1 placeholder page; SetPageCount() replaces the
// placeholder with the real count and bumps the generation, so code holding a
// flat page number (DisplayModel, RenderCache, ...) knows to resync.
// Chapters are 1-based; nChapters <= 1 still yields exactly one chapter.
struct ChapterTable {
    void Init(int nChapters);
    void SetPageCount(int chapter, int n);
    int ChapterCount();
    int TotalPages();
    int PageCount(int chapter);
    bool IsLaidOut(int chapter);
    Location LocationFromPageNo(int pageNo);
    int PageNoFromLocation(Location loc);
    int Generation();
    void Reset();

  private:
    Mutex mutex;
    Vec<int> pageCounts;
    Vec<bool> laidOut;
    Vec<int> cumPages; // cumPages[i] = total pages through chapter i+1, inclusive
    AtomicInt generation = 0;

    void RebuildLocked();
};
