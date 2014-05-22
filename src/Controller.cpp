/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "Controller.h"

#include "AppPrefs.h"
#include "ChmEngine.h"
#include "DisplayModel.h"
#include "Doc.h"
#include "EbookController.h"
#include "EbookControls.h"
#include "EbookFormatter.h"
#include "FileThumbnails.h"
using namespace Gdiplus;
#include "GdiPlusUtil.h"
#include "SumatraPDF.h"
#include "UITask.h"
#include "WindowInfo.h"

///// FixedPageUI /////

class FpController : public FixedPageUIController /*, public DisplayModelCallback */ {
    DisplayModel *dm;
    LinkHandler *linkHandler;

public:
    FpController(DisplayModel *dm, LinkHandler *linkHandler);
    virtual ~FpController();

    virtual const WCHAR *FilePath() const { return dm->engine->FileName(); }
    virtual const WCHAR *DefaultFileExt() const { return dm->engine->GetDefaultFileExt(); }
    virtual int PageCount() const { return dm->engine->PageCount(); }
    virtual WCHAR *GetProperty(DocumentProperty prop) { return dm->engine->GetProperty(prop); }

    virtual int CurrentPageNo() { return dm->CurrentPageNo(); }
    virtual void GoToPage(int pageNo, bool addNavPoint) { dm->GoToPage(pageNo, 0, addNavPoint); }
    virtual bool CanNavigate(int dir) { return dm->CanNavigate(dir); }
    virtual void Navigate(int dir) { dm->Navigate(dir); }

    virtual void SetDisplayMode(DisplayMode mode, bool keepContinuous=true);
    virtual DisplayMode GetDisplayMode() const { return dm->GetDisplayMode(); }
    virtual void SetPresentationMode(bool enable) { dm->SetPresentationMode(enable); }
    virtual void SetZoomVirtual(float zoom, PointI *fixPt=NULL) { dm->ZoomTo(zoom, fixPt); }
    virtual float GetZoomVirtual() const { return dm->ZoomVirtual(); }
    virtual float GetNextZoomStep(float towards) const { return dm->NextZoomStep(towards); }
    virtual void SetViewPortSize(SizeI size) { dm->ChangeViewPortSize(size); }

    virtual bool HasTocTree() const { return dm->engine->HasTocTree(); }
    virtual DocTocItem *GetTocTree() { return dm->engine->GetTocTree(); }
    virtual void GotoLink(PageDestination *dest) { linkHandler->GotoLink(dest); }
    virtual PageDestination *GetNamedDest(const WCHAR *name) { return dm->engine->GetNamedDest(name); }

    virtual void UpdateDisplayState(DisplayState *ds) { dm->DisplayStateFromModel(ds); }
    virtual void CreateThumbnail(DisplayState *ds);

    virtual bool HasPageLabels() const { return dm->engine->HasPageLabels(); }
    virtual WCHAR *GetPageLabel(int pageNo) const { return dm->engine->GetPageLabel(pageNo); }
    virtual int GetPageByLabel(const WCHAR *label) const { return dm->engine->GetPageByLabel(label); }

    virtual bool ValidPageNo(int pageNo) const { return 1 <= pageNo && pageNo <= PageCount(); }
    virtual bool GoToNextPage() { return dm->GoToNextPage(0); }
    virtual bool GoToPrevPage(bool toBottom) { return dm->GoToPrevPage(toBottom ? -1 : 0); }
    virtual bool GoToFirstPage() { return dm->GoToFirstPage(); }
    virtual bool GoToLastPage() { return dm->GoToLastPage(); }

    virtual FixedPageUIController *AsFixed() { return this; }

    // FixedPageUIController
    virtual DisplayModel *model() { return dm; }
    virtual BaseEngine *engine() { return dm->engine; }

    /*
    // DisplayModelCallback
    virtual void Repaint();
    virtual void UpdateScrollbars(SizeI canvas);
    virtual void RequestRendering(int pageNo);
    virtual void CleanUp(DisplayModel *dm);
    */
};

