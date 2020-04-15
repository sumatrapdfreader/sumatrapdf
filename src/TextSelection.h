/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

inline unsigned int distSq(int x, int y) {
    return x * x + y * y;
}
// underscore is mainly used for programming and is thus considered a word character
inline bool isWordChar(WCHAR c) {
    return IsCharAlphaNumeric(c) || c == '_';
}

struct PageText {
    RectI* coords;
    WCHAR* text;
    int len;
};

struct DocumentTextCache {
    EngineBase* engine = nullptr;
    int nPages = 0;
    PageText* pagesText = nullptr;
    int debugSize;

    CRITICAL_SECTION access;

    explicit DocumentTextCache(EngineBase* engine);
    ~DocumentTextCache();

    bool HasData(int pageNo);
    const WCHAR* GetData(int pageNo, int* lenOut = nullptr, RectI** coordsOut = nullptr);
};

// TODO: replace with Vec<TextSel>
struct TextSel {
    int len = 0;
    int cap = 0;
    int* pages = nullptr;
    RectI* rects = nullptr;
};

struct TextSelection {
    TextSelection(EngineBase* engine, DocumentTextCache* textCache);
    ~TextSelection();

    bool IsOverGlyph(int pageNo, double x, double y);
    void StartAt(int pageNo, int glyphIx);
    void StartAt(int pageNo, double x, double y) {
        StartAt(pageNo, FindClosestGlyph(pageNo, x, y));
    }
    void SelectUpTo(int pageNo, int glyphIx);
    void SelectUpTo(int pageNo, double x, double y) {
        SelectUpTo(pageNo, FindClosestGlyph(pageNo, x, y));
    }
    void SelectWordAt(int pageNo, double x, double y);
    void CopySelection(TextSelection* orig);
    WCHAR* ExtractText(const WCHAR* lineSep);
    void Reset();

    TextSel result;

    void GetGlyphRange(int* fromPage, int* fromGlyph, int* toPage, int* toGlyph) const;

    int startPage, endPage;
    int startGlyph, endGlyph;

    EngineBase* engine;
    DocumentTextCache* textCache;

    int FindClosestGlyph(int pageNo, double x, double y);
    void FillResultRects(int pageNo, int glyph, int length, WStrVec* lines = nullptr);
};
