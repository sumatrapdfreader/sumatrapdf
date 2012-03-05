/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef EbookController_h
#define EbookController_h

#include "BaseUtil.h"
#include "Mui.h"
#include "ThreadUtil.h"
#include "Vec.h"

using namespace mui;

struct  EbookControls;
struct  PageData;
class   MobiDoc;
class   ThreadLayoutMobi;

struct FinishedMobiLoadingData {
    TCHAR *     fileName;
    MobiDoc *   mobiDoc;
    double      loadingTimeMs;

    void Free() {
        free(fileName);
    }
};

struct MobiLayoutData {
    enum { MAX_PAGES = 32 };
    PageData *         pages[MAX_PAGES];
    size_t             pageCount;
    bool               fromBeginning;
    bool               finished;
    ThreadLayoutMobi * thread;
};

// data used on the ui thread side when handling UiMsg::MobiLayout
// it's in its own struct for clarity
struct LayoutTemp {
    // if we're doing layout that starts from the beginning, this is NULL
    // otherwise it's the reparse point of the page we were showing when
    // we started the layout
    const char *    reparsePoint;
    Vec<PageData *> pagesFromPage;
    Vec<PageData *> pagesFromBeginning;

    void            DeletePages();
};

class EbookController : public IClicked, ISizeChanged
{
    EbookControls * ctrls;

    MobiDoc *       mobiDoc;
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
    Vec<PageData*>* pagesFromBeginning;
    Vec<PageData*>* pagesFromPage;

    // currPageNo is in range 1..$numberOfPages. It's always a page number
    // as if the pages were formatted from the begginging. We don't always
    // know this (when we're showing a page from pagesFromPage and we
    // haven't yet formatted enough pages from beginning to determine which
    // of those pages contains top of the shown page), in which case it's 0
    size_t          currPageNo;

    // page that we're currently showing. It can come from pagesFromBeginning,
    // pagesFromPage or from layoutTemp during layout or it can be a page that
    // we took from previous pagesFromBeginning/pagesFromPage when we started
    // new layout process
    PageData *      pageShown;
    // if true, we need to delete pageShown if we no longer need it
    bool            deletePageShown;

    int             pageDx, pageDy; // size of the page for which pages was generated

    ThreadLayoutMobi *layoutThread;
    LayoutTemp        layoutTemp;

    void UpdateStatus();
    void DeletePages(Vec<PageData*>** pages);
    void DeletePageShown();
    void ShowPage(PageData *pd, bool deleteWhenDone);
    PageData* PreserveTempPageShown();
    void UpdateCurrPageNoForPage(PageData *pd);
    Vec<PageData*> *GetPagesFromBeginning();
    void TriggerLayout();
    bool LayoutInProgress() const { return layoutThread != NULL; }
    bool GoOnePageForward(Vec<PageData*> *pages);
    void GoOnePageForward();
    size_t GetMaxPageCount();

    // IClickHandler
    virtual void Clicked(Control *c, int x, int y);

    // ISizeChanged
    virtual void SizeChanged(Control *c, int dx, int dy);

public:
    EbookController(EbookControls *ctrls);
    virtual ~EbookController();

    void SetHtml(const char *html);
    void LoadMobi(const TCHAR *fileName);
    void HandleFinishedMobiLoadingMsg(FinishedMobiLoadingData *finishedMobiLoading);
    void HandleMobiLayoutMsg(MobiLayoutData *mobiLayout);
    void OnLayoutTimer();
    void AdvancePage(int dist);
    void GoToPage(int newPageNo);
    void GoToLastPage();
};

#endif