FpController::FpController(DisplayModel *dm, LinkHandler *linkHandler) :
    dm(dm), linkHandler(linkHandler)
{
    CrashIf(!dm || !linkHandler);
}

FpController::~FpController()
{
    delete dm;
}

void FpController::SetDisplayMode(DisplayMode mode, bool keepContinuous)
{
    if (keepContinuous && IsContinuous(dm->GetDisplayMode())) {
        switch (mode) {
        case DM_SINGLE_PAGE: mode = DM_CONTINUOUS; break;
        case DM_FACING: mode = DM_CONTINUOUS_FACING; break;
        case DM_BOOK_VIEW: mode = DM_CONTINUOUS_BOOK_VIEW; break;
        }
    }
    dm->ChangeDisplayMode(mode);
}

void FpController::CreateThumbnail(DisplayState *ds)
{
    // don't create thumbnails for password protected documents
    // (unless we're also remembering the decryption key anyway)
    if (dm->engine->IsPasswordProtected() &&
        !ScopedMem<char>(dm->engine->GetDecryptionKey())) {
        RemoveThumbnail(*ds);
        return;
    }

    RectD pageRect = dm->engine->PageMediabox(1);
    if (pageRect.IsEmpty())
        return;

    pageRect = dm->engine->Transform(pageRect, 1, 1.0f, 0);
    float zoom = THUMBNAIL_DX / (float)pageRect.dx;
    if (pageRect.dy > (float)THUMBNAIL_DY / zoom)
        pageRect.dy = (float)THUMBNAIL_DY / zoom;
    pageRect = dm->engine->Transform(pageRect, 1, 1.0f, 0, true);

    RenderThumbnail(dm, zoom, pageRect);
}

FixedPageUIController *FixedPageUIController::Create(DisplayModel *dm, LinkHandler *linkHandler)
{
    return new FpController(dm, linkHandler);
}

///// ChmUI /////

class ChmController : public ChmUIController, public ChmNavigationCallback {
    ChmEngine *_engine;
    WindowInfo *win;
    float initZoom;

public:
    ChmController(ChmEngine *engine, WindowInfo *win);
    virtual ~ChmController();

    virtual const WCHAR *FilePath() const { return _engine->FileName(); }
    virtual const WCHAR *DefaultFileExt() const { return _engine->GetDefaultFileExt(); }
    virtual int PageCount() const { return _engine->PageCount(); }
    virtual WCHAR *GetProperty(DocumentProperty prop) { return _engine->GetProperty(prop); }

    virtual int CurrentPageNo() { return _engine->CurrentPageNo(); }
    virtual void GoToPage(int pageNo, bool addNavPoint) { _engine->DisplayPage(pageNo); }
    virtual bool CanNavigate(int dir) { return _engine->CanNavigate(dir); }
    virtual void Navigate(int dir) { _engine->Navigate(dir); }

    virtual void SetDisplayMode(DisplayMode mode, bool keepContinuous=true) { /* not supported */ }
    virtual DisplayMode GetDisplayMode() const { return DM_SINGLE_PAGE; }
    virtual void SetPresentationMode(bool enable) { /* not supported */ }
    virtual void SetZoomVirtual(float zoom, PointI *fixPt=NULL);
    virtual float GetZoomVirtual() const { return _engine->GetZoom(); }
    virtual float GetNextZoomStep(float towards) const;
    virtual void SetViewPortSize(SizeI size) { /* not needed(?) */ }

    virtual bool HasTocTree() const { return _engine->HasTocTree(); }
    virtual DocTocItem *GetTocTree() { return _engine->GetTocTree(); }
    virtual void GotoLink(PageDestination *dest) { _engine->GoToDestination(dest); }
    virtual PageDestination *GetNamedDest(const WCHAR *name) { return _engine->GetNamedDest(name); }

