/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct TextSearch : public TextSelection {
    enum class Direction : bool { Backward = false, Forward = true };

    TextSearch(EngineBase* engine, DocumentTextCache* textCache);
    ~TextSearch();

    void SetSensitive(bool sensitive);
    void SetDirection(Direction direction);
    void SetLastResult(TextSelection* sel);
    TextSel* FindFirst(int page, const WCHAR* text);
    TextSel* FindNext();

    int GetCurrentPageNo() const;
    int GetSearchHitStartPageNo() const;

    ProgressUpdateCb progressCb;

    // Lightweight container for page and offset within the page to use as return value of MatchEnd
    struct PageAndOffset {
        int page;
        int offset;
    };

    WCHAR* findText = nullptr;
    WCHAR* anchor = nullptr;
    int findPage = 0;
    int searchHitStartAt = 0; // when text found spans several pages, searchHitStartAt < findPage
    bool forward = true;
    bool caseSensitive = false;
    // these two options are implicitly set when the search text begins
    // resp. ends in a single space (many users already search that way),
    // combining them yields a 'Whole words' search
    bool matchWordStart = false;
    bool matchWordEnd = false;

    void SetText(const WCHAR* text);
    bool FindTextInPage(int pageNo, PageAndOffset* finalGlyph);
    bool FindStartingAtPage(int pageNo);
    PageAndOffset MatchEnd(const WCHAR* start) const;

    void Clear();
    void Reset();

    const WCHAR* pageText = nullptr;
    int findIndex = 0;

    WCHAR* lastText = nullptr;
    int nPages = 0;
    Vec<bool> pagesToSkip;
};
