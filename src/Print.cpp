/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/UITask.h"
#include "utils/WinUtil.h"
#include "utils/Log.h"

#include "wingui/TreeModel.h"
#include "DisplayMode.h"
#include "Controller.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "SettingsStructs.h"
#include "GlobalPrefs.h"
#include "ChmModel.h"
#include "DisplayModel.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "TabInfo.h"
#include "AppUtil.h"
#include "Print.h"
#include "Selection.h"
#include "SumatraDialogs.h"
#include "SumatraProperties.h"
#include "Translations.h"

class AbortCookieManager {
    CRITICAL_SECTION cookieAccess;

  public:
    AbortCookie* cookie = nullptr;

    AbortCookieManager() {
        InitializeCriticalSection(&cookieAccess);
    }
    ~AbortCookieManager() {
        Clear();
        DeleteCriticalSection(&cookieAccess);
    }

    void Abort() {
        ScopedCritSec scope(&cookieAccess);
        if (cookie) {
            cookie->Abort();
        }
        Clear();
    }

    void Clear() {
        ScopedCritSec scope(&cookieAccess);
        if (cookie) {
            delete cookie;
            cookie = nullptr;
        }
    }
};

struct PrintData {
    Printer* printer{nullptr};
    EngineBase* engine{nullptr};
    Vec<PRINTPAGERANGE> ranges; // empty when printing a selection
    Vec<SelectionOnPage> sel;   // empty when printing a page range
    Print_Advanced_Data advData;
    int rotation{0};
    ProgressUpdateUI* progressUI{nullptr};
    AbortCookieManager* abortCookie{nullptr};

    PrintData(EngineBase* engine, Printer* printer, Vec<PRINTPAGERANGE>& ranges, Print_Advanced_Data& advData,
              int rotation = 0, Vec<SelectionOnPage>* sel = nullptr) {
        this->printer = printer;
        this->advData = advData;
        this->rotation = rotation;
        if (engine) {
            this->engine = engine->Clone();
        }

        if (!sel) {
            this->ranges = ranges;
        } else {
            this->sel = *sel;
        }
    }

    ~PrintData() {
        delete printer;
        delete engine;
    }
};

void Printer::SetDevMode(DEVMODEW* dm) {
    free((void*)devMode);
    devMode = dm;
}

Printer::~Printer() {
    str::Free(name);
    free((void*)devMode);

    for (int i = 0; i < nPaperSizes; i++) {
        str::Free(paperNames[i]);
    }

    for (int i = 0; i < nBins; i++) {
        str::Free(binNames[i]);
    }

    free((void*)papers);
    free((void*)paperNames);
    free((void*)paperSizes);

    free((void*)bins);
    free((void*)binNames);
}

