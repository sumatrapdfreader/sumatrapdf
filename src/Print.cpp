/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "Print.h"

#include "AppTools.h"
#include "Doc.h"
#include "FileUtil.h"
#include "Notifications.h"
#include "Selection.h"
#include "SumatraDialogs.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "UITask.h"
#include "WindowInfo.h"
#include "WinUtil.h"

struct PrintData {
    ScopedMem<WCHAR> driverName, printerName, portName;
    ScopedMem<DEVMODE> devMode;
    BaseEngine *engine;
    Vec<PRINTPAGERANGE> ranges; // empty when printing a selection
    Vec<SelectionOnPage> sel;   // empty when printing a page range
    Print_Advanced_Data advData;
    int rotation;

    PrintData(BaseEngine *engine, PRINTER_INFO_2 *printerInfo, DEVMODE *devMode,
              Vec<PRINTPAGERANGE>& ranges, Print_Advanced_Data& advData,
              int rotation=0, Vec<SelectionOnPage> *sel=NULL) :
        engine(NULL), advData(advData), rotation(rotation)
    {
        if (engine)
            this->engine = engine->Clone();

        if (printerInfo) {
            driverName.Set(str::Dup(printerInfo->pDriverName));
            printerName.Set(str::Dup(printerInfo->pPrinterName));
            portName.Set(str::Dup(printerInfo->pPortName));
        }
        if (devMode)
            this->devMode.Set((LPDEVMODE)memdup(devMode, devMode->dmSize + devMode->dmDriverExtra));

        if (!sel)
            this->ranges = ranges;
        else
            this->sel = *sel;
    }

    ~PrintData() {
        delete engine;
    }
};

class AbortCookieManager {
    CRITICAL_SECTION cookieAccess;
public:
    AbortCookie *cookie;

    AbortCookieManager() : cookie(NULL) {
        InitializeCriticalSection(&cookieAccess);
    }
    ~AbortCookieManager() {
        Clear();
        DeleteCriticalSection(&cookieAccess);
    }

    void Abort() {
        ScopedCritSec scope(&cookieAccess);
        if (cookie)
            cookie->Abort();
        Clear();
    }

    void Clear() {
        ScopedCritSec scope(&cookieAccess);
        if (cookie) {
            delete cookie;
            cookie = NULL;
        }
    }
};

class ScopeHDC {
    HDC hdc;
public:
    ScopeHDC(HDC hdc) : hdc(hdc) { }
    ~ScopeHDC() { DeleteDC(hdc); }
    operator HDC() const { return hdc; }
};

class ScopeDisableDEP {
public:
    ScopeDisableDEP() { EnableDataExecution(); }
    ~ScopeDisableDEP() { DisableDataExecution(); }
};

static RectD BoundSelectionOnPage(Vec<SelectionOnPage>& sel, int pageNo)
{
    RectD bounds;
    for (size_t i = 0; i < sel.Count(); i++) {
        if (sel.At(i).pageNo == pageNo)
            bounds = bounds.Union(sel.At(i).rect);
    }
    return bounds;
}

