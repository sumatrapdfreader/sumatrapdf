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
protected:
    TCHAR *text;
    int   length;
    bool  forward;
    bool  sensitive;

public:
    PdfSearchResult result;
    PdfSearchTracker *tracker;

private:
    PdfEngine *engine;
    TCHAR *pageText;
    int findIndex;
    fz_irect *coords;

protected:
    void PdfSearch::FillResultRects(TCHAR *found);
    bool FindTextInPage(int pageNo = 0);
    bool FindStartingAtPage(int pageNo);

public:
    PdfSearch(PdfEngine *engine);
    ~PdfSearch();

    void Reset();
    void SetText(TCHAR *text);
    void SetSensitive(bool sensitive) { this->sensitive = sensitive; }
    void SetDirection(bool forward);
    bool FindFirst(int page, TCHAR *text);
    bool FindNext();

protected:
    void Clear()
    {
        free(text);
        text = NULL;
        Reset();
    }
    
    // returns false, if the search has been canceled
    bool UpdateTracker(int pageNo, int total)
    {
        if (!tracker)
            return true;

        int count;
        if (forward)
            count = pageNo;
        else
            count = total - pageNo + 1;
        return tracker->FindUpdateStatus(count, total);
    }
};

#endif // _PDF_SEARCH_H
