/* Copyright 2006-2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "Print.h"

#include "AppTools.h"
#include "EngineManager.h"
#include "FileUtil.h"
#include "Notifications.h"
#include "Selection.h"
#include "SumatraDialogs.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "WindowInfo.h"
#include "WinUtil.h"

static bool PrinterSupportsStretchDib(HWND hwndForMsgBox, HDC hdc)
{
#ifdef USE_GDI_FOR_PRINTING
    // assume the printer supports enough of GDI(+) for reasonable results
    return true;
#else
    // most printers can support stretchdibits,
    // whereas a lot of printers do not support bitblt
    // quit if printer doesn't support StretchDIBits
    int rasterCaps = GetDeviceCaps(hdc, RASTERCAPS);
    int supportsStretchDib = rasterCaps & RC_STRETCHDIB;
    if (supportsStretchDib)
        return true;

    MessageBox(hwndForMsgBox, _T("This printer doesn't support the StretchDIBits function"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK);
    return false;
#endif
}

struct PrintData {
    HDC hdc; // owned by PrintData

    BaseEngine *engine;
    Vec<PRINTPAGERANGE> ranges; // empty when printing a selection
    Vec<SelectionOnPage> sel;   // empty when printing a page range
    int rotation;
    PrintRangeAdv rangeAdv;
    PrintScaleAdv scaleAdv;
    short orientation;

    PrintData(BaseEngine *engine, HDC hdc, DEVMODE *devMode,
              Vec<PRINTPAGERANGE>& ranges, int rotation=0,
              PrintRangeAdv rangeAdv=PrintRangeAll,
              PrintScaleAdv scaleAdv=PrintScaleShrink,
              Vec<SelectionOnPage> *sel=NULL) :
        engine(NULL), hdc(hdc), rotation(rotation), rangeAdv(rangeAdv), scaleAdv(scaleAdv)
    {
        if (engine)
            this->engine = engine->Clone();

        if (!sel)
            this->ranges = ranges;
        else
            this->sel = *sel;

        orientation = 0;
        if (devMode && (devMode->dmFields & DM_ORIENTATION))
            orientation = devMode->dmOrientation;
    }

    ~PrintData() {
        delete engine;
        DeleteDC(hdc);
    }
};

static void PrintToDevice(PrintData& pd, ProgressUpdateUI *progressUI=NULL)
{
    assert(pd.engine);
    if (!pd.engine) return;

    HDC hdc = pd.hdc;
    BaseEngine& engine = *pd.engine;
    ScopedMem<TCHAR> fileName;

    DOCINFO di = { 0 };
    di.cbSize = sizeof (DOCINFO);
    if (gPluginMode) {
        fileName.Set(ExtractFilenameFromURL(gPluginURL));
        // fall back to a generic "filename" instead of the more confusing temporary filename
        di.lpszDocName = fileName ? fileName : _T("filename");
    }
    else
        di.lpszDocName = engine.FileName();

    int current = 0, total = 0;
    for (size_t i = 0; i < pd.ranges.Count(); i++) {
        if (pd.ranges.At(i).nToPage < pd.ranges.At(i).nFromPage)
            total += pd.ranges.At(i).nFromPage - pd.ranges.At(i).nToPage + 1;
        else
            total += pd.ranges.At(i).nToPage - pd.ranges.At(i).nFromPage + 1;
    }
    total += (int)pd.sel.Count();
    if (progressUI)
        progressUI->UpdateProgress(current, total);

    if (StartDoc(hdc, &di) <= 0)
        return;

    SetMapMode(hdc, MM_TEXT);

    const int paperWidth = GetDeviceCaps(hdc, PHYSICALWIDTH);
    const int paperHeight = GetDeviceCaps(hdc, PHYSICALHEIGHT);
    const int printableWidth = GetDeviceCaps(hdc, HORZRES);
    const int printableHeight = GetDeviceCaps(hdc, VERTRES);
    const int leftMargin = GetDeviceCaps(hdc, PHYSICALOFFSETX);
    const int topMargin = GetDeviceCaps(hdc, PHYSICALOFFSETY);
    const int rightMargin = paperWidth - printableWidth - leftMargin;
    const int bottomMargin = paperHeight - printableHeight - topMargin;
    const float dpiFactor = min(GetDeviceCaps(hdc, LOGPIXELSX) / engine.GetFileDPI(),
                                GetDeviceCaps(hdc, LOGPIXELSY) / engine.GetFileDPI());
    bool bPrintPortrait = paperWidth < paperHeight;
    if (pd.orientation)
        bPrintPortrait = DMORIENT_PORTRAIT == pd.orientation;

    if (pd.sel.Count() > 0) {
        for (size_t i = 0; i < pd.sel.Count(); i++) {
            StartPage(hdc);
            RectD *clipRegion = &pd.sel.At(i).rect;

            SizeT<float> sSize = clipRegion->Size().Convert<float>();
            float zoom = min((float)printableWidth / sSize.dx,
                             (float)printableHeight / sSize.dy);
            // use the correct zoom values, if the page fits otherwise
            // and the user didn't ask for anything else (default setting)
            if (PrintScaleShrink == pd.scaleAdv)
                zoom = min(dpiFactor, zoom);
            else if (PrintScaleNone == pd.scaleAdv)
                zoom = dpiFactor;

#ifdef USE_GDI_FOR_PRINTING
            RectI rc = RectI::FromXY((int)(printableWidth - sSize.dx * zoom) / 2,
                                     (int)(printableHeight - sSize.dy * zoom) / 2,
                                     paperWidth, paperHeight);
            engine.RenderPage(hdc, rc, pd.sel.At(i).pageNo, zoom, pd.rotation, clipRegion, Target_Print);
#else
            RenderedBitmap *bmp = engine.RenderBitmap(pd.sel.At(i).pageNo, zoom, pd.rotation, clipRegion, Target_Print);
            if (bmp) {
                PointI TL((printableWidth - bmp->Size().dx) / 2,
                          (printableHeight - bmp->Size().dy) / 2);
                bmp->StretchDIBits(hdc, RectI(TL, bmp->Size()));
                delete bmp;
            }
#endif
            if (EndPage(hdc) <= 0) {
                AbortDoc(hdc);
                return;
            }

            current++;
            if (progressUI) {
                if (progressUI->WasCanceled()) {
                    AbortDoc(hdc);
                    return;
                }
                progressUI->UpdateProgress(current, total);
            }
        }

        EndDoc(hdc);
        return;
    }

    // print all the pages the user requested
    for (size_t i = 0; i < pd.ranges.Count(); i++) {
        int dir = pd.ranges.At(i).nFromPage > pd.ranges.At(i).nToPage ? -1 : 1;
        for (DWORD pageNo = pd.ranges.At(i).nFromPage; pageNo != pd.ranges.At(i).nToPage + dir; pageNo += dir) {
            if ((PrintRangeEven == pd.rangeAdv && pageNo % 2 != 0) ||
                (PrintRangeOdd == pd.rangeAdv && pageNo % 2 == 0))
                continue;

            StartPage(hdc);
            // MM_TEXT: Each logical unit is mapped to one device pixel.
            // Positive x is to the right; positive y is down.

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
            PointI offset(-leftMargin, -topMargin);

            if (pd.scaleAdv != PrintScaleNone) {
                // make sure to fit all content into the printable area when scaling
                // and the whole document page on the physical paper
                RectD rect = engine.PageContentBox(pageNo, Target_Print);
                RectT<float> cbox = engine.Transform(rect, pageNo, 1.0, rotation).Convert<float>();
                zoom = min((float)printableWidth / cbox.dx,
                       min((float)printableHeight / cbox.dy,
                       min((float)paperWidth / pSize.dx,
                           (float)paperHeight / pSize.dy)));
                // use the correct zoom values, if the page fits otherwise
                // and the user didn't ask for anything else (default setting)
                if (PrintScaleShrink == pd.scaleAdv && dpiFactor < zoom)
                    zoom = dpiFactor;
                // make sure that no content lies in the non-printable paper margins
                RectT<float> onPaper((paperWidth - pSize.dx * zoom) / 2 + cbox.x * zoom,
                                    (paperHeight - pSize.dy * zoom) / 2 + cbox.y * zoom,
                                    cbox.dx * zoom, cbox.dy * zoom);
                if (leftMargin > onPaper.x)
                    offset.x += (int)(leftMargin - onPaper.x);
                else if (paperWidth - rightMargin < onPaper.BR().x)
                    offset.x += (int)((paperWidth - rightMargin) - onPaper.BR().x);
                if (topMargin > onPaper.y)
                    offset.y += (int)(topMargin - onPaper.y);
                else if (paperHeight - bottomMargin < onPaper.BR().y)
                    offset.y += (int)((paperHeight - bottomMargin) - onPaper.BR().y);
            }

#ifdef USE_GDI_FOR_PRINTING
            RectI rc = RectI::FromXY((int)(paperWidth - pSize.dx * zoom) / 2 + offset.x,
                                     (int)(paperHeight - pSize.dy * zoom) / 2 + offset.y,
                                     paperWidth, paperHeight);
            engine.RenderPage(hdc, rc, pageNo, zoom, rotation, NULL, Target_Print);
#else
            RenderedBitmap *bmp = engine.RenderBitmap(pageNo, zoom, rotation, NULL, Target_Print);
            if (bmp) {
                PointI TL((paperWidth - bmp->Size().dx) / 2 + offset.x,
                          (paperHeight - bmp->Size().dy) / 2 + offset.y);
                bmp->StretchDIBits(hdc, RectI(TL, bmp->Size()));
                delete bmp;
            }
#endif
            if (EndPage(hdc) <= 0) {
                AbortDoc(hdc);
                return;
            }

            current++;
            if (progressUI) {
                if (progressUI->WasCanceled()) {
                    AbortDoc(hdc);
                    return;
                }
                progressUI->UpdateProgress(current, total);
            }
        }
    }

    EndDoc(hdc);
}

class PrintThreadUpdateWorkItem : public UIThreadWorkItem {
    NotificationWnd *wnd;
    int current, total;

public:
    PrintThreadUpdateWorkItem(WindowInfo *win, NotificationWnd *wnd, int current, int total)
        : UIThreadWorkItem(win), wnd(wnd), current(current), total(total) { }

    virtual void Execute() {
        if (WindowInfoStillValid(win) && win->notifications->Contains(wnd))
            wnd->UpdateProgress(current, total);
    }
};

class PrintThreadData : public ProgressUpdateUI, public NotificationWndCallback, public UIThreadWorkItem {
    NotificationWnd *wnd;
    bool isCanceled;

public:
    PrintData *data;
    HANDLE thread; // close the print thread handle after execution

    PrintThreadData(WindowInfo *win, PrintData *data) :
        UIThreadWorkItem(win), data(data), isCanceled(false), thread(NULL) {
        wnd = new NotificationWnd(win->hwndCanvas, _T(""), _TR("Printing page %d of %d..."), this);
        win->notifications->Add(wnd);
    }

    ~PrintThreadData() {
        CloseHandle(thread);
        delete data;
        RemoveNotification(wnd);
    }

    virtual void UpdateProgress(int current, int total) {
        QueueWorkItem(new PrintThreadUpdateWorkItem(win, wnd, current, total));
    }

    virtual bool WasCanceled() {
        return isCanceled || !WindowInfoStillValid(win) || win->printCanceled;
    }

    // called when printing has been canceled
    virtual void RemoveNotification(NotificationWnd *wnd) {
        isCanceled = true;
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
        PrintToDevice(*threadData->data, threadData);
        QueueWorkItem(threadData);
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

Note: The following doesn't apply for USE_GDI_FOR_PRINTING

Creates a new dummy page for each page with a large zoom factor,
and then uses StretchDIBits to copy this to the printer's dc.

So far have tested printing from XP to
 - Acrobat Professional 6 (note that acrobat is usually set to
   downgrade the resolution of its bitmaps to 150dpi)
 - HP Laserjet 2300d
 - HP Deskjet D4160
 - Lexmark Z515 inkjet, which should cover most bases.

In order to print with Adobe Reader instead: ViewWithAcrobat(win, _T("/P"));
*/
#define MAXPAGERANGES 10
void OnMenuPrint(WindowInfo *win, bool waitForCompletion)
{
    // we remember some printer settings per process
    static ScopedMem<DEVMODE> defaultDevMode;
    static PrintScaleAdv defaultScaleAdv = PrintScaleShrink;

    bool printSelection = false;
    Vec<PRINTPAGERANGE> ranges;

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
    pd.Flags       = PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE | PD_COLLATE;
    if (!win->selectionOnPage)
        pd.Flags |= PD_NOSELECTION;
    pd.nCopies     = 1;
    /* by default print all pages */
    pd.nPageRanges = 1;
    pd.nMaxPageRanges = MAXPAGERANGES;
    PRINTPAGERANGE *ppr = SAZA(PRINTPAGERANGE, MAXPAGERANGES);
    pd.lpPageRanges = ppr;
    ppr->nFromPage = 1;
    ppr->nToPage = dm->PageCount();
    pd.nMinPage = 1;
    pd.nMaxPage = dm->PageCount();
    pd.nStartPage = START_PAGE_GENERAL;

    Print_Advanced_Data advanced = { PrintRangeAll, PrintScaleShrink };
    ScopedMem<DLGTEMPLATE> dlgTemplate; // needed for RTL languages
    HPROPSHEETPAGE hPsp = CreatePrintAdvancedPropSheet(&advanced, dlgTemplate);
    pd.lphPropertyPages = &hPsp;
    pd.nPropertyPages = 1;

    // restore remembered settings
    if (defaultDevMode)
        pd.hDevMode = GlobalMemDup(defaultDevMode.Get(), defaultDevMode.Get()->dmSize + defaultDevMode.Get()->dmDriverExtra);
    advanced.scale = defaultScaleAdv;

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
    }

    if (pd.dwResultAction != PD_RESULT_PRINT)
        goto Exit;

    if (!PrinterSupportsStretchDib(win->hwndFrame, pd.hDC))
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

    LPDEVMODE devMode = (LPDEVMODE)GlobalLock(pd.hDevMode);
    PrintData *data = new PrintData(dm->engine, pd.hDC, devMode, ranges,
                                    dm->Rotation(), advanced.range, advanced.scale,
                                    printSelection ? win->selectionOnPage : NULL);
    pd.hDC = NULL; // deleted by PrintData
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
    DeleteDC(pd.hDC);
    GlobalFree(pd.hDevNames);
    GlobalFree(pd.hDevMode);
}

