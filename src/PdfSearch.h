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

class PdfSearchEngine
{
protected:
    void *text;
    int   length;
    bool  forward;
    bool sensitive;

public:
    PdfSearchResult result;
    PdfSearchTracker *tracker;

public:
    PdfSearchEngine()
    {
        tracker = NULL;
        text = NULL;
        sensitive = false;
        forward = true;
        result.page = 1;
    }

    virtual ~PdfSearchEngine()
    {
        Clear();
    }
    
    virtual void Reset() {}
    virtual void SetText(wchar_t *text);
    void SetSensitive(bool sensitive) { this->sensitive = sensitive; }
    virtual void SetDirection(bool forward) { this->forward = forward; }
    virtual bool FindFirst(int page, wchar_t *text) = 0;
    virtual bool FindNext() = 0;

protected:
    void Clear()
    {
        free(text);
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

class PdfSearchFitz : public PdfSearchEngine
{
private:
    PdfEngine *engine;
    pdf_textline *line;
    pdf_textline *current;
    long last;
public:
    PdfSearchFitz(PdfEngine *engine);
    virtual ~PdfSearchFitz();

    virtual void Reset();
    virtual void SetDirection(bool forward);
    virtual bool FindFirst(int page, wchar_t *text);
    virtual bool FindNext();

protected:
    void inline ReverseLineList();
    bool inline MatchChars(int c1, int c2);
    bool inline MatchAtPosition(int n);
    bool FindTextInPage(int page = 0);
    bool FindStartingAtPage(int page);
};

#endif // _PDF_SEARCH_H
