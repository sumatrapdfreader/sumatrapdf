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

class TextSearchTracker
{
public:
    virtual bool FindUpdateStatus(int count, int total) = 0;
};

class TextSearch : public TextSelection
{
public:
    TextSearch(BaseEngine *engine, TextSearchTracker *tracker=NULL);
    ~TextSearch();

    void SetText(TCHAR *text);
    void SetSensitive(bool sensitive);
    void SetDirection(bool forward);
    bool FindFirst(int page, TCHAR *text);
    bool FindNext();

    int findPage;
    TextSearchTracker *tracker;

protected:
    TCHAR *findText;
    TCHAR *anchor;
    bool forward;
    bool caseSensitive;
    // the 'Whole words' option is implicitly set when the search text
    // ends in a single space (many users already search that way)
    bool wholeWords;

    bool FindTextInPage(int pageNo = 0);
    bool FindStartingAtPage(int pageNo);
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
    
    // returns false, if the search has been canceled
    bool UpdateTracker(int pageNo, int total)
    {
        if (!tracker)
            return true;
        return tracker->FindUpdateStatus(pageNo, total);
    }

private:
    TCHAR *pageText;
    int findIndex;

    TCHAR *lastText;
    BYTE *findCache;
};

#endif