    virtual void UpdateDisplayState(DisplayState *ds);
    virtual void CreateThumbnail(DisplayState *ds);

    virtual ChmUIController *AsChm() { return this; }

    // FixedPageUIController
    virtual ChmEngine *engine() { return _engine; }

    // ChmNavigationCallback
    virtual void PageNoChanged(int pageNo);
    virtual void LaunchBrowser(const WCHAR *url) { ::LaunchBrowser(url); }
    virtual void FocusFrame(bool always) { win->FocusFrame(always); }
    virtual void SaveDownload(const WCHAR *url, const unsigned char *data, size_t len);
};

ChmController::ChmController(ChmEngine *engine, WindowInfo *win) : _engine(engine), win(win), initZoom(INVALID_ZOOM)
{
    CrashIf(!_engine || _engine->PageCount() <= 0 || !win);
    _engine->SetNavigationCalback(this);
}

ChmController::~ChmController()
{
    delete _engine;
}

void ChmController::SetZoomVirtual(float zoom, PointI *fixPt)
{
    if (zoom <= 0 || !IsValidZoom(zoom))
        zoom = 100.0f;
    _engine->ZoomTo(zoom);
    initZoom = zoom;
}

// adapted from DisplayModel::NextZoomStep
float ChmController::GetNextZoomStep(float towardsLevel) const
{
    float currZoom = GetZoomVirtual();

    if (gGlobalPrefs->zoomIncrement > 0) {
        if (currZoom < towardsLevel)
            return min(currZoom * (gGlobalPrefs->zoomIncrement / 100 + 1), towardsLevel);
        if (currZoom > towardsLevel)
            return max(currZoom / (gGlobalPrefs->zoomIncrement / 100 + 1), towardsLevel);
        return currZoom;
    }

    Vec<float> *zoomLevels = gGlobalPrefs->zoomLevels;
    CrashIf(zoomLevels->Count() != 0 && (zoomLevels->At(0) < ZOOM_MIN || zoomLevels->Last() > ZOOM_MAX));
    CrashIf(zoomLevels->Count() != 0 && zoomLevels->At(0) > zoomLevels->Last());

    const float FUZZ = 0.01f;
    float newZoom = towardsLevel;
    if (currZoom < towardsLevel) {
        for (size_t i = 0; i < zoomLevels->Count(); i++) {
            if (zoomLevels->At(i) - FUZZ > currZoom) {
                newZoom = zoomLevels->At(i);
                break;
            }
        }
    }
    else if (currZoom > towardsLevel) {
        for (size_t i = zoomLevels->Count(); i > 0; i--) {
            if (zoomLevels->At(i - 1) + FUZZ < currZoom) {
                newZoom = zoomLevels->At(i - 1);
                break;
            }
        }
    }

    return newZoom;
}

void ChmController::UpdateDisplayState(DisplayState *ds)
{
    if (!ds->filePath || !str::EqI(ds->filePath, _engine->FileName()))
        str::ReplacePtr(&ds->filePath, _engine->FileName());

    ds->useDefaultState = !gGlobalPrefs->rememberStatePerDocument;

    str::ReplacePtr(&ds->displayMode, prefs::conv::FromDisplayMode(GetDisplayMode()));
    prefs::conv::FromZoom(&ds->zoom, GetZoomVirtual(), ds);

    ds->pageNo = CurrentPageNo();
    ds->scrollPos = PointI();
}

class ChmThumbnailTask : public UITask, public ChmNavigationCallback
{
    ChmEngine *engine;
    HWND hwnd;
    RenderedBitmap *bmp;

public:
    ChmThumbnailTask(ChmEngine *engine, HWND hwnd) :
        engine(engine), hwnd(hwnd), bmp(NULL) { }

    ~ChmThumbnailTask() {
        delete engine;
        DestroyWindow(hwnd);
        delete bmp;
    }