static bool PrintToDevice(PrintData& pd, ProgressUpdateUI *progressUI=NULL, AbortCookieManager *abortCookie=NULL)
{
    AssertCrash(pd.engine);
    if (!pd.engine)
        return false;
    AssertCrash(pd.printerName);
    if (!pd.printerName)
        return false;

    BaseEngine& engine = *pd.engine;
    ScopedMem<WCHAR> fileName;

    DOCINFO di = { 0 };
    di.cbSize = sizeof (DOCINFO);
    if (gPluginMode) {
        fileName.Set(ExtractFilenameFromURL(gPluginURL));
        // fall back to a generic "filename" instead of the more confusing temporary filename
        di.lpszDocName = fileName ? fileName : L"filename";
    }
    else
        di.lpszDocName = engine.FileName();

    int current = 1, total = 0;
    if (pd.sel.Count() == 0) {
        for (size_t i = 0; i < pd.ranges.Count(); i++) {
            if (pd.ranges.At(i).nToPage < pd.ranges.At(i).nFromPage)
                total += pd.ranges.At(i).nFromPage - pd.ranges.At(i).nToPage + 1;
            else
                total += pd.ranges.At(i).nToPage - pd.ranges.At(i).nFromPage + 1;
        }
    }
    else {
        for (int pageNo = 1; pageNo <= engine.PageCount(); pageNo++) {
            if (!BoundSelectionOnPage(pd.sel, pageNo).IsEmpty())
                total++;
        }
    }
    AssertCrash(total > 0);
    if (0 == total)
        return false;
    if (progressUI)
        progressUI->UpdateProgress(current, total);

    // cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1882
    // According to our crash dumps, Cannon printer drivers (caprenn.dll etc.)
    // are buggy and like to crash during printing with DEP violation
    // We disable dep during printing to hopefully not crash
    // TODO: even better would be to print in a separate process so that
    // crashes during printing wouldn't affect the main process. It's also
    // much more complicated to implement
    ScopeDisableDEP scopeNoDEP;

    // cf. http://blogs.msdn.com/b/oldnewthing/archive/2012/11/09/10367057.aspx
    ScopeHDC hdc(CreateDC(pd.driverName, pd.printerName, pd.portName, pd.devMode));
    if (!hdc)
        return false;

    if (StartDoc(hdc, &di) <= 0)
        return false;

    // MM_TEXT: Each logical unit is mapped to one device pixel.
    // Positive x is to the right; positive y is down.
    SetMapMode(hdc, MM_TEXT);

    const SizeI paperSize(GetDeviceCaps(hdc, PHYSICALWIDTH),
                          GetDeviceCaps(hdc, PHYSICALHEIGHT));
    const RectI printable(GetDeviceCaps(hdc, PHYSICALOFFSETX),
                          GetDeviceCaps(hdc, PHYSICALOFFSETY),
                          GetDeviceCaps(hdc, HORZRES), GetDeviceCaps(hdc, VERTRES));
    const float dpiFactor = min(GetDeviceCaps(hdc, LOGPIXELSX) / engine.GetFileDPI(),
                                GetDeviceCaps(hdc, LOGPIXELSY) / engine.GetFileDPI());
    bool bPrintPortrait = paperSize.dx < paperSize.dy;
    if (pd.devMode && (pd.devMode.Get()->dmFields & DM_ORIENTATION))
        bPrintPortrait = DMORIENT_PORTRAIT == pd.devMode.Get()->dmOrientation;

    if (pd.sel.Count() > 0) {
        for (int pageNo = 1; pageNo <= engine.PageCount(); pageNo++) {
            RectD bounds = BoundSelectionOnPage(pd.sel, pageNo);
            if (bounds.IsEmpty())
                continue;

            if (progressUI)
                progressUI->UpdateProgress(current, total);

            StartPage(hdc);

            SizeT<float> bSize = bounds.Size().Convert<float>();
            float zoom = min((float)printable.dx / bSize.dx,
                             (float)printable.dy / bSize.dy);
            // use the correct zoom values, if the page fits otherwise
            // and the user didn't ask for anything else (default setting)
            if (PrintScaleShrink == pd.advData.scale)
                zoom = min(dpiFactor, zoom);
            else if (PrintScaleNone == pd.advData.scale)
                zoom = dpiFactor;

            for (size_t i = 0; i < pd.sel.Count(); i++) {
                if (pd.sel.At(i).pageNo != pageNo)
                    continue;

                RectD *clipRegion = &pd.sel.At(i).rect;
                PointI offset((int)((clipRegion->x - bounds.x) * zoom), (int)((clipRegion->y - bounds.y) * zoom));
                if (!pd.advData.asImage) {
                    RectI rc((int)(printable.dx - bSize.dx * zoom) / 2 + offset.x,
                             (int)(printable.dy - bSize.dy * zoom) / 2 + offset.y,
                             (int)(clipRegion->dx * zoom), (int)(clipRegion->dy * zoom));
                    engine.RenderPage(hdc, rc, pd.sel.At(i).pageNo, zoom, pd.rotation, clipRegion, Target_Print, abortCookie ? &abortCookie->cookie : NULL);
                    if (abortCookie)
                        abortCookie->Clear();
                }
                else {
                    RenderedBitmap *bmp = NULL;
                    short shrink = 1;
                    do {
                        bmp = engine.RenderBitmap(pd.sel.At(i).pageNo, zoom / shrink, pd.rotation, clipRegion, Target_Print, abortCookie ? &abortCookie->cookie : NULL);
                        if (abortCookie)
                            abortCookie->Clear();
                        if (!bmp || !bmp->GetBitmap()) {
                            shrink *= 2;
                            delete bmp;
                            bmp = NULL;
                        }
                    } while (!bmp && shrink < 32 && !(progressUI && progressUI->WasCanceled()));
                    if (bmp) {
                        RectI rc((int)(paperSize.dx - bSize.dx * zoom) / 2 + offset.x,
                                 (int)(paperSize.dy - bSize.dy * zoom) / 2 + offset.y,
                                 bmp->Size().dx * shrink, bmp->Size().dy * shrink);
                        bmp->StretchDIBits(hdc, rc);
                        delete bmp;
                    }
                }
            }

            if (EndPage(hdc) <= 0 || progressUI && progressUI->WasCanceled()) {
                AbortDoc(hdc);
                return false;
            }
            current++;
        }

        EndDoc(hdc);
        return false;
    }

    // print all the pages the user requested
    for (size_t i = 0; i < pd.ranges.Count(); i++) {
        int dir = pd.ranges.At(i).nFromPage > pd.ranges.At(i).nToPage ? -1 : 1;
        for (DWORD pageNo = pd.ranges.At(i).nFromPage; pageNo != pd.ranges.At(i).nToPage + dir; pageNo += dir) {
            if ((PrintRangeEven == pd.advData.range && pageNo % 2 != 0) ||
                (PrintRangeOdd == pd.advData.range && pageNo % 2 == 0))
                continue;
            if (progressUI)
                progressUI->UpdateProgress(current, total);

            StartPage(hdc);

            SizeT<float> pSize = engine.PageMediabox(pageNo).Size().Convert<float>();
            int rotation = 0;
            // Turn the document by 90 deg if it isn't in portrait mode
            if (pSize.dx > pSize.dy) {
                rotation += 90;
                swap(pSize.dx, pSize.dy);
            }
            // make sure not to print upside-down
            rotation = (rotation % 180) == 0 ? 0 : 270;
            // finally turn the page by (another) 90 deg in landscape mode
            if (!bPrintPortrait) {
                rotation = (rotation + 90) % 360;
                swap(pSize.dx, pSize.dy);
            }

            // dpiFactor means no physical zoom
            float zoom = dpiFactor;
            // offset of the top-left corner of the page from the printable area
            // (negative values move the page into the left/top margins, etc.);
            // offset adjustments are needed because the GDI coordinate system
            // starts at the corner of the printable area and we rather want to
            // center the page on the physical paper (default behavior)
            PointI offset(-printable.x, -printable.y);

            if (pd.advData.scale != PrintScaleNone) {
                // make sure to fit all content into the printable area when scaling
                // and the whole document page on the physical paper
                RectD rect = engine.PageContentBox(pageNo, Target_Print);
                RectT<float> cbox = engine.Transform(rect, pageNo, 1.0, rotation).Convert<float>();
                zoom = min((float)printable.dx / cbox.dx,
                       min((float)printable.dy / cbox.dy,
                       min((float)paperSize.dx / pSize.dx,
                           (float)paperSize.dy / pSize.dy)));
                // use the correct zoom values, if the page fits otherwise
                // and the user didn't ask for anything else (default setting)
                if (PrintScaleShrink == pd.advData.scale && dpiFactor < zoom)
                    zoom = dpiFactor;
                // make sure that no content lies in the non-printable paper margins
                RectT<float> onPaper((paperSize.dx - pSize.dx * zoom) / 2 + cbox.x * zoom,
                                     (paperSize.dy - pSize.dy * zoom) / 2 + cbox.y * zoom,
                                     cbox.dx * zoom, cbox.dy * zoom);
                if (onPaper.x < printable.x)
                    offset.x += (int)(printable.x - onPaper.x);
                else if (onPaper.BR().x > printable.BR().x)
                    offset.x -= (int)(onPaper.BR().x - printable.BR().x);
                if (onPaper.y < printable.y)
                    offset.y += (int)(printable.y - onPaper.y);
                else if (onPaper.BR().y > printable.BR().y)
                    offset.y -= (int)(onPaper.BR().y - printable.BR().y);
            }

            if (!pd.advData.asImage) {
                RectI rc = RectI::FromXY((int)(paperSize.dx - pSize.dx * zoom) / 2 + offset.x,
                                         (int)(paperSize.dy - pSize.dy * zoom) / 2 + offset.y,
                                         paperSize.dx, paperSize.dy);
                engine.RenderPage(hdc, rc, pageNo, zoom, rotation, NULL, Target_Print, abortCookie ? &abortCookie->cookie : NULL);
                if (abortCookie)
                    abortCookie->Clear();
            }
            else {
                RenderedBitmap *bmp = NULL;
                short shrink = 1;
                do {
                    bmp = engine.RenderBitmap(pageNo, zoom / shrink, rotation, NULL, Target_Print, abortCookie ? &abortCookie->cookie : NULL);
                    if (abortCookie)
                        abortCookie->Clear();
                    if (!bmp || !bmp->GetBitmap()) {
                        shrink *= 2;
                        delete bmp;
                        bmp = NULL;
                    }
                } while (!bmp && shrink < 32 && !(progressUI && progressUI->WasCanceled()));
                if (bmp) {
                    RectI rc((paperSize.dx - bmp->Size().dx * shrink) / 2 + offset.x,
                             (paperSize.dy - bmp->Size().dy * shrink) / 2 + offset.y,
                             bmp->Size().dx * shrink, bmp->Size().dy * shrink);
                    bmp->StretchDIBits(hdc, rc);
                    delete bmp;
                }
            }

            if (EndPage(hdc) <= 0 || progressUI && progressUI->WasCanceled()) {
                AbortDoc(hdc);
                return false;
            }
            current++;
        }
    }

    EndDoc(hdc);
    return true;
}

