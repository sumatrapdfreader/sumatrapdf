/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef DocumentLayout_h
#define DocumentLayout_h

enum class DisplayMode;

struct DocumentLayoutMargin {
    int top = 0;
    int right = 0;
    int bottom = 0;
    int left = 0;
};

struct DocumentLayoutPage {
    RectF mediaBox{};
    Rect pos{};
    Rect pageOnScreen{};
    float visibleRatio = 0;
    float zoomReal = 1;
    bool isShown = false;
};

struct DocumentLayoutParams {
    DisplayMode displayMode{};
    int startPage = 1;
    Size viewPortSize{};
    Point viewPortOffset{};
    float zoomVirtual = 100;
    float dpiFactor = 1;
    int rotation = 0;
    bool displayR2L = false;
    bool usePageZooms = false;
    DocumentLayoutMargin windowMargin{};
    Size pageSpacing{};
};

struct DocumentLayout {
    Vec<DocumentLayoutPage> pages;
    DocumentLayoutParams params;
    Size canvasSize{};
    Rect viewPort{};
    float zoomReal = 1;

    void Reset(int pageCount);
    bool ValidPageNo(int pageNo) const;
    void SetPageMediaBox(int pageNo, RectF mediaBox);
    DocumentLayoutPage* GetPage(int pageNo);
    const DocumentLayoutPage* GetPage(int pageNo) const;
    void Relayout(const DocumentLayoutParams& params);
    void RecalcVisibleParts();
    int CurrentPageNo() const;
    int FirstVisiblePageNo() const;
};

#endif
