/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

enum TextSearchDirection {
    FIND_BACKWARD = false,
    FIND_FORWARD  = true
};

class ProgressUpdateUI
{
public:
    virtual void UpdateProgress(int current, int total) = 0;
    virtual bool WasCanceled() = 0;
    virtual ~ProgressUpdateUI() { }
};

class TextSearch : public TextSelection
{
public:
    TextSearch(BaseEngine *engine, PageTextCache *textCache);
    ~TextSearch();

    void SetSensitive(bool sensitive);
    void SetDirection(TextSearchDirection direction);
    void SetLastResult(TextSelection *sel);
    TextSel *FindFirst(int page, const WCHAR *text, ProgressUpdateUI *tracker=nullptr);
    TextSel *FindNext(ProgressUpdateUI *tracker=nullptr);

    // note: the result might not be a valid page number!
    int GetCurrentPageNo() const { return findPage; }

protected:
    WCHAR *findText;
    WCHAR *anchor;
    int findPage;
    bool forward;
    bool caseSensitive;
    // these two options are implicitly set when the search text begins
    // resp. ends in a single space (many users already search that way),
    // combining them yields a 'Whole words' search
    bool matchWordStart;
    bool matchWordEnd;

    void SetText(const WCHAR *text);
    bool FindTextInPage(int pageNo = 0);
    bool FindStartingAtPage(int pageNo, ProgressUpdateUI *tracker);
    int MatchLen(const WCHAR *start) const;

    void Clear()
    {
        str::ReplacePtr(&findText, nullptr);
        str::ReplacePtr(&anchor, nullptr);
        str::ReplacePtr(&lastText, nullptr);
        Reset();
    }
    void Reset();

private:
    const WCHAR *pageText;
    int findIndex;

    WCHAR *lastText;
    BYTE *findCache;
};