class PrintThreadUpdateTask : public UITask {
    NotificationWnd *wnd;
    int current, total;
    WindowInfo *win;

public:
    PrintThreadUpdateTask(WindowInfo *win, NotificationWnd *wnd, int current, int total)
        : win(win), wnd(wnd), current(current), total(total) { }

    virtual void Execute() {
        if (WindowInfoStillValid(win) && win->notifications->Contains(wnd))
            wnd->UpdateProgress(current, total);
    }
};

class PrintThreadData : public ProgressUpdateUI, public NotificationWndCallback, public UITask {
    NotificationWnd *wnd;
    AbortCookieManager cookie;
    bool isCanceled;
    WindowInfo *win;

public:
    PrintData *data;
    HANDLE thread; // close the print thread handle after execution

    PrintThreadData(WindowInfo *win, PrintData *data) :
        win(win), data(data), isCanceled(false), thread(NULL) {
        wnd = new NotificationWnd(win->hwndCanvas, L"", _TR("Printing page %d of %d..."), this);
        win->notifications->Add(wnd);
    }

    ~PrintThreadData() {
        CloseHandle(thread);
        delete data;
        RemoveNotification(wnd);
    }

    virtual void UpdateProgress(int current, int total) {
        uitask::Post(new PrintThreadUpdateTask(win, wnd, current, total));
    }

