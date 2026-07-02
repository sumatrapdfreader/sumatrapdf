/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct TextSearch : public TextSelection {
    enum class Direction : bool {
        Backward = false,
        Forward = true
    };

    explicit TextSearch(EngineBase* engine);
    ~TextSearch();

    void SetMatchCase(bool sensitive);
    void SetMatchWholeWord(bool wholeWord);
    void SetDirection(Direction direction);
    void SetLastResult(TextSelection* sel);
    TextSel* FindFirst(int page, WStr text);
    // like FindFirst but searches only the given page (issue #3085)
    TextSel* FindFirstOnPage(int pageNo, WStr text);
    TextSel* FindNext();

    int GetCurrentPageNo() const;
    int GetSearchHitStartPageNo() const;

    ProgressUpdateCb progressCb;

    // Lightweight container for page and offset within the page to use as return value of MatchEnd
    struct PageAndOffset {
        int page;
        int offset;
    };

    Str findText;
    Str anchor;
    int findTextLen = 0;
    int anchorLen = 0;
    int findPage = 0;
    int searchHitStartAt = 0; // when text found spans several pages, searchHitStartAt < findPage
    bool forward = true;
    bool matchCase = false;
    // when set, the search only matches complete words: it forces both
    // matchWordStart and matchWordEnd on regardless of leading/trailing spaces
    // (issue #4295)
    bool matchWholeWord = false;
    // these two options are implicitly set when the search text begins
    // resp. ends in a single space (many users already search that way),
    // combining them yields a 'Whole words' search
    bool matchWordStart = false;
    bool matchWordEnd = false;

    void SetText(WStr text);
    void SetText(Str text);
    bool FindTextInPage(int pageNo, PageAndOffset* finalGlyph);
    bool FindStartingAtPage(int pageNo);
    PageAndOffset MatchEnd(int startOff) const;

    void Clear();
    void Reset();

    Str pageText;
    int pageTextLen = 0;
    int findIndex = 0;

    Str lastText;
    int nPages = 0;
    Vec<bool> pagesToSkip;
};
