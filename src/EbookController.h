/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef EbookController_h
#define EbookController_h

#include "Doc.h"
#include "DisplayState.h"
#include "Sigslot.h"

struct  EbookControls;
class   HtmlPage;
class   EbookFormattingThread;
class   EbookFormattingTask;
class   HtmlFormatterArgs;
namespace mui { class Control; }
using namespace mui;

class EbookController : public sigslot::has_slots
{
    EbookControls * ctrls;

    Doc             doc;

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
    EbookController(EbookControls *ctrls, DisplayMode displayMode);
    virtual ~EbookController();

    void SetDoc(Doc newDoc, int startReparseIdxArg = -1);
    void HandlePagesFromEbookLayout(EbookFormattingTask *ebookLayout);
    void OnLayoutTimer();
    void AdvancePage(int dist);
    int  GetCurrentPageNo() const { return currPageNo; }
    size_t GetMaxPageCount();
    void GoToPage(int newPageNo);
    void GoToLastPage();
    const Doc&  GetDoc() const { return doc; }
    int  CurrPageReparseIdx() const { return currPageReparseIdx; }

    void SetSinglePage();
    void SetDoublePage();
    bool IsSinglePage() const;
    bool IsDoublePage() const { return !IsSinglePage(); }
};

class HtmlFormatter;

HtmlFormatterArgs *CreateFormatterArgsDoc(Doc doc, int dx, int dy, Allocator *textAllocator=NULL);
HtmlFormatter *CreateFormatter(Doc doc, HtmlFormatterArgs* args);

#define EBOOK_LAYOUT_TIMER_ID       6
#define EBOOK_LAYOUT_DELAY_IN_MS    200
#endif