    virtual void Execute() {
        SaveThumbnailForFile(engine->FileName(), bmp);
        bmp = NULL;
    }

    virtual void PageNoChanged(int pageNo) {
        CrashIf(pageNo != 1);
        RectI area(0, 0, THUMBNAIL_DX * 2, THUMBNAIL_DY * 2);
        bmp = engine->TakeScreenshot(area, SizeI(THUMBNAIL_DX, THUMBNAIL_DY));
        uitask::Post(this);
    }

    virtual void LaunchBrowser(const WCHAR *url) { }
    virtual void FocusFrame(bool always) { }
    virtual void SaveDownload(const WCHAR *url, const unsigned char *data, size_t len) { }
};

void ChmController::CreateThumbnail(DisplayState *ds)
{
    // Create a thumbnail of chm document by loading it again and rendering
    // its first page to a hwnd specially created for it.
    ChmEngine *engine = static_cast<ChmEngine *>(_engine->Clone());
    if (!engine)
        return;

    // We render twice the size of thumbnail and scale it down
    int winDx = THUMBNAIL_DX * 2 + GetSystemMetrics(SM_CXVSCROLL);
    int winDy = THUMBNAIL_DY * 2 + GetSystemMetrics(SM_CYHSCROLL);
    // reusing WC_STATIC. I don't think exact class matters (WndProc
    // will be taken over by HtmlWindow anyway) but it can't be NULL.
    HWND hwnd = CreateWindow(WC_STATIC, L"BrowserCapture", WS_POPUP,
                             0, 0, winDx, winDy, NULL, NULL, NULL, NULL);
    if (!hwnd) {
        delete engine;
        return;
    }
    bool ok = engine->SetParentHwnd(hwnd);
    if (!ok) {
        DestroyWindow(hwnd);
        delete engine;
        return;
    }

#if 0 // when debugging set to 1 to see the window
    ShowWindow(hwnd, SW_SHOW);
#endif

    // engine and window will be destroyed by the callback once it's invoked
    ChmThumbnailTask *callback = new ChmThumbnailTask(engine, hwnd);
    engine->SetNavigationCalback(callback);
    engine->DisplayPage(1);
}

void ChmController::PageNoChanged(int pageNo)
{
    // TODO: setting zoom before the first page is loaded seems not to work
    // (might be a regression from between r4593 and r4629)
    if (IsValidZoom(initZoom)) {
        SetZoomVirtual(initZoom);
        initZoom = INVALID_ZOOM;
    }
    win->PageNoChanged(pageNo);
}

void ChmController::SaveDownload(const WCHAR *url, const unsigned char *data, size_t len)
{
    ScopedMem<WCHAR> plainUrl(str::ToPlainUrl(url));
    LinkSaver(*win, path::GetBaseName(plainUrl)).SaveEmbedded(data, len);
}

ChmUIController *ChmUIController::Create(ChmEngine *engine, WindowInfo *win)
{
    return new ChmController(engine, win);
}

///// EbookUI /////

class EbController : public EbookUIController {
    EbookController *_ctrl;
    EbookControls *_ctrls;

public:
    EbController(EbookControls *ctrls);
    virtual ~EbController();

    virtual const WCHAR *FilePath() const { return _ctrl->GetDoc().GetFilePath(); }
    virtual const WCHAR *DefaultFileExt() const { return path::GetExt(FilePath()); }
    virtual int PageCount() const { return (int)_ctrl->GetMaxPageCount(); }
    virtual WCHAR *GetProperty(DocumentProperty prop) { return doc()->GetProperty(prop); }

    virtual int CurrentPageNo() { return _ctrl->GetCurrentPageNo(); }
    virtual void GoToPage(int pageNo, bool addNavPoint) { _ctrl->GoToPage(pageNo); }
    virtual bool CanNavigate(int dir) { return false; }
    // TODO: this used to be equivalent to GoToNextPage/GoToPrevPage
    virtual void Navigate(int dir) { /* not supported */ }

