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
class   UiMsg;
class   MobiDoc;
class   ThreadLayoutMobi;

// data used on the ui thread side when handling UiMsg::MobiLayout
// It's separated out into its own struct for clarity
struct LayoutTemp {
    // if we're doing layout that starts from the beginning, this is NULL
    // otherwise it's the reparse point of the page we were showing when
    // we started the layout
    const char *    startPageReparsePoint;
    Vec<PageData *> pagesFromPage;
    Vec<PageData *> pagesFromBeginning;
};

class EbookController : public IClicked, ISizeChanged
{
    EbookControls * ctrls;

    MobiDoc *       mobiDoc;
    const char *    html;

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
    int             startPageForPagesFromPage;

    // either pagesFromBeginning or pagesFromPage
    Vec<PageData*>* pagesShowing;
    int             shownPageNo; // within pagesShowing

    // if pagesShowing == pagesFromBeginning its the same as shownPageNo
    // if pagesShowing == pagesFromPage it might be unknown during layout
    // (if we haven't gotten enough pages to determine it) or the page that contains
    // the beginning of the shown page
    int             currPageNoFromBeginning;

    int             pageDx, pageDy; // size of the page for which pages was generated

    ThreadLayoutMobi *layoutThread;
    LayoutTemp        layoutTmp;

    void UpdateStatus() const;
    void DeletePages(Vec<PageData*>** pages);
    void TriggerLayout();
    bool LayoutInProgress() const { return layoutThread != NULL; }

    // IClickHandler
    virtual void Clicked(Control *c, int x, int y);

    // ISizeChanged
    virtual void SizeChanged(Control *c, int dx, int dy);

public:
    EbookController(EbookControls *ctrls);
    virtual ~EbookController();

    void SetHtml(const char *html);
    void LoadMobi(const TCHAR *fileName);
    void HandleFinishedMobiLoadingMsg(UiMsg *msg);
    void HandleMobiLayoutMsg(UiMsg *msg);
    void OnLayoutTimer();
    void AdvancePage(int dist);
    void GoToPage(int newPageNo);
    void GoToLastPage();
};

#endif
