/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// TODO: replace with Vec<TextSel>
struct TextSel {
    int len = 0;
    int cap = 0;
    int* pages = nullptr;
    Rect* rects = nullptr;
};

// Unit for keyboard/accessibility selection extension (platform-neutral).
// Callers map input (e.g. Shift+arrow keys) to unit + signed delta.
enum class TextSelectUnit {
    Glyph, // one glyph / character
    Word,  // to the previous / next word boundary
    Line,  // one visual line of text
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
    int FindClosestGlyphAt(int pageNo, double x, double y);
    void StartAt(int pageNo, int glyphIx);
    void StartAt(int pageNo, double x, double y);
    void SelectUpTo(int pageNo, int glyphIx);
    void SelectUpTo(int pageNo, double x, double y);
    void GetWordBoundsAt(int pageNo, double x, double y, int* wordStartOut, int* wordEndOut);
    void SelectWordAt(int pageNo, double x, double y);
    void SelectLineAt(int pageNo, double x, double y);
    void SelectWordsUpTo(int pageNo, double x, double y);
    bool ExtendBy(TextSelectUnit unit, int delta);
    void CopySelection(TextSelection* orig);
    Str ExtractText(Str lineSep);
    void Reset();

    TextSel result{};

    void GetGlyphRange(int* fromPage, int* fromGlyph, int* toPage, int* toGlyph) const;
};

uint distSq(int x, int y);
bool isWordChar(int c);
bool TextPosMoveBy(EngineBase*, int& page, int& glyph, TextSelectUnit unit, int dir);