    virtual bool WasCanceled() {
        return isCanceled || !WindowInfoStillValid(win) || win->printCanceled;
    }

    // called when printing has been canceled
    virtual void RemoveNotification(NotificationWnd *wnd) {
        isCanceled = true;
        cookie.Abort();
        this->wnd = NULL;
        if (WindowInfoStillValid(win))
            win->notifications->RemoveNotification(wnd);
    }

    static DWORD WINAPI PrintThread(LPVOID data)
    {
        PrintThreadData *threadData = (PrintThreadData *)data;
        // wait for PrintToDeviceOnThread to return so that we
        // close the correct handle to the current printing thread
        while (!threadData->win->printThread)
            Sleep(1);
        threadData->thread = threadData->win->printThread;
        PrintToDevice(*threadData->data, threadData, &threadData->cookie);
        uitask::Post(threadData);
        return 0;
    }

    virtual void Execute() {
        if (!WindowInfoStillValid(win))
            return;
        if (win->printThread != thread)
            return;
        win->printThread = NULL;
    }
};

static void PrintToDeviceOnThread(WindowInfo *win, PrintData *data)
{
    assert(!win->printThread);
    PrintThreadData *threadData = new PrintThreadData(win, data);
    win->printThread = NULL;
    win->printThread = CreateThread(NULL, 0, PrintThreadData::PrintThread, threadData, 0, NULL);
}

