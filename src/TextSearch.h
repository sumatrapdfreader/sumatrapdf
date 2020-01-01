/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

enum class TextSearchDirection : bool { Backward = false, Forward = true };

class TextSearch : public TextSelection {
  public:
    TextSearch(EngineBase* engine, PageTextCache* textCache);
    ~TextSearch();

    void SetSensitive(bool sensitive);
    void SetDirection(TextSearchDirection direction);
    void SetLastResult(TextSelection* sel);
    TextSel* FindFirst(int page, const WCHAR* text, ProgressUpdateUI* tracker = nullptr);
    TextSel* FindNext(ProgressUpdateUI* tracker = nullptr);

    // note: the result might not be a valid page number!
    int GetCurrentPageNo() const {
        return findPage;
    }

    // note: the result might not be a valid page number!
    int GetSearchHitStartPageNo() const {
        return searchHitStartAt;
    }

  protected:
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
    bool FindStartingAtPage(int pageNo, ProgressUpdateUI* tracker);
    PageAndOffset MatchEnd(const WCHAR* start) const;

    void Clear() {
        str::ReplacePtr(&findText, nullptr);
        str::ReplacePtr(&anchor, nullptr);
        str::ReplacePtr(&lastText, nullptr);
        Reset();
    }
    void Reset();

  private:
    const WCHAR* pageText = nullptr;
    int findIndex = 0;

    WCHAR* lastText = nullptr;
    int nPages = 0;
    std::vector<bool> pagesToSkip;
};
