#ifndef _PDF_SEARCH_H
#define _PDF_SEARCH_H

#include <windows.h>
#include "PdfEngine.h"

enum PdfSearchDirection {
    FIND_BACKWARD = false,
    FIND_FORWARD  = true
};

typedef struct {
    int page;
    int len;
    RECT *rects;
} PdfSearchResult;

class PdfSearchTracker
{
public:
    virtual bool FindUpdateStatus(int count, int total) = 0;
};

class PdfSearch
{
public:
    PdfSearch(PdfEngine *engine);
    ~PdfSearch();

    void Reset();
    void SetText(TCHAR *text);
    void SetSensitive(bool sensitive) { this->sensitive = sensitive; }
    void SetDirection(bool forward);
    bool FindFirst(int page, TCHAR *text);
    bool FindNext();

    PdfSearchResult result;
    PdfSearchTracker *tracker;

protected:
    TCHAR *text;
    TCHAR *anchor;
    bool  forward;
    bool  sensitive;

    void FillResultRects(TCHAR *found, int length);
    bool FindTextInPage(int pageNo = 0);
    bool FindStartingAtPage(int pageNo);
    int MatchLen(TCHAR *start);

    void Clear()
    {
        free(text);
        text = NULL;
        free(anchor);
        anchor = NULL;
        Reset();
    }
    
    // returns false, if the search has been canceled
    bool UpdateTracker(int pageNo, int total)
    {
        if (!tracker)
            return true;
        return tracker->FindUpdateStatus(pageNo, total);
    }

private:
    PdfEngine *engine;
    TCHAR *pageText;
    int findIndex;
    fz_bbox *coords;
};

#endif // _PDF_SEARCH_H