// get all the important info about a printer
Printer* NewPrinter(WCHAR* printerName) {
    HANDLE hPrinter{nullptr};
    LONG ret{0};
    Printer* printer{nullptr};
    BOOL ok = OpenPrinterW(printerName, &hPrinter, nullptr);
    if (!ok) {
        return nullptr;
    }

    LONG structSize = 0;
    LPDEVMODE devMode = nullptr;

    DWORD needed = 0;
    GetPrinterW(hPrinter, 2, nullptr, 0, &needed);
    PRINTER_INFO_2* info = (PRINTER_INFO_2*)AllocArray<BYTE>(needed);
    if (info) {
        ok = GetPrinterW(hPrinter, 2, (LPBYTE)info, needed, &needed);
    }
    if (!ok || !info || needed <= sizeof(PRINTER_INFO_2)) {
        goto Exit;
    }

    /* ask for the size of DEVMODE struct */
    structSize = DocumentPropertiesW(nullptr, hPrinter, printerName, nullptr, nullptr, 0);
    if (structSize < sizeof(DEVMODEW)) {
        // if (displayErrors) {
        //    MessageBoxWarning(nullptr, _TR("Could not obtain Printer properties"), _TR("Printing problem."));
        //}
        goto Exit;
    }
    devMode = (DEVMODEW*)Allocator::AllocZero(nullptr, structSize);

    // Get the default DevMode for the printer and modify it for your needs.
    ret = DocumentPropertiesW(nullptr, hPrinter, printerName, devMode, nullptr, DM_OUT_BUFFER);
    if (IDOK != ret) {
        // if (displayErrors) {
        //    MessageBoxWarning(nullptr, _TR("Could not obtain Printer properties"), _TR("Printing problem."));
        //}
        goto Exit;
    }

    printer = new Printer();
    printer->name = str::Dup(printerName);
    printer->devMode = devMode;
    printer->info = info;

    {
        DWORD n = DeviceCapabilitiesW(printerName, nullptr, DC_PAPERS, nullptr, nullptr);
        DWORD n2 = DeviceCapabilitiesW(printerName, nullptr, DC_PAPERNAMES, nullptr, nullptr);
        DWORD n3 = DeviceCapabilitiesW(printerName, nullptr, DC_PAPERSIZE, nullptr, nullptr);
        if (n != n2 || n != n3 || 0 == n || ((DWORD)-1 == n)) {
            delete printer;
            return nullptr;
        }
        printer->nPaperSizes = (int)n;
        size_t paperNameSize = 64;
        printer->papers = AllocArray<WORD>(n);
        WCHAR* paperNamesSeq = AllocArray<WCHAR>(paperNameSize * (size_t)n + 1); // +1 is "just in case"
        printer->paperSizes = AllocArray<POINT>(n);
        printer->paperNames = AllocArray<WCHAR*>(n);

        DeviceCapabilitiesW(printerName, nullptr, DC_PAPERS, (WCHAR*)printer->papers, nullptr);
        DeviceCapabilitiesW(printerName, nullptr, DC_PAPERNAMES, paperNamesSeq, nullptr);
        DeviceCapabilitiesW(printerName, nullptr, DC_PAPERSIZE, (WCHAR*)printer->paperSizes, nullptr);

        WCHAR* paperName = paperNamesSeq;
        for (int i = 0; i < (int)n; i++) {
            printer->paperNames[i] = str::Dup(paperName);
            paperName += paperNameSize;
        }
        str::Free(paperNamesSeq);
    }

    {
        DWORD n = DeviceCapabilitiesW(printerName, nullptr, DC_BINS, nullptr, nullptr);
        DWORD n2 = DeviceCapabilitiesW(printerName, nullptr, DC_BINNAMES, nullptr, nullptr);
        if (n != n2 || ((DWORD)-1 == n)) {
            delete printer;
            return nullptr;
        }
        printer->nBins = n;
        // it's ok for nBins to be 0, it means there's only one, default bin
        if (n > 0) {
            size_t binNameSize = 24;
            printer->bins = AllocArray<WORD>(n);
            printer->binNames = AllocArray<WCHAR*>(n);
            WCHAR* binNamesSeq = AllocArray<WCHAR>(binNameSize * n + 1); // +1 is "just in case"
            DeviceCapabilitiesW(printerName, nullptr, DC_BINS, (WCHAR*)printer->bins, nullptr);
            DeviceCapabilitiesW(printerName, nullptr, DC_BINNAMES, binNamesSeq, nullptr);
            WCHAR* binName = binNamesSeq;
            for (int i = 0; i < (int)n; i++) {
                printer->binNames[i] = str::Dup(binName);
                binName += binNameSize;
            }
            str::Free(binNamesSeq);
        }
    }

    {
        DWORD n;

        n = DeviceCapabilitiesW(printerName, nullptr, DC_COLLATE, nullptr, nullptr);
        printer->canCallate = (n > 0);

        n = DeviceCapabilitiesW(printerName, nullptr, DC_COLORDEVICE, nullptr, nullptr);
        printer->isColor = (n > 0);

        n = DeviceCapabilitiesW(printerName, nullptr, DC_DUPLEX, nullptr, nullptr);
        printer->isDuplex = (n > 0);

        n = DeviceCapabilitiesW(printerName, nullptr, DC_STAPLE, nullptr, nullptr);
        printer->canStaple = (n > 0);

        n = DeviceCapabilitiesW(printerName, nullptr, DC_ORIENTATION, nullptr, nullptr);
        printer->orientation = n;
    }

Exit:
    ClosePrinter(hPrinter);
    return printer;
}

static void SetCustomPaperSize(Printer* printer, SizeF size) {
    auto devMode = printer->devMode;
    devMode->dmPaperSize = 0;
    devMode->dmPaperWidth = size.dx;
    devMode->dmPaperLength = size.dy;
    devMode->dmFields |= DM_PAPERSIZE | DM_PAPERWIDTH | DM_PAPERLENGTH;
}

// Make sure dy > dx i.e. it's tall not wide
static Size NormalizePaperSize(Size s) {
    if (s.dy > s.dx) {
        return Size(s.dx, s.dy);
    }
    return Size(s.dy, s.dx);
}

static void MessageBoxWarningCond(bool show, const WCHAR* msg, const WCHAR* title) {
    logf(L"%s: %s\n", title, msg);
    if (!show) {
        return;
    }
    MessageBoxWarning(nullptr, msg, title);
}

static RectF BoundSelectionOnPage(const Vec<SelectionOnPage>& sel, int pageNo) {
    RectF bounds;
    for (size_t i = 0; i < sel.size(); i++) {
        if (sel.at(i).pageNo == pageNo) {
            bounds = bounds.Union(sel.at(i).rect);
        }
    }
    return bounds;
}