    virtual void SetDisplayMode(DisplayMode mode, bool keepContinuous=true);
    virtual DisplayMode GetDisplayMode() const { return _ctrl->IsDoublePage() ? DM_FACING : DM_SINGLE_PAGE; }
    virtual void SetPresentationMode(bool enable) { /* not supported */ }
    virtual void SetZoomVirtual(float zoom, PointI *fixPt=NULL) { /* not supported */ }
    virtual float GetZoomVirtual() const { return 100; }
    virtual float GetNextZoomStep(float towards) const { return 100; }
    virtual void SetViewPortSize(SizeI size) { _ctrls->mainWnd->RequestLayout(); }

    virtual bool HasTocTree() const { return false; }
    virtual DocTocItem *GetTocTree() { return NULL; }
    virtual void GotoLink(PageDestination *dest) { CrashIf(true); }
    virtual PageDestination *GetNamedDest(const WCHAR *name) { return NULL; }

    virtual void UpdateDisplayState(DisplayState *ds);
    virtual void CreateThumbnail(DisplayState *ds);

    virtual bool GoToNextPage() { _ctrl->AdvancePage(1); return true; }
    virtual bool GoToPrevPage(bool toBottom) { _ctrl->AdvancePage(-1); return true; }

    virtual EbookUIController *AsEbook() { return this; }

    // EbookUIController
    virtual EbookController *ctrl() { return _ctrl; }
    virtual EbookControls *ctrls() { return _ctrls; }
    virtual Doc *doc() { return (Doc *)&_ctrl->GetDoc(); }

    virtual LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam, bool& wasHandled) {
        return _ctrls->mainWnd->evtMgr->OnMessage(msg, wParam, lParam, wasHandled);
    }
    virtual void SetController(EbookController *ctrl) { _ctrl = ctrl; }
    virtual void RequestRepaint() { _ctrls->mainWnd->MarkForRepaint(); }
    virtual void UpdateDocumentColors();
};

EbController::EbController(EbookControls *ctrls) :
    _ctrl(NULL), _ctrls(ctrls)
{
    CrashIf(_ctrl || !_ctrls);
}

EbController::~EbController()
{
    delete _ctrl;
    DestroyEbookControls(_ctrls);
}

void EbController::SetDisplayMode(DisplayMode mode, bool keepContinuous)
{
    if (!IsSingle(mode))
        _ctrl->SetDoublePage();
    else
        _ctrl->SetSinglePage();
}

void EbController::UpdateDisplayState(DisplayState *ds)
{
    if (!ds->filePath || !str::EqI(ds->filePath, FilePath()))
        str::ReplacePtr(&ds->filePath, FilePath());

    // don't modify any of the other DisplayState values
    // as long as they're not used, so that the same
    // DisplayState settings can also be used for EbookEngine;
    // we get reasonable defaults from DisplayState's constructor anyway
    ds->reparseIdx = _ctrl->CurrPageReparseIdx();
    str::ReplacePtr(&ds->displayMode, prefs::conv::FromDisplayMode(GetDisplayMode()));
}

