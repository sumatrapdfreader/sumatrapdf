/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

enum class DisplayMode;

struct DocumentLayoutMargin {
    int top = 0;
    int right = 0;
    int bottom = 0;
    int left = 0;
};

struct DocumentLayoutPage {
    RectF mediaBox;
    Rect pos;
    Rect pageOnScreen;
    float visibleRatio = 0;
    float zoomReal = 1;
    bool isShown = false;
};

struct DocumentLayoutParams {
    DisplayMode displayMode{};
    int startPage = 1;
    Size viewPortSize;
    Point viewPortOffset;
    float zoomVirtual = 100;
    float dpiFactor = 1;
    int rotation = 0;
    bool displayR2L = false;
    bool usePageZooms = false;
    // extra scroll room after last page in continuous view (issue #411)
    bool paddingAfterLastPage = false;
    // comic/image facing and book view: a page wider than it is tall occupies
    // the whole two-page row (issues #1324, #872)
    bool landscapeAsSpread = false;
    Vec<u8> spreadFlags;
    DocumentLayoutMargin windowMargin{};
    Size pageSpacing;
};

struct FacingRow {
    int firstPage = 1;
    int lastPage = 1;
    bool isSpread = false;
};

void CollectFacingRows(Vec<FacingRow>& out, int pageCount, bool bookView, const Vec<u8>& spreadFlags);

struct DocumentLayout {
    Vec<DocumentLayoutPage> pages;
    DocumentLayoutParams params;
    Size canvasSize;
    Rect viewPort;
    float zoomReal = 1;

    void Reset(int pageCount);
    bool ValidPageNo(int pageNo) const;
    void SetPageMediaBox(int pageNo, RectF mediaBox);
    DocumentLayoutPage* GetPage(int pageNo);
    const DocumentLayoutPage* GetPage(int pageNo) const;
    void Relayout(const DocumentLayoutParams& params);
    void RecalcVisibleParts();
    int CurrentPageNo() const;
    int PageNoAtViewPortTop() const;
    int FirstVisiblePageNo() const;
};
