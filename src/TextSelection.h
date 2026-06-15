/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// TODO: replace with Vec<TextSel>
struct TextSel {
    int len = 0;
    int cap = 0;
    int* pages = nullptr;
    Rect* rects = nullptr;
};

struct TextSelection {
    int startPage = -1;
    int endPage = -1;
    int startGlyph = -1;
    int endGlyph = -1;

    // the word selected by the most recent SelectWordAt(); used as the anchor
    // for word-granular extension via SelectWordsUpTo()
    int wordStartPage = -1;
    int wordStartGlyph = -1;
    int wordEndPage = -1;
    int wordEndGlyph = -1;

    EngineBase* engine = nullptr;

    explicit TextSelection(EngineBase* engine);
    ~TextSelection();

    bool IsOverGlyph(int pageNo, double x, double y);
    void StartAt(int pageNo, int glyphIx);
    void StartAt(int pageNo, double x, double y);
    void SelectUpTo(int pageNo, int glyphIx);
    void SelectUpTo(int pageNo, double x, double y);
    void GetWordBoundsAt(int pageNo, double x, double y, int* wordStartOut, int* wordEndOut);
    void SelectWordAt(int pageNo, double x, double y);
    // extend the selection so it spans whole words from the anchor word (set by
    // the last SelectWordAt) to the word at (x, y)
    void SelectWordsUpTo(int pageNo, double x, double y);
    void CopySelection(TextSelection* orig);
    WCHAR* ExtractText(const char* lineSep);
    void Reset();

    TextSel result{};

    void GetGlyphRange(int* fromPage, int* fromGlyph, int* toPage, int* toGlyph) const;
};

uint distSq(int x, int y);
bool isWordChar(WCHAR c);