static bool PrintToDevice(const PrintData& pd) {
    CrashIf(!pd.engine);
    if (!pd.engine) {
        return false;
    }
    CrashIf(!pd.printer);
    if (!pd.printer) {
        return false;
    }

    auto progressUI = pd.progressUI;
    auto abortCookie = pd.abortCookie;

    EngineBase& engine = *pd.engine;
    AutoFreeWstr fileName;

    DOCINFOW di{};
    di.cbSize = sizeof(DOCINFO);
    if (gPluginMode) {
        fileName.Set(url::GetFileName(gPluginURL));
        // fall back to a generic "filename" instead of the more confusing temporary filename
        di.lpszDocName = fileName ? fileName.Get() : L"filename";
    } else {
        di.lpszDocName = engine.FileName();
    }

    int current = 1, total = 0;
    if (pd.sel.size() == 0) {
        for (size_t i = 0; i < pd.ranges.size(); i++) {
            if (pd.ranges.at(i).nToPage < pd.ranges.at(i).nFromPage) {
                total += pd.ranges.at(i).nFromPage - pd.ranges.at(i).nToPage + 1;
            } else {
                total += pd.ranges.at(i).nToPage - pd.ranges.at(i).nFromPage + 1;
            }
        }
    } else {
        for (int pageNo = 1; pageNo <= engine.PageCount(); pageNo++) {
            if (!BoundSelectionOnPage(pd.sel, pageNo).IsEmpty()) {
                total++;
            }
        }
    }
    CrashIf(total <= 0);
    if (0 == total) {
        return false;
    }

    if (progressUI) {
        progressUI->UpdateProgress(current, total);
    }

    auto devMode = pd.printer->devMode;
    // http://blogs.msdn.com/b/oldnewthing/archive/2012/11/09/10367057.aspx
    AutoDeleteDC hdc(CreateDCW(nullptr, pd.printer->name, nullptr, devMode));
    if (!hdc) {
        return false;
    }

    // for PDF Printer, this shows a file dialog to pick file name for destination PDF
    if (StartDoc(hdc, &di) <= 0) {
        return false;
    }

    // MM_TEXT: Each logical unit is mapped to one device pixel.
    // Positive x is to the right; positive y is down.
    SetMapMode(hdc, MM_TEXT);

    const Size paperSize(GetDeviceCaps(hdc, PHYSICALWIDTH), GetDeviceCaps(hdc, PHYSICALHEIGHT));
    const Rect printable(GetDeviceCaps(hdc, PHYSICALOFFSETX), GetDeviceCaps(hdc, PHYSICALOFFSETY),
                         GetDeviceCaps(hdc, HORZRES), GetDeviceCaps(hdc, VERTRES));
    float fileDPI = engine.GetFileDPI();
    float px = (float)GetDeviceCaps(hdc, LOGPIXELSX);
    float py = (float)GetDeviceCaps(hdc, LOGPIXELSY);
    float dpiFactor = std::min(px / fileDPI, py / fileDPI);
    bool bPrintPortrait = paperSize.dx < paperSize.dy;
    if (devMode && (devMode->dmFields & DM_ORIENTATION)) {
        bPrintPortrait = DMORIENT_PORTRAIT == devMode->dmOrientation;
    }
    if (pd.advData.rotation == PrintRotationAdv::Portrait) {
        bPrintPortrait = true;
    } else if (pd.advData.rotation == PrintRotationAdv::Landscape) {
        bPrintPortrait = false;
    }

    if (pd.sel.size() > 0) {
        for (int pageNo = 1; pageNo <= engine.PageCount(); pageNo++) {
            RectF bounds = BoundSelectionOnPage(pd.sel, pageNo);
            if (bounds.IsEmpty()) {
                continue;
            }

            if (progressUI) {
                progressUI->UpdateProgress(current, total);
            }

            StartPage(hdc);

            SizeF bSize = bounds.Size();
            float zoom = std::min((float)printable.dx / bSize.dx, (float)printable.dy / bSize.dy);
            // use the correct zoom values, if the page fits otherwise
            // and the user didn't ask for anything else (default setting)
            if (PrintScaleAdv::Shrink == pd.advData.scale) {
                zoom = std::min(dpiFactor, zoom);
            } else if (PrintScaleAdv::None == pd.advData.scale) {
                zoom = dpiFactor;
            }

            for (size_t i = 0; i < pd.sel.size(); i++) {
                if (pd.sel.at(i).pageNo != pageNo) {
                    continue;
                }

                RectF* clipRegion = &pd.sel.at(i).rect;
                Point offset((int)((clipRegion->x - bounds.x) * zoom), (int)((clipRegion->y - bounds.y) * zoom));
                if (pd.advData.scale != PrintScaleAdv::None) {
                    // center the selection on the physical paper
                    offset.x += (int)(printable.dx - bSize.dx * zoom) / 2;
                    offset.y += (int)(printable.dy - bSize.dy * zoom) / 2;
                }

                bool ok = false;
                short shrink = 1;
                do {
                    RenderPageArgs args(pd.sel.at(i).pageNo, zoom / shrink, pd.rotation, clipRegion,
                                        RenderTarget::Print);
                    if (abortCookie) {
                        args.cookie_out = &abortCookie->cookie;
                    }
                    RenderedBitmap* bmp = engine.RenderPage(args);
                    if (abortCookie) {
                        abortCookie->Clear();
                    }
                    if (bmp && bmp->GetBitmap()) {
                        Rect rc(offset.x, offset.y, bmp->Size().dx * shrink, bmp->Size().dy * shrink);
                        ok = bmp->StretchDIBits(hdc, rc);
                    }
                    delete bmp;
                    shrink *= 2;
                } while (!ok && shrink < 32 && !(progressUI && progressUI->WasCanceled()));
            }
            // TODO: abort if !ok?

            if (EndPage(hdc) <= 0 || (progressUI && progressUI->WasCanceled())) {
                AbortDoc(hdc);
                return false;
            }
            current++;
        }

        EndDoc(hdc);
        return false;
    }

    // print all the pages the user requested
    for (size_t i = 0; i < pd.ranges.size(); i++) {
        int dir = pd.ranges.at(i).nFromPage > pd.ranges.at(i).nToPage ? -1 : 1;
        for (DWORD pageNo = pd.ranges.at(i).nFromPage; pageNo != pd.ranges.at(i).nToPage + dir; pageNo += dir) {
            if ((PrintRangeAdv::Even == pd.advData.range && pageNo % 2 != 0) ||
                (PrintRangeAdv::Odd == pd.advData.range && pageNo % 2 == 0)) {
                continue;
            }
            if (progressUI) {
                progressUI->UpdateProgress(current, total);
            }

            StartPage(hdc);

            SizeF pSize = engine.PageMediabox(pageNo).Size();
            int rotation = 0;
            // Turn the document by 90 deg if it isn't in portrait mode
            if (pSize.dx > pSize.dy) {
                rotation += 90;
                std::swap(pSize.dx, pSize.dy);
            }
            // make sure not to print upside-down
            rotation = (rotation % 180) == 0 ? 0 : 270;
            // finally turn the page by (another) 90 deg in landscape mode
            if (!bPrintPortrait) {
                rotation = (rotation + 90) % 360;
                std::swap(pSize.dx, pSize.dy);
            }

            // dpiFactor means no physical zoom
            float zoom = dpiFactor;
            // offset of the top-left corner of the page from the printable area
            // (negative values move the page into the left/top margins, etc.);
            // offset adjustments are needed because the GDI coordinate system
            // starts at the corner of the printable area and we rather want to
            // center the page on the physical paper (except for PrintScaleNone
            // where the page starts at the very top left of the physical paper so
            // that printing forms/labels of varying size remains reliably possible)
            Point offset(-printable.x, -printable.y);

            if (pd.advData.scale != PrintScaleAdv::None) {
                // make sure to fit all content into the printable area when scaling
                // and the whole document page on the physical paper
                RectF rect = engine.PageContentBox(pageNo, RenderTarget::Print);
                RectF cbox = engine.Transform(rect, pageNo, 1.0, rotation);
                zoom = std::min((float)printable.dx / cbox.dx,
                                std::min((float)printable.dy / cbox.dy,
                                         std::min((float)paperSize.dx / pSize.dx, (float)paperSize.dy / pSize.dy)));
                // use the correct zoom values, if the page fits otherwise
                // and the user didn't ask for anything else (default setting)
                if (PrintScaleAdv::Shrink == pd.advData.scale && dpiFactor < zoom) {
                    zoom = dpiFactor;
                }
                // center the page on the physical paper
                offset.x += (int)(paperSize.dx - pSize.dx * zoom) / 2;
                offset.y += (int)(paperSize.dy - pSize.dy * zoom) / 2;
                // make sure that no content lies in the non-printable paper margins
                RectF onPaper(printable.x + offset.x + cbox.x * zoom, printable.y + offset.y + cbox.y * zoom,
                              cbox.dx * zoom, cbox.dy * zoom);
                if (onPaper.x < printable.x) {
                    offset.x += (int)(printable.x - onPaper.x);
                } else if (onPaper.BR().x > printable.BR().x) {
                    offset.x -= (int)(onPaper.BR().x - printable.BR().x);
                }
                if (onPaper.y < printable.y) {
                    offset.y += (int)(printable.y - onPaper.y);
                } else if (onPaper.BR().y > printable.BR().y) {
                    offset.y -= (int)(onPaper.BR().y - printable.BR().y);
                }
            }

            bool ok = false;
            short shrink = 1;
            do {
                RenderPageArgs args(pageNo, zoom / shrink, rotation, nullptr, RenderTarget::Print);
                if (abortCookie) {
                    args.cookie_out = &abortCookie->cookie;
                }
                RenderedBitmap* bmp = engine.RenderPage(args);
                if (abortCookie) {
                    abortCookie->Clear();
                }
                if (bmp && bmp->GetBitmap()) {
                    auto size = bmp->Size();
                    Rect rc(offset.x, offset.y, size.dx * shrink, size.dy * shrink);
                    ok = bmp->StretchDIBits(hdc, rc);
                }
                delete bmp;
                shrink *= 2;
            } while (!ok && shrink < 32 && !(progressUI && progressUI->WasCanceled()));
            // TODO: abort if !ok?

            if (EndPage(hdc) <= 0 || (progressUI && progressUI->WasCanceled())) {
                AbortDoc(hdc);
                return false;
            }
            current++;
        }
    }

    EndDoc(hdc);
    return true;
}