static void ApplyPrintSettings(const TCHAR *settings, int pageCount, Vec<PRINTPAGERANGE>& ranges, Print_Advanced_Data& advanced)
{
    StrVec rangeList;
    if (settings)
        rangeList.Split(settings, _T(","), true);

    for (size_t i = 0; i < rangeList.Count(); i++) {
        PRINTPAGERANGE pr;
        if (str::Parse(rangeList.At(i), _T("%d-%d%$"), &pr.nFromPage, &pr.nToPage)) {
            pr.nFromPage = limitValue(pr.nFromPage, (DWORD)1, (DWORD)pageCount);
            pr.nToPage = limitValue(pr.nToPage, (DWORD)1, (DWORD)pageCount);
            ranges.Append(pr);
        }
        else if (str::Parse(rangeList.At(i), _T("%d%$"), &pr.nFromPage)) {
            pr.nFromPage = pr.nToPage = limitValue(pr.nFromPage, (DWORD)1, (DWORD)pageCount);
            ranges.Append(pr);
        }
        else if (str::Eq(rangeList.At(i), _T("even")))
            advanced.range = PrintRangeEven;
        else if (str::Eq(rangeList.At(i), _T("odd")))
            advanced.range = PrintRangeOdd;
        else if (str::Eq(rangeList.At(i), _T("noscale")))
            advanced.scale = PrintScaleNone;
        else if (str::Eq(rangeList.At(i), _T("shrink")))
            advanced.scale = PrintScaleShrink;
        else if (str::Eq(rangeList.At(i), _T("fit")))
            advanced.scale = PrintScaleFit;
    }

    if (ranges.Count() == 0) {
        PRINTPAGERANGE pr = { 1, pageCount };
        ranges.Append(pr);
    }
}