void AbortPrinting(WindowInfo *win)
{
    if (win->printThread) {
        win->printCanceled = true;
        WaitForSingleObject(win->printThread, INFINITE);
    }
    win->printCanceled = false;
}

static HGLOBAL GlobalMemDup(void *data, size_t len)
{
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, len);
    if (!hGlobal)
        return NULL;

    void *globalData = GlobalLock(hGlobal);
    if (!globalData) {
        GlobalFree(hGlobal);
        return NULL;
    }

    memcpy(globalData, data, len);
    GlobalUnlock(hGlobal);
    return hGlobal;
}

/* Show Print Dialog box to allow user to select the printer
and the pages to print.

For reference: In order to print with Adobe Reader instead: ViewWithAcrobat(win, L"/P");

Note: The following only applies for printing as image

Creates a new dummy page for each page with a large zoom factor,
and then uses StretchDIBits to copy this to the printer's dc.

So far have tested printing from XP to
 - Acrobat Professional 6 (note that acrobat is usually set to
   downgrade the resolution of its bitmaps to 150dpi)
 - HP Laserjet 2300d
 - HP Deskjet D4160
 - Lexmark Z515 inkjet, which should cover most bases.
*/
#define MAXPAGERANGES 10
void OnMenuPrint(WindowInfo *win, bool waitForCompletion)
{
    // we remember some printer settings per process
    static ScopedMem<DEVMODE> defaultDevMode;
    static PrintScaleAdv defaultScaleAdv = PrintScaleShrink;
    static bool defaultAsImage = false;

    bool printSelection = false;
    Vec<PRINTPAGERANGE> ranges;
    PRINTER_INFO_2 printerInfo = { 0 };

    if (!HasPermission(Perm_PrinterAccess)) return;

    DisplayModel *dm = win->dm;
    assert(dm);
    if (!dm) return;

    if (!dm->engine || !dm->engine->IsPrintingAllowed())
        return;

    if (win->IsChm()) {
        win->dm->AsChmEngine()->PrintCurrentPage();
        return;
    }

    if (win->printThread) {
        int res = MessageBox(win->hwndFrame, _TR("Printing is still in progress. Abort and start over?"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_YESNO | (IsUIRightToLeft() ? MB_RTLREADING : 0));
        if (res == IDNO)
            return;
    }
    AbortPrinting(win);

    PRINTDLGEX pd;
    ZeroMemory(&pd, sizeof(PRINTDLGEX));
    pd.lStructSize = sizeof(PRINTDLGEX);
    pd.hwndOwner   = win->hwndFrame;
    pd.Flags       = PD_USEDEVMODECOPIESANDCOLLATE | PD_COLLATE;
    if (!win->selectionOnPage)
        pd.Flags |= PD_NOSELECTION;
    pd.nCopies     = 1;
    /* by default print all pages */
    pd.nPageRanges = 1;
    pd.nMaxPageRanges = MAXPAGERANGES;
    PRINTPAGERANGE *ppr = AllocArray<PRINTPAGERANGE>(MAXPAGERANGES);
    pd.lpPageRanges = ppr;
    ppr->nFromPage = 1;
    ppr->nToPage = dm->PageCount();
    pd.nMinPage = 1;
    pd.nMaxPage = dm->PageCount();
    pd.nStartPage = START_PAGE_GENERAL;

    Print_Advanced_Data advanced(PrintRangeAll, defaultScaleAdv, defaultAsImage);
    ScopedMem<DLGTEMPLATE> dlgTemplate; // needed for RTL languages
    HPROPSHEETPAGE hPsp = CreatePrintAdvancedPropSheet(&advanced, dlgTemplate);
    pd.lphPropertyPages = &hPsp;
    pd.nPropertyPages = 1;

    // restore remembered settings
    if (defaultDevMode)
        pd.hDevMode = GlobalMemDup(defaultDevMode.Get(), defaultDevMode.Get()->dmSize + defaultDevMode.Get()->dmDriverExtra);

    if (PrintDlgEx(&pd) != S_OK) {
        if (CommDlgExtendedError() != 0) {
            /* if PrintDlg was cancelled then
               CommDlgExtendedError is zero, otherwise it returns the
               error code, which we could look at here if we wanted.
               for now just warn the user that printing has stopped
               becasue of an error */
            MessageBox(win->hwndFrame, _TR("Couldn't initialize printer"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK | (IsUIRightToLeft() ? MB_RTLREADING : 0));
        }
        goto Exit;
    }

    if (pd.dwResultAction == PD_RESULT_PRINT || pd.dwResultAction == PD_RESULT_APPLY) {
        // remember settings for this process
        LPDEVMODE devMode = (LPDEVMODE)GlobalLock(pd.hDevMode);
        if (devMode) {
            defaultDevMode.Set((LPDEVMODE)memdup(devMode, devMode->dmSize + devMode->dmDriverExtra));
            GlobalUnlock(pd.hDevMode);
        }
        defaultScaleAdv = advanced.scale;
        defaultAsImage = advanced.asImage;
    }

    if (pd.dwResultAction != PD_RESULT_PRINT)
        goto Exit;

    if (pd.Flags & PD_CURRENTPAGE) {
        PRINTPAGERANGE pr = { dm->CurrentPageNo(), dm->CurrentPageNo() };
        ranges.Append(pr);
    } else if (win->selectionOnPage && (pd.Flags & PD_SELECTION)) {
        printSelection = true;
    } else if (!(pd.Flags & PD_PAGENUMS)) {
        PRINTPAGERANGE pr = { 1, dm->PageCount() };
        ranges.Append(pr);
    } else {
        assert(pd.nPageRanges > 0);
        for (DWORD i = 0; i < pd.nPageRanges; i++)
            ranges.Append(pd.lpPageRanges[i]);
    }

    LPDEVNAMES devNames = (LPDEVNAMES)GlobalLock(pd.hDevNames);
    LPDEVMODE devMode = (LPDEVMODE)GlobalLock(pd.hDevMode);
    if (devNames) {
        printerInfo.pDriverName = (LPWSTR)devNames + devNames->wDriverOffset;
        printerInfo.pPrinterName = (LPWSTR)devNames + devNames->wDeviceOffset;
        printerInfo.pPortName = (LPWSTR)devNames + devNames->wOutputOffset;
    }
    PrintData *data = new PrintData(dm->engine, &printerInfo, devMode, ranges, advanced,
                                    dm->Rotation(), printSelection ? win->selectionOnPage : NULL);
    if (devNames)
        GlobalUnlock(pd.hDevNames);
    if (devMode)
        GlobalUnlock(pd.hDevMode);

    if (!waitForCompletion)
        PrintToDeviceOnThread(win, data);
    else {
        PrintToDevice(*data);
        delete data;
    }

Exit:
    free(ppr);
    GlobalFree(pd.hDevNames);
    GlobalFree(pd.hDevMode);
}

static void ApplyPrintSettings(const WCHAR *settings, int pageCount, Vec<PRINTPAGERANGE>& ranges, Print_Advanced_Data& advanced)
{
    WStrVec rangeList;
    if (settings)
        rangeList.Split(settings, L",", true);

    for (size_t i = 0; i < rangeList.Count(); i++) {
        PRINTPAGERANGE pr;
        if (str::Parse(rangeList.At(i), L"%d-%d%$", &pr.nFromPage, &pr.nToPage)) {
            pr.nFromPage = limitValue(pr.nFromPage, (DWORD)1, (DWORD)pageCount);
            pr.nToPage = limitValue(pr.nToPage, (DWORD)1, (DWORD)pageCount);
            ranges.Append(pr);
        }
        else if (str::Parse(rangeList.At(i), L"%d%$", &pr.nFromPage)) {
            pr.nFromPage = pr.nToPage = limitValue(pr.nFromPage, (DWORD)1, (DWORD)pageCount);
            ranges.Append(pr);
        }
        else if (str::Eq(rangeList.At(i), L"even"))
            advanced.range = PrintRangeEven;
        else if (str::Eq(rangeList.At(i), L"odd"))
            advanced.range = PrintRangeOdd;
        else if (str::Eq(rangeList.At(i), L"noscale"))
            advanced.scale = PrintScaleNone;
        else if (str::Eq(rangeList.At(i), L"shrink"))
            advanced.scale = PrintScaleShrink;
        else if (str::Eq(rangeList.At(i), L"fit"))
            advanced.scale = PrintScaleFit;
        else if (str::Eq(rangeList.At(i), L"compat"))
            advanced.asImage = true;
    }

    if (ranges.Count() == 0) {
        PRINTPAGERANGE pr = { 1, pageCount };
        ranges.Append(pr);
    }
}

bool PrintFile(const WCHAR *fileName, const WCHAR *printerName, bool displayErrors, const WCHAR *settings)
{
    if (!HasPermission(Perm_PrinterAccess))
        return false;

    ScopedMem<WCHAR> fileName2(path::Normalize(fileName));
    BaseEngine *engine = EngineManager::CreateEngine(!gUseEbookUI, fileName2);
    if (!engine || !engine->IsPrintingAllowed()) {
        if (displayErrors)
            MessageBox(NULL, _TR("Cannot print this file"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK | (IsUIRightToLeft() ? MB_RTLREADING : 0));
        return false;
    }

    HANDLE printer;
    bool ok = OpenPrinter((WCHAR *)printerName, &printer, NULL);
    if (!ok) {
        if (displayErrors)
            MessageBox(NULL, _TR("Printer with given name doesn't exist"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK | (IsUIRightToLeft() ? MB_RTLREADING : 0));
        return false;
    }

    // get printer driver information
    DWORD needed = 0;
    GetPrinter(printer, 2, NULL, 0, &needed);
    ScopedMem<PRINTER_INFO_2> infoData((PRINTER_INFO_2 *)AllocArray<BYTE>(needed));
    if (infoData)
        ok = GetPrinter(printer, 2, (LPBYTE)infoData.Get(), needed, &needed);
    if (!ok || !infoData || needed <= sizeof(PRINTER_INFO_2)) goto Exit;

    LONG structSize = DocumentProperties(NULL,
        printer,                /* Handle to our printer. */
        (WCHAR *)printerName,   /* Name of the printer. */ 
        NULL,                   /* Asking for size, so */
        NULL,                   /* these are not used. */
        0);                     /* Zero returns buffer size. */
    if (structSize < sizeof(DEVMODE)) {
        // If failure, inform the user, cleanup and return failure.
        if (displayErrors)
            MessageBox(NULL, _TR("Could not obtain Printer properties"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK | (IsUIRightToLeft() ? MB_RTLREADING : 0));
        goto Exit;
    }
    LPDEVMODE devMode = (LPDEVMODE)malloc(structSize);
    if (!devMode) goto Exit;

    // Get the default DevMode for the printer and modify it for your needs.
    LONG returnCode = DocumentProperties(NULL,
        printer,
        (WCHAR *)printerName,
        devMode,        /* The address of the buffer to fill. */
        NULL,           /* Not using the input buffer. */
        DM_OUT_BUFFER); /* Have the output buffer filled. */
    if (IDOK != returnCode) {
        // If failure, inform the user, cleanup and return failure.
        if (displayErrors)
            MessageBox(NULL, _TR("Could not obtain Printer properties"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK | (IsUIRightToLeft() ? MB_RTLREADING : 0));
        goto Exit;
    }

    ClosePrinter(printer);
    printer = NULL;

    {
        Print_Advanced_Data advanced;
        Vec<PRINTPAGERANGE> ranges;
        ApplyPrintSettings(settings, engine->PageCount(), ranges, advanced);

        PrintData pd(engine, infoData, devMode, ranges, advanced);
        ok = PrintToDevice(pd);
        if (!ok && displayErrors)
            MessageBox(NULL, _TR("Couldn't initialize printer"), _TR("Printing problem."),
                MB_ICONEXCLAMATION | MB_OK | (IsUIRightToLeft() ? MB_RTLREADING : 0));
    }

Exit:
    free(devMode);
    if (printer)
        ClosePrinter(printer);
    delete engine;
    return ok;
}