class PrintThreadData : public ProgressUpdateUI {
  public:
    NotificationWnd* wnd = nullptr;
    AbortCookieManager cookie;
    bool isCanceled = false;
    WindowInfo* win = nullptr;

    PrintData* data = nullptr;
    HANDLE thread = nullptr; // close the print thread handle after execution

    PrintThreadData(WindowInfo* win, PrintData* data) {
        this->win = win;
        this->data = data;
        wnd = new NotificationWnd(win->hwndCanvas, 0);
        wnd->wndRemovedCb = [this](NotificationWnd* wnd) { this->RemoveNotification(wnd); };
        wnd->Create(L"", _TR("Printing page %d of %d..."));

        // don't use a groupId for this notification so that
        // multiple printing notifications could coexist between tabs
        win->notifications->Add(wnd, nullptr);
    }
    PrintThreadData(PrintThreadData const&) = delete;
    PrintThreadData& operator=(PrintThreadData const&) = delete;

    ~PrintThreadData() override {
        CloseHandle(thread);
        delete data;
        RemoveNotification(wnd);
    }

    // called when printing has been canceled
    void RemoveNotification(NotificationWnd* wnd) {
        isCanceled = true;
        cookie.Abort();
        this->wnd = nullptr;
        if (WindowInfoStillValid(win)) {
            win->notifications->RemoveNotification(wnd);
        }
    }

