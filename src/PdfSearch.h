#ifndef _PDF_SEARCH_H
#define _PDF_SEARCH_H

#include <windows.h>
#include "PdfEngine.h"
#include "PdfSelection.h"

enum PdfSearchDirection {
    FIND_BACKWARD = false,
    FIND_FORWARD  = true
};

class PdfSearchTracker
{
public:
    virtual bool FindUpdateStatus(int count, int total) = 0;
};

class PdfSearch : public PdfSelection
{
public:
    PdfSearch(PdfEngine *engine);
    ~PdfSearch();

    void SetText(TCHAR *text);
    void SetSensitive(bool sensitive) { this->sensitive = sensitive; }
    void SetDirection(bool forward);
    bool FindFirst(int page, TCHAR *text);
    bool FindNext();

    int findPage;
    PdfSearchTracker *tracker;

protected:
    TCHAR *text;
    TCHAR *anchor;
    bool  forward;
    bool  sensitive;

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
};

#endif // _PDF_SEARCH_H
