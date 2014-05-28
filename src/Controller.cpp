/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "Controller.h"

#include "AppPrefs.h"
#include "Doc.h"
#include "EbookController.h"
#include "EbookControls.h"
#include "FileUtil.h"

///// EbookUI /////

// TODO: merge with EbookController
class EbController : public EbookUIController, public EbookControllerCallback {
    EbookController *ctrl;
    EbookControls *ctrls;
    bool handleMsgs;

public:
    EbController(EbookControls *ctrls, ControllerCallback *cb);
    virtual ~EbController();

    virtual const WCHAR *FilePath() const { return doc()->GetFilePath(); }
    virtual const WCHAR *DefaultFileExt() const { return path::GetExt(FilePath()); }
    virtual int PageCount() const { return (int)ctrl->GetMaxPageCount(); }
    virtual WCHAR *GetProperty(DocumentProperty prop) { return doc()->GetProperty(prop); }

    virtual int CurrentPageNo() const { return ctrl->GetCurrentPageNo(); }
    virtual void GoToPage(int pageNo, bool addNavPoint) { ctrl->GoToPage(pageNo); }
    virtual bool CanNavigate(int dir) const { return false; }
    // TODO: this used to be equivalent to GoToNextPage/GoToPrevPage
    virtual void Navigate(int dir) { /* not supported */ }

    virtual void SetDisplayMode(DisplayMode mode, bool keepContinuous=false);
    virtual DisplayMode GetDisplayMode() const { return ctrl->IsDoublePage() ? DM_FACING : DM_SINGLE_PAGE; }
    virtual void SetPresentationMode(bool enable) { /* not supported */ }
    virtual void SetZoomVirtual(float zoom, PointI *fixPt=NULL) { /* not supported */ }
    virtual float GetZoomVirtual() const { return 100; }
    virtual float GetNextZoomStep(float towards) const { return 100; }
    virtual void SetViewPortSize(SizeI size) { ctrls->mainWnd->RequestLayout(); }

    virtual bool HasTocTree() const { return false; }
    virtual DocTocItem *GetTocTree() { return NULL; }
    virtual void ScrollToLink(PageDestination *dest) { CrashIf(true); }
    virtual PageDestination *GetNamedDest(const WCHAR *name) { return NULL; }

    virtual void UpdateDisplayState(DisplayState *ds);
    virtual void CreateThumbnail(SizeI size, ThumbnailCallback *tnCb);

    virtual bool GoToNextPage() { ctrl->AdvancePage(1); return true; }
    virtual bool GoToPrevPage(bool toBottom) { ctrl->AdvancePage(-1); return true; }
    virtual bool GoToLastPage() { ctrl->GoToLastPage(); return true; }

    virtual EbookUIController *AsEbook() { return this; }

    // EbookUIController
    virtual const Doc *doc() const { return ctrl->doc(); }

    virtual LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam, bool& wasHandled) {
        return handleMsgs ? ctrls->mainWnd->evtMgr->OnMessage(msg, wParam, lParam, wasHandled) : 0;
    }
    virtual void EnableMessageHandling(bool enable) { handleMsgs = enable; }
    virtual void UpdateDocumentColors();
    virtual void RequestRepaint() { ctrls->mainWnd->MarkForRepaint(); }
    virtual void OnLayoutTimer() { ctrl->OnLayoutTimer(); }
    virtual EbookController *CreateController(DisplayMode displayMode);

    // EbookControllerCallback
    virtual void HandleLayoutedPages(EbookController *ctrl, EbookFormattingData *data) {
        CrashIf(ctrl != this->ctrl);
        cb->HandleLayoutedPages(ctrl, data);
    }
    virtual void RequestDelayedLayout(int delay) { cb->RequestDelayedLayout(delay); }
};

EbController::EbController(EbookControls *ctrls, ControllerCallback *cb) :
    EbookUIController(cb), ctrl(NULL), ctrls(ctrls), handleMsgs(true)
{
    CrashIf(ctrl || !ctrls);
}

EbController::~EbController()
{
    delete ctrl;
    DestroyEbookControls(ctrls);
}

void EbController::SetDisplayMode(DisplayMode mode, bool keepContinuous)
{
    if (!IsSingle(mode))
        ctrl->SetDoublePage();
    else
        ctrl->SetSinglePage();
}

void EbController::UpdateDisplayState(DisplayState *ds)
{
    if (!ds->filePath || !str::EqI(ds->filePath, FilePath()))
        str::ReplacePtr(&ds->filePath, FilePath());

    // don't modify any of the other DisplayState values
    // as long as they're not used, so that the same
    // DisplayState settings can also be used for EbookEngine;
    // we get reasonable defaults from DisplayState's constructor anyway
    ds->reparseIdx = ctrl->CurrPageReparseIdx();
    str::ReplacePtr(&ds->displayMode, prefs::conv::FromDisplayMode(GetDisplayMode()));
}

void EbController::CreateThumbnail(SizeI size, ThumbnailCallback *tnCb)
{
    // TODO: create thumbnail asynchronously
    RenderedBitmap *bmp = ctrl->CreateThumbnail(size);
    tnCb->SaveThumbnail(bmp);
}

// TODO: also needs to update for font name/size changes, but it's more complicated
// because requires re-layout
void EbController::UpdateDocumentColors()
{
    SetMainWndBgCol(ctrls);
    // changing background will repaint mainWnd control but changing
    // of text color will not, so we request uncoditional repaint
    // TODO: in PageControl::Paint() use a property for text color, instead of
    // taking it directly from prefs
    ::RequestRepaint(ctrls->mainWnd);
}

EbookController *EbController::CreateController(DisplayMode displayMode)
{
    CrashIf(ctrl);
    return (ctrl = new EbookController(ctrls, displayMode, this));
}

EbookUIController *EbookUIController::Create(HWND hwnd, ControllerCallback *cb)
{
    EbookControls *ctrls = CreateEbookControls(hwnd);
    if (!ctrls) return NULL;
    return new EbController(ctrls, cb);
}