    void UpdateProgress(int current, int total) override {
        uitask::Post([=] {
            if (WindowInfoStillValid(win) && win->notifications->Contains(wnd)) {
                wnd->UpdateProgress(current, total);
            }
        });
    }

    bool WasCanceled() override {
        return isCanceled || !WindowInfoStillValid(win) || win->printCanceled;
    }
};

static DWORD WINAPI PrintThread(LPVOID data) {
    PrintThreadData* threadData = (PrintThreadData*)data;
    WindowInfo* win = threadData->win;
    // wait for PrintToDeviceOnThread to return so that we
    // close the correct handle to the current printing thread
    while (!win->printThread) {
        Sleep(1);
    }

    HANDLE thread = threadData->thread = win->printThread;

    PrintData* pd = threadData->data;
    pd->progressUI = threadData;
    pd->abortCookie = &threadData->cookie;
    PrintToDevice(*pd);

    uitask::Post([=] {
        if (WindowInfoStillValid(win) && thread == win->printThread) {
            win->printThread = nullptr;
        }
        delete threadData;
    });
    DestroyTempAllocator();
    return 0;
}

static void PrintToDeviceOnThread(WindowInfo* win, PrintData* data) {
    CrashIf(win->printThread);
    PrintThreadData* threadData = new PrintThreadData(win, data);
    win->printThread = nullptr;
    win->printThread = CreateThread(nullptr, 0, PrintThread, threadData, 0, nullptr);
}

void AbortPrinting(WindowInfo* win) {
    if (win->printThread) {
        win->printCanceled = true;
        WaitForSingleObject(win->printThread, INFINITE);
    }
    win->printCanceled = false;
}

