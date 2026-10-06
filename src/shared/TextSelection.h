/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct TextSel {
    int pageNo = 0;
    Rect rect;
    QuadF quad{};
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

    Vec<TextSel> result;

    void GetGlyphRange(int* fromPage, int* fromGlyph, int* toPage, int* toGlyph) const;
};

uint distSq(int x, int y);
bool isWordChar(int c);
bool TextPosMoveBy(EngineBase*, int& page, int& glyph, TextSelectUnit unit, int dir);
void FillSelectionRects(Vec<TextSel>* result, int pageNo, Rect* coords, int textLen, int glyph, int length,
                        Rect mediabox, QuadF* glyphQuads = nullptr);
int FindClosestGlyphIn(EngineBase* engine, int pageNo, Rect* coords, QuadF* quads, int textLen, double x, double y);
