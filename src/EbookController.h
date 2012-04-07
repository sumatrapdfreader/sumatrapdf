/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef EbookController_h
#define EbookController_h

#include "BaseUtil.h"
#include "Doc.h"
#include "Mui.h"
#include "HtmlFormatter.h"
#include "SumatraWindow.h"
#include "ThreadUtil.h"

using namespace mui;

struct  EbookControls;
class   EbookController;
class   HtmlPage;
class   PoolAllocator;
class   EbookFormattingThread;

struct FinishedMobiLoadingData {
    TCHAR *         fileName;
    Doc *           doc;
    SumatraWindow   win;
    double          loadingTimeMs;

    void Free() {
        free(fileName);
        delete doc;
    }
};

struct EbookFormattingData {
    enum { MAX_PAGES = 32 };
    HtmlPage *         pages[MAX_PAGES];
    size_t             pageCount;
    bool               fromBeginning;
    bool               finished;
    EbookController *  controller;
    int                threadNo;
};

// data used on the ui thread side when handling UiMsg::MobiLayout
// it's in its own struct for clarity
struct FormattingTemp {
    // if we're doing layout that starts from the beginning, this is 0
    // otherwise it's the reparse point of the page we were showing when
    // we started the layout
    int             reparseIdx;
    Vec<HtmlPage *> pagesFromPage;
    Vec<HtmlPage *> pagesFromBeginning;

    void            DeletePages();
};

HtmlFormatterArgs *CreateFormatterArgs(const char *html, Doc doc, int dx, int dy, PoolAllocator *textAllocator);

class EbookController : public sigslot::has_slots
{
    EbookControls * ctrls;

    Doc             doc;
    const char *    html;

    // only set while we load the file on a background thread, used in UpdateStatus()
    TCHAR *         fileBeingLoaded;

    // TODO: this should be recycled along with pages so that its
    // memory use doesn't grow without bounds
    PoolAllocator   textAllocator;

    // we're in one of 3 states:
    // 1. showing pages as laid out from the beginning
    // 2. showing pages as laid out starting from another page
    //    (caused by resizing a window while displaying that page)
    // 3. like 2. but layout process is still in progress and we're waiting
    //    for more pages
    Vec<HtmlPage*>* pagesFromBeginning;
    Vec<HtmlPage*>* pagesFromPage;

    // currPageNo is in range 1..$numberOfPages. It's always a page number
    // as if the pages were formatted from the begginging. We don't always
    // know this (when we're showing a page from pagesFromPage and we
    // haven't yet formatted enough pages from beginning to determine which
    // of those pages contains top of the shown page), in which case it's 0
    size_t          currPageNo;

    // page that we're currently showing. It can come from pagesFromBeginning,
    // pagesFromPage or from formattingTemp during layout or it can be a page that
    // we took from previous pagesFromBeginning/pagesFromPage when we started
    // new layout process
    HtmlPage *      pageShown;
    // if true, we need to delete pageShown if we no longer need it
    bool            deletePageShown;

     // size of the page for which pages were generated
    int             pageDx, pageDy;

    EbookFormattingThread *formattingThread;
    int               formattingThreadNo;
    FormattingTemp        formattingTemp;

    // when loading a new mobiDoc, this indicates the page we should
    // show after loading. -1 indicates no action needed
    int               startReparseIdx;

    Vec<HtmlPage*> *GetPagesFromBeginning();
    HtmlPage*   PreserveTempPageShown();
    void        UpdateStatus();
    void        DeletePages(Vec<HtmlPage*>** pages);
    void        DeletePageShown();
    void        ShowPage(HtmlPage *pd, bool deleteWhenDone);
    void        UpdateCurrPageNoForPage(HtmlPage *pd);
    void        TriggerBookFormatting();
    bool        FormattingInProgress() const { return formattingThread != NULL; }
    bool        GoOnePageForward(Vec<HtmlPage*> *pages);
    void        GoOnePageForward();
    size_t      GetMaxPageCount();
    void        StopFormattingThread(bool forceTerminate);
    void        CloseCurrentDocument();

    // event handlers
    void        ClickedNext(Control *c, int x, int y);
    void        ClickedPrev(Control *c, int x, int y);
    void        ClickedProgress(Control *c, int x, int y);
    void        SizeChangedPage(Control *c, int dx, int dy);

public:
    EbookController(EbookControls *ctrls);
    virtual ~EbookController();

    void SetHtml(const char *html);
    void SetDoc(Doc newDoc, int startReparseIdxArg = -1);
    void HandleFinishedMobiLoadingMsg(FinishedMobiLoadingData *finishedMobiLoading);
    void HandleMobiLayoutMsg(EbookFormattingData *mobiLayout);
    void OnLayoutTimer();
    void AdvancePage(int dist);
    void GoToPage(int newPageNo);
    void GoToLastPage();
    Doc  GetDoc() const { return doc; }
    int  CurrPageReparseIdx() const;
};

#endif