static HGLOBAL GlobalMemDup(const void* data, size_t len) {
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, len);
    if (!hGlobal) {
        return nullptr;
    }

    void* globalData = GlobalLock(hGlobal);
    if (!globalData) {
        GlobalFree(hGlobal);
        return nullptr;
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
enum { MAXPAGERANGES = 10 };
void OnMenuPrint(WindowInfo* win, bool waitForCompletion) {
    // we remember some printer settings per process
    static ScopedMem<DEVMODE> defaultDevMode;
    static PrintScaleAdv defaultScaleAdv = PrintScaleAdv::Shrink;
    static bool hasDefaults = false;

    Printer* printer{nullptr};

    if (!hasDefaults) {
        hasDefaults = true;
        if (str::EqI(gGlobalPrefs->printerDefaults.printScale, "fit")) {
            defaultScaleAdv = PrintScaleAdv::Fit;
        } else if (str::EqI(gGlobalPrefs->printerDefaults.printScale, "none")) {
            defaultScaleAdv = PrintScaleAdv::None;
        }
    }

    bool printSelection = false;
    Vec<PRINTPAGERANGE> ranges;
    Vec<SelectionOnPage>* sel;

    if (!HasPermission(Perm::PrinterAccess)) {
        return;
    }
    if (!win->IsDocLoaded()) {
        return;
    }

    if (win->AsChm()) {
        // the Print dialog allows access to the file system, so fall back
        // to printing the entire document without dialog if that isn't desired
        bool showUI = HasPermission(Perm::DiskAccess);
        win->AsChm()->PrintCurrentPage(showUI);
        return;
    }
    CrashIf(!win->AsFixed());
    if (!win->AsFixed()) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    int rotation = dm->GetRotation();
    auto engine = dm->GetEngine();

#ifndef DISABLE_DOCUMENT_RESTRICTIONS
    if (!dm->GetEngine()->AllowsPrinting()) {
        return;
    }
#endif

    if (win->printThread) {
        uint type = MB_ICONEXCLAMATION | MB_YESNO | MbRtlReadingMaybe();
        const WCHAR* title = _TR("Printing in progress.");
        const WCHAR* msg = _TR("Printing is still in progress. Abort and start over?");
        int res = MessageBox(win->hwndFrame, msg, title, type);
        if (res == IDNO) {
            return;
        }
    }
    AbortPrinting(win);

    // the Print dialog allows access to the file system, so fall back
    // to printing the entire document without dialog if that isn't desired
    if (!HasPermission(Perm::DiskAccess)) {
        PrintFile(dm->GetEngine());
        return;
    }

    PRINTDLGEXW pdex{};
    pdex.lStructSize = sizeof(PRINTDLGEXW);
    pdex.hwndOwner = win->hwndFrame;
    pdex.Flags = PD_USEDEVMODECOPIESANDCOLLATE | PD_COLLATE;
    if (!win->currentTab->selectionOnPage) {
        pdex.Flags |= PD_NOSELECTION;
    }
    pdex.nCopies = 1;
    /* by default print all pages */
    pdex.nPageRanges = 1;
    pdex.nMaxPageRanges = MAXPAGERANGES;
    PRINTPAGERANGE* ppr = AllocArray<PRINTPAGERANGE>(MAXPAGERANGES);
    pdex.lpPageRanges = ppr;
    ppr->nFromPage = 1;
    ppr->nToPage = dm->PageCount();
    pdex.nMinPage = 1;
    pdex.nMaxPage = dm->PageCount();
    pdex.nStartPage = START_PAGE_GENERAL;

    Print_Advanced_Data advanced(PrintRangeAdv::All, defaultScaleAdv);
    ScopedMem<DLGTEMPLATE> dlgTemplate; // needed for RTL languages
    HPROPSHEETPAGE hPsp = CreatePrintAdvancedPropSheet(&advanced, dlgTemplate);
    pdex.lphPropertyPages = &hPsp;
    pdex.nPropertyPages = 1;

    bool failedEngineClone{false};
    PrintData* pd{nullptr};
    DEVMODE* devMode{nullptr};
    // restore remembered settings
    if (defaultDevMode) {
        DEVMODE* p = defaultDevMode.Get();
        pdex.hDevMode = GlobalMemDup(p, p->dmSize + p->dmDriverExtra);
    }

    if (PrintDlgEx(&pdex) != S_OK) {
        if (CommDlgExtendedError() != 0) {
            /* if PrintDlg was cancelled then
               CommDlgExtendedError is zero, otherwise it returns the
               error code, which we could look at here if we wanted.
               for now just warn the user that printing has stopped
               becasue of an error */
            MessageBoxWarning(win->hwndFrame, _TR("Couldn't initialize printer"), _TR("Printing problem."));
        }
        goto Exit;
    }

    if (!pdex.hDevNames) {
        MessageBoxWarning(win->hwndFrame, _TR("Couldn't get printer name"), _TR("Printing problem."));
        goto Exit;
    }

    {
        DEVNAMES* devNames = (DEVNAMES*)GlobalLock(pdex.hDevNames);
        if (devNames) {
            // printerInfo.pDriverName = (LPWSTR)devNames + devNames->wDriverOffset;
            WCHAR* printerName = (WCHAR*)devNames + devNames->wDeviceOffset;
            printer = NewPrinter(printerName);
            // printerInfo.pPortName = (LPWSTR)devNames + devNames->wOutputOffset;
            GlobalUnlock(pdex.hDevNames);
        }
    }

    if (!printer) {
        MessageBoxWarning(win->hwndFrame, _TR("Couldn't initialize printer"), _TR("Printing problem."));
        goto Exit;
    }

    devMode = (DEVMODEW*)GlobalLock(pdex.hDevMode);

    if (pdex.dwResultAction == PD_RESULT_PRINT || pdex.dwResultAction == PD_RESULT_APPLY) {
        // remember settings for this process
        if (devMode) {
            defaultDevMode.Set((DEVMODEW*)memdup(devMode, devMode->dmSize + devMode->dmDriverExtra));
        }
        defaultScaleAdv = advanced.scale;
    }

    if (devMode) {
        auto dmCopy = (DEVMODEW*)memdup(devMode, devMode->dmSize + devMode->dmDriverExtra);
        printer->SetDevMode(dmCopy);
        GlobalUnlock(pdex.hDevMode);
    }

    if (pdex.dwResultAction != PD_RESULT_PRINT) {
        goto Exit;
    }

    if (pdex.Flags & PD_CURRENTPAGE) {
        PRINTPAGERANGE pr = {(DWORD)dm->CurrentPageNo(), (DWORD)dm->CurrentPageNo()};
        ranges.Append(pr);
    } else if (win->currentTab->selectionOnPage && (pdex.Flags & PD_SELECTION)) {
        printSelection = true;
    } else if (!(pdex.Flags & PD_PAGENUMS)) {
        PRINTPAGERANGE pr = {1, (DWORD)dm->PageCount()};
        ranges.Append(pr);
    } else {
        CrashIf(pdex.nPageRanges <= 0);
        for (DWORD i = 0; i < pdex.nPageRanges; i++) {
            ranges.Append(pdex.lpPageRanges[i]);
        }
    }

    sel = printSelection ? win->currentTab->selectionOnPage : nullptr;
    pd = new PrintData(engine, printer, ranges, advanced, rotation, sel);

    // if a file is missing and the engine can't thus be cloned,
    // we print using the original engine on the main thread
    // so that the document can't be closed and the original engine
    // unexpectedly deleted
    // TODO: instead prevent closing the document so that printing
    // can still happen on a separate thread and be interruptible
    failedEngineClone = dm->GetEngine() && !pd->engine;
    if (failedEngineClone) {
        pd->engine = dm->GetEngine();
    }

    if (!waitForCompletion && !failedEngineClone) {
        PrintToDeviceOnThread(win, pd);
    } else {
        PrintToDevice(*pd);
        if (failedEngineClone) {
            pd->engine = nullptr;
        }
        delete pd;
    }

Exit:
    free(ppr);
    GlobalFree(pdex.hDevNames);
    GlobalFree(pdex.hDevMode);
}

static short GetPaperSize(EngineBase* engine) {
    RectF mediabox = engine->PageMediabox(1);
    SizeF size = engine->Transform(mediabox, 1, 1.0f / engine->GetFileDPI(), 0).Size();

    switch (GetPaperFormat(size)) {
        case PaperFormat::A2:
            return DMPAPER_A2;
        case PaperFormat::A3:
            return DMPAPER_A3;
        case PaperFormat::A4:
            return DMPAPER_A4;
        case PaperFormat::A5:
            return DMPAPER_A5;
        case PaperFormat::A6:
            return DMPAPER_A6;
        case PaperFormat::Letter:
            return DMPAPER_LETTER;
        case PaperFormat::Legal:
            return DMPAPER_LEGAL;
        case PaperFormat::Tabloid:
            return DMPAPER_TABLOID;
        case PaperFormat::Statement:
            return DMPAPER_STATEMENT;
        default:
            return 0;
    }
}

#if 0
static short GetPaperByName(const WCHAR* papername) {
    if (str::EqI(papername, L"letter")) {
        return DMPAPER_LETTER;
    }
    if (str::EqI(papername, L"legal")) {
        return DMPAPER_LEGAL;
    }
    if (str::EqI(papername, L"tabloid")) {
        return DMPAPER_TABLOID;
    }
    if (str::EqI(papername, L"statement")) {
        return DMPAPER_STATEMENT;
    }
    if (str::EqI(papername, L"A2")) {
        return DMPAPER_A2;
    }
    if (str::EqI(papername, L"A3")) {
        return DMPAPER_A3;
    }
    if (str::EqI(papername, L"A4")) {
        return DMPAPER_A4;
    }
    if (str::EqI(papername, L"A5")) {
        return DMPAPER_A5;
    }
    if (str::EqI(papername, L"A6")) {
        return DMPAPER_A6;
    }
    return 0;
}
#endif

// wantedName can be a paper name, like "A6" or number for DMPAPER_* contstants like DMPAPER_LETTER
static short GetPaperByName(Printer* printer, const WCHAR* wantedName) {
    auto devMode = printer->devMode;
    CrashIf(!(devMode->dmFields & DM_PAPERSIZE));
    if (!(devMode->dmFields & DM_PAPERSIZE)) {
        return devMode->dmPaperSize;
    }

    int n = printer->nPaperSizes;
    for (int i = 0; i < n; i++) {
        auto paperName = printer->paperNames[i];
        if (str::EqIS(wantedName, paperName)) {
            return printer->papers[i];
        }
    }

    // alternatively allow indicating the paper directly by number
    DWORD paperId{0};
    if (str::Parse(wantedName, L"%u%$", &paperId)) {
        return (short)paperId;
    }
    return devMode->dmPaperSize;
}

static short GetPaperKind(const WCHAR* kindName) {
    DWORD kind;
    if (str::Parse(kindName, L"%u%$", &kind)) {
        return (short)kind;
    }
    return DMPAPER_USER;
}

static short GetPaperSourceByName(Printer* printer, const WCHAR* binName) {
    auto devMode = printer->devMode;
    CrashIf(!(devMode->dmFields & DM_DEFAULTSOURCE));
    if (!(devMode->dmFields & DM_DEFAULTSOURCE)) {
        return devMode->dmDefaultSource;
    }
    int n = printer->nBins;
    if (n == 0) {
        return devMode->dmDefaultSource;
    }
    for (int i = 0; i < n; i++) {
        WCHAR* currName = printer->binNames[i];
        if (str::EqIS(currName, binName)) {
            return printer->bins[i];
        }
    }
    DWORD count{0};
    // alternatively allow indicating the paper bin directly by number
    if (str::Parse(binName, L"%u%$", &count)) {
        return (short)count;
    }
    return devMode->dmDefaultSource;
}

static void ApplyPrintSettings(Printer* printer, const WCHAR* settings, int pageCount, Vec<PRINTPAGERANGE>& ranges,
                               Print_Advanced_Data& advanced) {
    auto devMode = printer->devMode;
    auto printerName = printer->name;

    WStrVec rangeList;
    if (settings) {
        rangeList.Split(settings, L",", true);
    }

    for (size_t i = 0; i < rangeList.size(); i++) {
        int val;
        PRINTPAGERANGE pr{};
        WCHAR* s = rangeList.at(i);
        if (str::Parse(s, L"%d-%d%$", &pr.nFromPage, &pr.nToPage)) {
            pr.nFromPage = limitValue(pr.nFromPage, (DWORD)1, (DWORD)pageCount);
            pr.nToPage = limitValue(pr.nToPage, (DWORD)1, (DWORD)pageCount);
            ranges.Append(pr);
        } else if (str::Parse(s, L"%d%$", &pr.nFromPage)) {
            pr.nFromPage = pr.nToPage = limitValue(pr.nFromPage, (DWORD)1, (DWORD)pageCount);
            ranges.Append(pr);
        } else if (str::EqI(s, L"even")) {
            advanced.range = PrintRangeAdv::Even;
        } else if (str::EqI(s, L"odd")) {
            advanced.range = PrintRangeAdv::Odd;
        } else if (str::EqI(s, L"noscale")) {
            advanced.scale = PrintScaleAdv::None;
        } else if (str::EqI(s, L"shrink")) {
            advanced.scale = PrintScaleAdv::Shrink;
        } else if (str::EqI(s, L"fit")) {
            advanced.scale = PrintScaleAdv::Fit;
        } else if (str::EqI(s, L"portrait")) {
            advanced.rotation = PrintRotationAdv::Portrait;
        } else if (str::EqI(s, L"landscape")) {
            advanced.rotation = PrintRotationAdv::Landscape;
        } else if (str::Parse(s, L"%dx%$", &val) && 0 < val && val < 1000) {
            devMode->dmCopies = (short)val;
            devMode->dmFields |= DM_COPIES;
        } else if (str::EqI(s, L"simplex")) {
            devMode->dmDuplex = DMDUP_SIMPLEX;
            devMode->dmFields |= DM_DUPLEX;
        } else if (str::EqI(s, L"duplex") || str::EqI(s, L"duplexlong")) {
            devMode->dmDuplex = DMDUP_VERTICAL;
            devMode->dmFields |= DM_DUPLEX;
        } else if (str::EqI(s, L"duplexshort")) {
            devMode->dmDuplex = DMDUP_HORIZONTAL;
            devMode->dmFields |= DM_DUPLEX;
        } else if (str::EqI(s, L"color")) {
            devMode->dmColor = DMCOLOR_COLOR;
            devMode->dmFields |= DM_COLOR;
        } else if (str::EqI(s, L"monochrome")) {
            devMode->dmColor = DMCOLOR_MONOCHROME;
            devMode->dmFields |= DM_COLOR;
        } else if (str::StartsWithI(s, L"bin=")) {
            devMode->dmDefaultSource = GetPaperSourceByName(printer, s + 4);
            devMode->dmFields |= DM_DEFAULTSOURCE;
        } else if (str::StartsWithI(s, L"paper=")) {
            devMode->dmPaperSize = GetPaperByName(printer, s + 6);
            devMode->dmFields |= DM_PAPERSIZE;
        } else if (str::StartsWithI(s, L"paperkind=")) {
            // alternatively allow indicating the paper kind directly by number
            devMode->dmPaperSize = GetPaperKind(s + 10);
            devMode->dmFields |= DM_PAPERSIZE;
        }
    }

    if (ranges.size() == 0) {
        PRINTPAGERANGE pr = {1, (DWORD)pageCount};
        ranges.Append(pr);
    }
}

static short DetectPrinterPaperSize(EngineBase* engine, Printer* printer) {
    // get size of first page in tenths of a millimeter in portrait mode
    RectF mediabox = engine->PageMediabox(1);
    SizeF size = engine->Transform(mediabox, 1, 254.0f / engine->GetFileDPI(), 0).Size();
    Size sizeP = NormalizePaperSize(Size(size.dx, size.dy));

    int n = printer->nPaperSizes;
    auto sizes = printer->paperSizes;
    // find equivalent paper size with 1mm tolerance
    for (int i = 0; i < n; i++) {
        POINT sz = sizes[i];
        Size pSizeP = NormalizePaperSize(Size(sz.x, sz.y));
        if (abs(sizeP.dx - pSizeP.dx) <= 10 && abs(sizeP.dy - pSizeP.dy) <= 10) {
            return printer->papers[i];
        }
    }
    return 0;
}

static void SetPrinterCustomPaperSizeForEngine(EngineBase* engine, Printer* printer) {
    // get size of first page in tenths of a millimeter
    RectF mediabox = engine->PageMediabox(1);
    SizeF size = engine->Transform(mediabox, 1, 254.0f / engine->GetFileDPI(), 0).Size();
    SetCustomPaperSize(printer, size);
}

bool PrintFile(EngineBase* engine, WCHAR* printerName, bool displayErrors, const WCHAR* settings) {
    bool ok = false;
    Printer* printer{nullptr};

    if (!HasPermission(Perm::PrinterAccess)) {
        return false;
    }

#ifndef DISABLE_DOCUMENT_RESTRICTIONS
    if (engine && !engine->AllowsPrinting()) {
        engine = nullptr;
    }
#endif

    if (!engine) {
        MessageBoxWarningCond(displayErrors, _TR("Cannot print this file"), _TR("Printing problem."));
        return false;
    }

    if (printerName) {
        printer = NewPrinter(printerName);
    } else {
        auto defName = GetDefaultPrinterName();
        if (!defName) {
            logf("PrintFile: GetDefaultPrinterName() failed\n");
            return false;
        }
        printer = NewPrinter(defName);
        str::Free(defName);
    }

    if (!printer) {
        MessageBoxWarningCond(displayErrors, _TR("Printer with given name doesn't exist"), _TR("Printing problem."));
        return false;
    }

    // set paper size to match the size of the document's first page
    // (will be overridden by any paper= value in -print-settings)
    auto devMode = printer->devMode;
    devMode->dmPaperSize = GetPaperSize(engine);
    {
        Print_Advanced_Data advanced;
        Vec<PRINTPAGERANGE> ranges;

        ApplyPrintSettings(printer, settings, engine->PageCount(), ranges, advanced);

        if (advanced.rotation == PrintRotationAdv::Auto && devMode->dmPaperSize == 0) {
            if (devMode->dmPaperSize = DetectPrinterPaperSize(engine, printer)) {
                devMode->dmFields |= DM_PAPERSIZE;
            } else {
                SetPrinterCustomPaperSizeForEngine(engine, printer);
            }
        }

        PrintData pd(engine, printer, ranges, advanced);
        ok = PrintToDevice(pd);
        if (!ok) {
            MessageBoxWarningCond(displayErrors, _TR("Couldn't initialize printer"), _TR("Printing problem."));
        }
    }

    delete printer;
    return ok;
}

bool PrintFile(const WCHAR* fileName, WCHAR* printerName, bool displayErrors, const WCHAR* settings) {
    logf(L"PrintFile: file: '%s', printer: '%s'\n", fileName, printerName);
    WCHAR* fileName2 = path::Normalize(fileName);
    EngineBase* engine = CreateEngine(fileName2, nullptr, true);
    if (!engine) {
        WCHAR* msg = str::Format(L"Couldn't open file '%s' for printing", fileName);
        MessageBoxWarningCond(displayErrors, msg, L"Error");
        free(msg);
        return false;
    }
    bool ok = PrintFile(engine, printerName, displayErrors, settings);
    delete engine;
    free(fileName2);
    return ok;
}
