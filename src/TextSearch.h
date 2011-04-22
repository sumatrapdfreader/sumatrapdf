/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef TextSearch_h
#define TextSearch_h

#include <windows.h>
#include "BaseEngine.h"
#include "TextSelection.h"

enum TextSearchDirection {
    FIND_BACKWARD = false,
    FIND_FORWARD  = true
};

class ProgressUpdateUI
{
public:
    virtual bool ProgressUpdate(int current, int total) = 0;
};

class TextSearch : public TextSelection
{
public:
    TextSearch(BaseEngine *engine);
    ~TextSearch();

    void SetSensitive(bool sensitive);
    void SetDirection(TextSearchDirection direction);
    TextSel *FindFirst(int page, TCHAR *text, ProgressUpdateUI *tracker=NULL);
    TextSel *FindNext(ProgressUpdateUI *tracker=NULL);

    // note: the result might not be a valid page number!
    int GetCurrentPageNo() { return findPage; }

protected:
    TCHAR *findText;
    TCHAR *anchor;
    int findPage;
    bool forward;
    bool caseSensitive;
    // the 'Whole words' option is implicitly set when the search text
    // ends in a single space (many users already search that way)
    bool wholeWords;

    void SetText(TCHAR *text);
    bool FindTextInPage(int pageNo = 0);
    bool FindStartingAtPage(int pageNo, ProgressUpdateUI *tracker);
    int MatchLen(TCHAR *start);

    void Clear()
    {
        free(findText);
        findText = NULL;
        free(anchor);
        anchor = NULL;
        free(lastText);
        lastText = NULL;
        Reset();
    }
    void Reset();

private:
    TCHAR *pageText;
    int findIndex;

    TCHAR *lastText;
    BYTE *findCache;
};

#endif
