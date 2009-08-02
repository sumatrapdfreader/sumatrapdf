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
    int left;
    int top;
    int right;
    int bottom;
} PdfSearchResult;

class PdfSearchTracker
{
public:
    virtual void FindUpdateStatus(int count, int total) = 0;
};

class PdfSearch
{
protected:
    WCHAR *text;
    int   length;
    bool  forward;
    bool sensitive;

public:
    PdfSearchResult result;
    PdfSearchTracker *tracker;

private:
    PdfEngine *engine;
    pdf_textline *line;
    pdf_textline *current;
    long last;

protected:
    void ReverseLineList();
    bool MatchChars(WCHAR c1, WCHAR c2);
    bool MatchAtPosition(int n);
    bool FindTextInPage(int page = 0);
    bool FindStartingAtPage(int page);

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
    
    void UpdateTracker(int pageNo, int total)
    {
        if (!tracker)
            return;

        int count;
        if (forward)
            count = pageNo;
        else
            count = total - pageNo + 1;
        tracker->FindUpdateStatus(count, total);
    }
};

#endif // _PDF_SEARCH_H