// TODO: move somewhere more appropriate
static RenderedBitmap *RenderFirstDocPageToBitmap(Doc doc, SizeI pageSize, SizeI bmpSize, int border)
{
    PoolAllocator textAllocator;
    HtmlFormatterArgs *args = CreateFormatterArgsDoc2(doc, pageSize.dx - 2 * border, pageSize.dy - 2 * border, &textAllocator);
    TextRenderMethod renderMethod = args->textRenderMethod;
    HtmlFormatter *formatter = CreateFormatter(doc, args);
    HtmlPage *pd = formatter->Next();
    delete formatter;
    delete args;
    args = NULL;
    if (!pd)
        return NULL;

    Bitmap pageBmp(pageSize.dx, pageSize.dy, PixelFormat24bppRGB);
    Graphics g(&pageBmp);
    Rect r(0, 0, pageSize.dx, pageSize.dy);
    r.Inflate(1, 1);
    SolidBrush br(Color(255, 255, 255));
    g.FillRectangle(&br, r);

    ITextRender *textRender = CreateTextRender(renderMethod, &g);
    textRender->SetTextBgColor(Color(255,255,255));
    DrawHtmlPage(&g, textRender, &pd->instructions, (REAL)border, (REAL)border, false, Color((ARGB)Color::Black));
    delete pd;
    delete textRender;

    Bitmap res(bmpSize.dx, bmpSize.dy, PixelFormat24bppRGB);
    Graphics g2(&res);
    g2.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g2.DrawImage(&pageBmp, Rect(0, 0, bmpSize.dx, bmpSize.dy),
                 0, 0, pageSize.dx, pageSize.dy, UnitPixel);

    HBITMAP hbmp;
    Status ok = res.GetHBITMAP((ARGB)Color::White, &hbmp);
    if (ok != Ok)
        return NULL;
    return new RenderedBitmap(hbmp, bmpSize);
}

static RenderedBitmap *ThumbFromCoverPage(Doc doc)
{
    ImageData *coverImage = doc.GetCoverImage();
    if (!coverImage)
        return NULL;
    Bitmap *coverBmp = BitmapFromData(coverImage->data, coverImage->len);
    if (!coverBmp)
        return NULL;

    Bitmap res(THUMBNAIL_DX, THUMBNAIL_DY, PixelFormat24bppRGB);
    float scale = (float)THUMBNAIL_DX / (float)coverBmp->GetWidth();
    int fromDy = THUMBNAIL_DY;
    if (scale < 1.f)
        fromDy = (int)((float)coverBmp->GetHeight() * scale);
    Graphics g(&res);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    Status ok = g.DrawImage(coverBmp, Rect(0, 0, THUMBNAIL_DX, THUMBNAIL_DY),
                            0, 0, coverBmp->GetWidth(), fromDy, UnitPixel);
    if (ok != Ok) {
        delete coverBmp;
        return NULL;
    }
    HBITMAP hbmp;
    ok = res.GetHBITMAP((ARGB)Color::White, &hbmp);
    delete coverBmp;
    if (ok == Ok)
        return new RenderedBitmap(hbmp, SizeI(THUMBNAIL_DX, THUMBNAIL_DY));
    return NULL;
}

void EbController::CreateThumbnail(DisplayState *ds)
{
    CrashIf(!doc()->AsEpub() && !doc()->AsFb2() && !doc()->AsMobi());

    // if there is cover image, we use it to generate thumbnail by scaling
    // image width to thumbnail dx, scaling height proportionally and using
    // as much of it as fits in thumbnail dy
    RenderedBitmap *bmp = ThumbFromCoverPage(*doc());
    if (!bmp) {
        // no cover image so generate thumbnail from first page
        SizeI pageSize(THUMBNAIL_DX * 3, THUMBNAIL_DY * 3);
        SizeI dstSize(THUMBNAIL_DX, THUMBNAIL_DY);
        bmp = RenderFirstDocPageToBitmap(*doc(), pageSize, dstSize, 10);
    }
    SetThumbnail(ds, bmp);
}

// TODO: also needs to update for font name/size changes, but it's more complicated
// because requires re-layout
void EbController::UpdateDocumentColors()
{
    SetMainWndBgCol(_ctrls);
    // changing background will repaint mainWnd control but changing
    // of text color will not, so we request uncoditional repaint
    // TODO: in PageControl::Paint() use a property for text color, instead of
    // taking it directly from prefs
    ::RequestRepaint(_ctrls->mainWnd);
}

EbookUIController *EbookUIController::Create(HWND hwnd)
{
    EbookControls *ctrls = CreateEbookControls(hwnd);
    if (!ctrls) return NULL;
    return new EbController(ctrls);
}