bool PrintFile(const TCHAR *fileName, const TCHAR *printerName, bool displayErrors, const TCHAR *settings)
{
    if (!HasPermission(Perm_PrinterAccess))
        return false;

    ScopedMem<TCHAR> fileName2(path::Normalize(fileName));
    BaseEngine *engine = EngineManager::CreateEngine(fileName2);
    if (!engine || !engine->IsPrintingAllowed()) {
        if (displayErrors)
            MessageBox(NULL, _TR("Cannot print this file"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK | (IsUIRightToLeft() ? MB_RTLREADING : 0));
        return false;
    }

    HANDLE printer;
    bool ok = OpenPrinter((LPTSTR)printerName, &printer, NULL);
    if (!ok) {
        if (displayErrors)
            MessageBox(NULL, _TR("Printer with given name doesn't exist"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK | (IsUIRightToLeft() ? MB_RTLREADING : 0));
        return false;
    }

    // get printer driver information
    DWORD needed = 0;
    GetPrinter(printer, 2, NULL, 0, &needed);
    ScopedMem<BYTE> infoData(SAZA(BYTE, needed));
    if (infoData)
        ok = GetPrinter(printer, 2, infoData, needed, &needed);
    if (!ok || !infoData || needed <= sizeof(PRINTER_INFO_2)) goto Exit;

    DWORD structSize = DocumentProperties(NULL,
        printer,                /* Handle to our printer. */
        (LPTSTR)printerName,    /* Name of the printer. */ 
        NULL,                   /* Asking for size, so */
        NULL,                   /* these are not used. */
        0);                     /* Zero returns buffer size. */
    LPDEVMODE devMode = (LPDEVMODE)malloc(structSize);
    HDC hdcPrint = NULL;
    if (!devMode) goto Exit;

    // Get the default DevMode for the printer and modify it for your needs.
    DWORD returnCode = DocumentProperties(NULL,
        printer,
        (LPTSTR)printerName,
        devMode,        /* The address of the buffer to fill. */
        NULL,           /* Not using the input buffer. */
        DM_OUT_BUFFER); /* Have the output buffer filled. */
    if (IDOK != returnCode) {
        // If failure, inform the user, cleanup and return failure.
        if (displayErrors)
            MessageBox(NULL, _TR("Could not obtain Printer properties"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK | (IsUIRightToLeft() ? MB_RTLREADING : 0));
        goto Exit;
    }

    /*
     * Merge the new settings with the old.
     * This gives the driver an opportunity to update any private
     * portions of the DevMode structure.
     */
    DocumentProperties(NULL,
        printer,
        (LPTSTR)printerName,
        devMode,        /* Reuse our buffer for output. */
        devMode,        /* Pass the driver our changes. */
        DM_IN_BUFFER |  /* Commands to Merge our changes and */
        DM_OUT_BUFFER); /* write the result. */

    ClosePrinter(printer);

    PRINTER_INFO_2 *info = (PRINTER_INFO_2 *)infoData.Get();
    hdcPrint = CreateDC(info->pDriverName, info->pPrinterName, info->pPortName, devMode);
    if (!hdcPrint) {
        if (displayErrors)
            MessageBox(NULL, _TR("Couldn't initialize printer"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK | (IsUIRightToLeft() ? MB_RTLREADING : 0));
        goto Exit;
    }
    if (PrinterSupportsStretchDib(NULL, hdcPrint)) {
        Vec<PRINTPAGERANGE> ranges;
        Print_Advanced_Data advanced = { PrintRangeAll, PrintScaleShrink };
        ApplyPrintSettings(settings, engine->PageCount(), ranges, advanced);

        PrintData pd(engine, hdcPrint, devMode, ranges, 0, advanced.range, advanced.scale);
        hdcPrint = NULL; // deleted by PrintData
        PrintToDevice(pd);
        ok = true;
    }
Exit:
    free(devMode);
    DeleteDC(hdcPrint);
    return ok;
}
