/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef EbookController_h
#define EbookController_h

#include "Doc.h"
#include "DisplayState.h"
#include "Sigslot.h"

class   EbookController;
struct  EbookControls;
class   HtmlPage;
class   EbookFormattingThread;
struct  EbookFormattingData;
class   HtmlFormatterArgs;
class   HtmlFormatter;
namespace mui { class Control; }
using namespace mui;

class EbookControllerCallback {
public:
    virtual ~EbookControllerCallback() { }
    // handle data from the formatting thread (pass it to HandlePagesFromEbookLayout)
    // TODO: do this internally without need for FindWindowInfoByController
    virtual void HandleLayoutedPages(EbookController *ctrl, EbookFormattingData *data) = 0;
    // call OnLayoutTimer in at least delay ms (replaces all previous requests)
    virtual void RequestDelayedLayout(int delay) = 0;
};

class EbookController : public sigslot::has_slots
{
    EbookControls * ctrls;

    Doc             _doc;

    // callback for interacting with UI code
    EbookControllerCallback *cb;

    // TODO: this should be recycled along with pages so that its
    // memory use doesn't grow without bounds
    PoolAllocator   textAllocator;

    Vec<HtmlPage*> *    pages;

    // pages being sent from background formatting thread
    Vec<HtmlPage*> *    incomingPages;

    // currPageNo is in range 1..$numberOfPages. 
    int             currPageNo;
    // reparseIdx of the current page (the first one if we're showing 2)
    int             currPageReparseIdx;

    // size of the page for which pages were generated
    SizeI           pageSize;

    EbookFormattingThread * formattingThread;
    int                     formattingThreadNo;

    Vec<HtmlPage*> *GetPages();
    void        UpdateStatus();
    void        TriggerBookFormatting();
    bool        FormattingInProgress() const { return formattingThread != NULL; }
    void        StopFormattingThread();
    void        CloseCurrentDocument();

    // event handlers
    void        ClickedNext(Control *c, int x, int y);
    void        ClickedPrev(Control *c, int x, int y);
    void        ClickedProgress(Control *c, int x, int y);
    void        SizeChangedPage(Control *c, int dx, int dy);

public:
    EbookController(EbookControls *ctrls, DisplayMode displayMode, EbookControllerCallback *cb);
    virtual ~EbookController();

    void SetDoc(Doc newDoc, int startReparseIdxArg = -1);
    void HandlePagesFromEbookLayout(EbookFormattingData *ebookLayout);
    void OnLayoutTimer();
    void AdvancePage(int dist);
    int  GetCurrentPageNo() const { return currPageNo; }
    size_t GetMaxPageCount();
    void GoToPage(int newPageNo);
    void GoToLastPage();
    const Doc *doc() const { return &_doc; }
    int  CurrPageReparseIdx() const { return currPageReparseIdx; }

    void SetSinglePage();
    void SetDoublePage();
    bool IsSinglePage() const;
    bool IsDoublePage() const { return !IsSinglePage(); }

    RenderedBitmap *CreateThumbnail(SizeI size);
};

HtmlFormatterArgs *CreateFormatterArgsDoc(Doc doc, int dx, int dy, Allocator *textAllocator=NULL);
HtmlFormatter *CreateFormatter(Doc doc, HtmlFormatterArgs* args);

#endif
