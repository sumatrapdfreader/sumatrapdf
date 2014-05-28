/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef EbookController_h
#define EbookController_h

#include "Controller.h"
#include "Doc.h"
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

class EbookController : public Controller, public sigslot::has_slots
{
public:
    EbookController(EbookControls *ctrls, ControllerCallback *cb);
    virtual ~EbookController();

    virtual const WCHAR *FilePath() const { return _doc.GetFilePath(); }
    virtual const WCHAR *DefaultFileExt() const;
    virtual int PageCount() const { return GetMaxPageCount(); }
    virtual WCHAR *GetProperty(DocumentProperty prop) { return _doc.GetProperty(prop); }

    virtual int CurrentPageNo() const { return currPageNo; }
    virtual void GoToPage(int pageNo, bool addNavPoint);
    virtual bool CanNavigate(int dir) const { return false; }
    virtual void Navigate(int dir) { /* not supported */ }

    virtual void SetDisplayMode(DisplayMode mode, bool keepContinuous=false);
    virtual DisplayMode GetDisplayMode() const { return IsDoublePage() ? DM_FACING : DM_SINGLE_PAGE; }
    virtual void SetPresentationMode(bool enable) { /* not supported */ }
    virtual void SetZoomVirtual(float zoom, PointI *fixPt=NULL) { /* not supported */ }
    virtual float GetZoomVirtual() const { return 100; }
    virtual float GetNextZoomStep(float towards) const { return 100; }
    virtual void SetViewPortSize(SizeI size);

    virtual bool HasTocTree() const { return false; }
    virtual DocTocItem *GetTocTree() { return NULL; }
    virtual void ScrollToLink(PageDestination *dest) { CrashIf(true); }
    virtual PageDestination *GetNamedDest(const WCHAR *name) { return NULL; }

    virtual void UpdateDisplayState(DisplayState *ds);
    virtual void CreateThumbnail(SizeI size, ThumbnailCallback *tnCb);

    virtual bool GoToNextPage();
    virtual bool GoToPrevPage(bool toBottom=false);

    virtual EbookController *AsEbook() { return this; }

public:
    // the following is specific to EbookController

    const Doc *doc() const { return &_doc; }

    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam, bool& wasHandled);
    void EnableMessageHandling(bool enable) { handleMsgs = enable; }
    void UpdateDocumentColors();
    void RequestRepaint();
    void HandlePagesFromEbookLayout(EbookFormattingData *ebookLayout);
    void OnLayoutTimer();
    void SetDoc(Doc newDoc, int startReparseIdxArg=-1, DisplayMode displayMode=DM_AUTOMATIC);

    // call SetDoc before using this EbookController
    static EbookController *Create(HWND hwnd, ControllerCallback *cb);

protected:

    EbookControls * ctrls;

    Doc             _doc;

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

    // whether HandleMessage passes messages on to ctrls->mainWnd
    bool            handleMsgs;

    Vec<HtmlPage*> *GetPages();
    void        UpdateStatus();
    void        TriggerBookFormatting();
    bool        FormattingInProgress() const { return formattingThread != NULL; }
    void        StopFormattingThread();
    void        CloseCurrentDocument();
    int         GetMaxPageCount() const;
    bool        IsDoublePage() const;

    // event handlers
    void        ClickedNext(Control *c, int x, int y);
    void        ClickedPrev(Control *c, int x, int y);
    void        ClickedProgress(Control *c, int x, int y);
    void        SizeChangedPage(Control *c, int dx, int dy);
};

HtmlFormatterArgs *CreateFormatterArgsDoc(Doc doc, int dx, int dy, Allocator *textAllocator=NULL);
HtmlFormatter *CreateFormatter(Doc doc, HtmlFormatterArgs* args);

#endif
