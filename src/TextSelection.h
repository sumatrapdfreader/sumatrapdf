/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

inline unsigned int distSq(int x, int y) {
    return x * x + y * y;
}
// underscore is mainly used for programming and is thus considered a word character
inline bool isWordChar(WCHAR c) {
    return IsCharAlphaNumeric(c) || c == '_';
}

class PageTextCache {
    EngineBase* engine = nullptr;
    RectI** coords = nullptr;
    WCHAR** text = nullptr;
    int* lens = nullptr;
#ifdef DEBUG
    size_t debug_size;
#endif

    CRITICAL_SECTION access;

  public:
    explicit PageTextCache(EngineBase* engine);
    ~PageTextCache();

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

class TextSelection {
  public:
    TextSelection(EngineBase* engine, PageTextCache* textCache);
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

  protected:
    int startPage, endPage;
    int startGlyph, endGlyph;

    EngineBase* engine;
    PageTextCache* textCache;

    int FindClosestGlyph(int pageNo, double x, double y);
    void FillResultRects(int pageNo, int glyph, int length, WStrVec* lines = nullptr);
};
