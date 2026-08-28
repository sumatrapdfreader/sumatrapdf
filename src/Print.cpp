/* Copyright 2025 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/ScopedWin.h"
#include "base/File.h"
#include "base/UITask.h"
#include "base/Win.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "AppSettings.h"
#include "ChmModel.h"
#include "MarkdownModel.h"
#include "DisplayModel.h"
#include "TextSelection.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Selection.h"
#include "SumatraDialogs.h"
#include "Translations.h"
#include "PrintWin11.h"
#include "Print.h"

class AbortCookieManager {
    Mutex cookieAccess;

  public:
    AbortCookie* cookie = nullptr;

    AbortCookieManager() = default;
    ~AbortCookieManager() { Clear(); }

    void Abort() {
        // don't call Clear() here: it re-locks cookieAccess, which is a
        // non-recursive SRWLOCK, so we'd self-deadlock. Do the clear inline.
        ScopedMutex scope(&cookieAccess);
        if (cookie) {
            cookie->Abort();
            delete cookie;
            cookie = nullptr;
        }
    }

    void Clear() {
        ScopedMutex scope(&cookieAccess);
        if (cookie) {
            delete cookie;
            cookie = nullptr;
        }
    }
};

struct PrintData {
    Printer* printer = nullptr;
    EngineBase* engine = nullptr;
    Vec<PRINTPAGERANGE> ranges; // empty when printing a selection
    Vec<SelectionOnPage> sel;   // empty when printing a page range
    Print_Advanced_Data advData;
    int rotation = 0;
    ProgressUpdateCb progressCb;
    AbortCookieManager* abortCookie = nullptr;
    bool failedEngineClone = false;

    PrintData(EngineBase* engine, Printer* printer, Vec<PRINTPAGERANGE>& ranges, Print_Advanced_Data& advData,
              int rotation = 0, Vec<SelectionOnPage>* sel = nullptr) {
        this->printer = printer;
        this->advData = advData;
        this->rotation = rotation;
        // prefer an independent engine for the print thread: Clone() and (as a
        // fallback) re-loading from the file both re-read the document from disk.
        this->engine = engine->Clone();
        if (!this->engine) {
            // re-create engine from file to avoid sharing with render cache
            Str path = engine->FilePath();
            logf("PrintData: engine->Clone() failed for '%s', re-creating\n", path ? path : StrL("(null)"));
            if (path) {
                this->engine = CreateEngineFromFile(path, nullptr, false);
            }
        }
        if (!this->engine) {
            // Clone and reload both failed - almost always because the file was
            // moved or deleted after opening (issue #5790). The document is still
            // loaded in memory, so print from the original engine directly.
            // EngineMupdf is safe to use from the print thread (Ctx() hands out a
            // per-thread fz_context) and printing only reads, so sharing it with
            // the UI thread is fine. Before this, printing failed silently.
            logf("PrintData: clone/reload failed, printing from the original in-memory engine\n");
            this->engine = engine;
            this->engine->AddRef();
        }
        if (!this->engine) {
            logf("PrintData: failed to create engine for printing\n");
            this->failedEngineClone = true;
        }

        if (!sel) {
            this->ranges = ranges;
        } else {
            this->sel = *sel;
        }
    }

    ~PrintData() {
        delete printer;
        SafeEngineRelease(&engine);
    }
};

void Printer::SetDevMode(DEVMODEW* dm) {
    free((void*)devMode);
    devMode = dm;
}

Printer::~Printer() {
    str::Free(name);
    str::Free(output);
    str::Free(docName);
    free((void*)devMode);
    free((void*)papers);
    free((void*)paperSizes);
    free((void*)bins);
}

static void AppendPrinterAttributes(str::Builder& out, DWORD attr) {
    struct {
        DWORD flag;
        Str name;
    } flags[] = {
        {PRINTER_ATTRIBUTE_QUEUED, StrL("QUEUED")},
        {PRINTER_ATTRIBUTE_DIRECT, StrL("DIRECT")},
        {PRINTER_ATTRIBUTE_DEFAULT, StrL("DEFAULT")},
        {PRINTER_ATTRIBUTE_SHARED, StrL("SHARED")},
        {PRINTER_ATTRIBUTE_NETWORK, StrL("NETWORK")},
        {PRINTER_ATTRIBUTE_HIDDEN, StrL("HIDDEN")},
        {PRINTER_ATTRIBUTE_LOCAL, StrL("LOCAL")},
        {PRINTER_ATTRIBUTE_ENABLE_DEVQ, StrL("ENABLE_DEVQ")},
        {PRINTER_ATTRIBUTE_KEEPPRINTEDJOBS, StrL("KEEPPRINTEDJOBS")},
        {PRINTER_ATTRIBUTE_DO_COMPLETE_FIRST, StrL("DO_COMPLETE_FIRST")},
        {PRINTER_ATTRIBUTE_WORK_OFFLINE, StrL("WORK_OFFLINE")},
        {PRINTER_ATTRIBUTE_ENABLE_BIDI, StrL("ENABLE_BIDI")},
        {PRINTER_ATTRIBUTE_RAW_ONLY, StrL("RAW_ONLY")},
        {PRINTER_ATTRIBUTE_PUBLISHED, StrL("PUBLISHED")},
        {PRINTER_ATTRIBUTE_FAX, StrL("FAX")},
        {PRINTER_ATTRIBUTE_TS, StrL("TS")},
    };
    for (auto& f : flags) {
        if (attr & f.flag) {
            out.Append(fmt("\n    %s", f.name));
        }
    }
}

static void AppendPrinterStatus(str::Builder& out, DWORD status) {
    struct {
        DWORD flag;
        Str name;
    } flags[] = {
        {PRINTER_STATUS_PAUSED, StrL("PAUSED")},
        {PRINTER_STATUS_ERROR, StrL("ERROR")},
        {PRINTER_STATUS_PENDING_DELETION, StrL("PENDING_DELETION")},
        {PRINTER_STATUS_PAPER_JAM, StrL("PAPER_JAM")},
        {PRINTER_STATUS_PAPER_OUT, StrL("PAPER_OUT")},
        {PRINTER_STATUS_MANUAL_FEED, StrL("MANUAL_FEED")},
        {PRINTER_STATUS_PAPER_PROBLEM, StrL("PAPER_PROBLEM")},
        {PRINTER_STATUS_OFFLINE, StrL("OFFLINE")},
        {PRINTER_STATUS_IO_ACTIVE, StrL("IO_ACTIVE")},
        {PRINTER_STATUS_BUSY, StrL("BUSY")},
        {PRINTER_STATUS_PRINTING, StrL("PRINTING")},
        {PRINTER_STATUS_OUTPUT_BIN_FULL, StrL("OUTPUT_BIN_FULL")},
        {PRINTER_STATUS_NOT_AVAILABLE, StrL("NOT_AVAILABLE")},
        {PRINTER_STATUS_WAITING, StrL("WAITING")},
        {PRINTER_STATUS_PROCESSING, StrL("PROCESSING")},
        {PRINTER_STATUS_INITIALIZING, StrL("INITIALIZING")},
        {PRINTER_STATUS_WARMING_UP, StrL("WARMING_UP")},
        {PRINTER_STATUS_TONER_LOW, StrL("TONER_LOW")},
        {PRINTER_STATUS_NO_TONER, StrL("NO_TONER")},
        {PRINTER_STATUS_PAGE_PUNT, StrL("PAGE_PUNT")},
        {PRINTER_STATUS_USER_INTERVENTION, StrL("USER_INTERVENTION")},
        {PRINTER_STATUS_OUT_OF_MEMORY, StrL("OUT_OF_MEMORY")},
        {PRINTER_STATUS_DOOR_OPEN, StrL("DOOR_OPEN")},
        {PRINTER_STATUS_SERVER_UNKNOWN, StrL("SERVER_UNKNOWN")},
        {PRINTER_STATUS_POWER_SAVE, StrL("POWER_SAVE")},
    };
    bool any = false;
    for (auto& f : flags) {
        if (status & f.flag) {
            out.Append(fmt("\n    %s", f.name));
            any = true;
        }
    }
    if (!any) {
        out.Append(StrL("\n    READY"));
    }
}

static void AppendDeviceCapabilities(str::Builder& out, const WCHAR* nameW, const WCHAR* portW) {
    // paper bins
    DWORD bins = DeviceCapabilitiesW(nameW, portW, DC_BINS, nullptr, nullptr);
    DWORD binNames = DeviceCapabilitiesW(nameW, portW, DC_BINNAMES, nullptr, nullptr);
    ReportIf(bins != binNames);
    if (0 == bins) {
        out.Append(StrL("  no paper bins available\n"));
    } else if (bins == (DWORD)-1) {
        out.Append(fmt("  error: call to DeviceCapabilities failed with error %#x\n", GetLastError()));
    } else {
        WORD* binValues = AllocArrayTemp<WORD>((int)bins);
        DeviceCapabilitiesW(nameW, portW, DC_BINS, (WCHAR*)binValues, nullptr);
        WCHAR* binNameValues = AllocArrayTemp<WCHAR>(24 * (int)binNames);
        DeviceCapabilitiesW(nameW, portW, DC_BINNAMES, binNameValues, nullptr);
        for (DWORD j = 0; j < bins; j++) {
            TempStr s = ToUtf8Temp(WStr(binNameValues + (24 * (size_t)j)));
            out.Append(fmt("  bin %d: '%s' (%d)\n", (int)j, s, binValues[j]));
        }
    }

    // paper sizes
    DWORD papers = DeviceCapabilitiesW(nameW, portW, DC_PAPERS, nullptr, nullptr);
    DWORD paperNames = DeviceCapabilitiesW(nameW, portW, DC_PAPERNAMES, nullptr, nullptr);
    if (papers > 0 && papers != (DWORD)-1) {
        WORD* paperValues = AllocArrayTemp<WORD>((int)papers);
        DeviceCapabilitiesW(nameW, portW, DC_PAPERS, (WCHAR*)paperValues, nullptr);
        // paper names are 64 WCHARs each
        WCHAR* paperNameValues = AllocArrayTemp<WCHAR>(64 * (int)paperNames);
        DeviceCapabilitiesW(nameW, portW, DC_PAPERNAMES, paperNameValues, nullptr);
        // paper sizes in tenths of a millimeter
        POINT* paperSizes = AllocArrayTemp<POINT>((int)papers);
        DeviceCapabilitiesW(nameW, portW, DC_PAPERSIZE, (WCHAR*)paperSizes, nullptr);
        out.Append(StrL("  paper sizes:\n"));
        for (DWORD j = 0; j < papers; j++) {
            TempStr s = ToUtf8Temp(WStr(paperNameValues + (64 * (size_t)j)));
            POINT sz = paperSizes[j];
            out.Append(fmt("    '%s' (id %d, %.1f x %.1f mm)\n", s, paperValues[j], sz.x / 10.0, sz.y / 10.0));
        }
    }

    // min/max custom paper size (dimensions packed in return value: LOWORD=width, HIWORD=height)
    DWORD minRes = DeviceCapabilitiesW(nameW, portW, DC_MINEXTENT, nullptr, nullptr);
    if (minRes != (DWORD)-1) {
        DWORD maxRes = DeviceCapabilitiesW(nameW, portW, DC_MAXEXTENT, nullptr, nullptr);
        int minW = LOWORD(minRes), minH = HIWORD(minRes);
        int maxW = LOWORD(maxRes), maxH = HIWORD(maxRes);
        out.Append(fmt("  custom paper size range: %.1f x %.1f mm to %.1f x %.1f mm\n", minW / 10.0, minH / 10.0,
                       maxW / 10.0, maxH / 10.0));
    }

    // duplex
    DWORD duplex = DeviceCapabilitiesW(nameW, portW, DC_DUPLEX, nullptr, nullptr);
    out.Append(fmt("  duplex: %s\n", Str(duplex == 1 ? "yes" : "no")));

    // color
    DWORD color = DeviceCapabilitiesW(nameW, portW, DC_COLORDEVICE, nullptr, nullptr);
    out.Append(fmt("  color: %s\n", Str(color == 1 ? "yes" : "no")));

    // copies
    DWORD copies = DeviceCapabilitiesW(nameW, portW, DC_COPIES, nullptr, nullptr);
    if (copies != (DWORD)-1) {
        out.Append(fmt("  max copies: %d\n", (int)copies));
    }

    // collate
    DWORD collate = DeviceCapabilitiesW(nameW, portW, DC_COLLATE, nullptr, nullptr);
    out.Append(fmt("  collation: %s\n", Str(collate == 1 ? "yes" : "no")));

    // orientation
    DWORD orient = DeviceCapabilitiesW(nameW, portW, DC_ORIENTATION, nullptr, nullptr);
    if (orient != (DWORD)-1 && orient != 0) {
        out.Append(fmt("  landscape rotation: %d degrees\n", (int)orient));
    }

    // resolutions
    DWORD nRes = DeviceCapabilitiesW(nameW, portW, DC_ENUMRESOLUTIONS, nullptr, nullptr);
    if (nRes > 0 && nRes != (DWORD)-1) {
        LONG* resPairs = AllocArrayTemp<LONG>(2 * (int)nRes);
        DeviceCapabilitiesW(nameW, portW, DC_ENUMRESOLUTIONS, (WCHAR*)resPairs, nullptr);
        out.Append(StrL("  resolutions:"));
        for (DWORD j = 0; j < nRes; j++) {
            LONG xDpi = resPairs[(size_t)j * 2];
            LONG yDpi = resPairs[(j * 2) + 1];
            out.Append(fmt(" %dx%d", (int)xDpi, (int)yDpi));
        }
        out.Append(StrL("\n"));
    }

    // N-up (pages per sheet)
    DWORD nup = DeviceCapabilitiesW(nameW, portW, DC_NUP, nullptr, nullptr);
    if (nup > 0 && nup != (DWORD)-1) {
        DWORD* nupValues = AllocArrayTemp<DWORD>((int)nup);
        DeviceCapabilitiesW(nameW, portW, DC_NUP, (WCHAR*)nupValues, nullptr);
        out.Append(StrL("  pages per sheet (N-up):"));
        for (DWORD j = 0; j < nup; j++) {
            out.Append(fmt(" %d", (int)nupValues[j]));
        }
        out.Append(StrL("\n"));
    }

    // media types
    DWORD nMedia = DeviceCapabilitiesW(nameW, portW, DC_MEDIATYPENAMES, nullptr, nullptr);
    if (nMedia > 0 && nMedia != (DWORD)-1) {
        // media type names are 64 WCHARs each
        WCHAR* mediaNames = AllocArrayTemp<WCHAR>(64 * (int)nMedia);
        DeviceCapabilitiesW(nameW, portW, DC_MEDIATYPENAMES, mediaNames, nullptr);
        DWORD* mediaValues = AllocArrayTemp<DWORD>((int)nMedia);
        DeviceCapabilitiesW(nameW, portW, DC_MEDIATYPES, (WCHAR*)mediaValues, nullptr);
        out.Append(StrL("  media types:\n"));
        for (DWORD j = 0; j < nMedia; j++) {
            TempStr s = ToUtf8Temp(WStr(mediaNames + (64 * (size_t)j)));
            out.Append(fmt("    '%s' (%d)\n", s, (int)mediaValues[j]));
        }
    }
}

static void AppendDevModeInfo(str::Builder& out, DEVMODEW* dm) {
    if (!dm) {
        return;
    }
    out.Append(StrL("  devmode defaults:\n"));
    if (dm->dmFields & DM_ORIENTATION) {
        Str s = dm->dmOrientation == DMORIENT_PORTRAIT ? StrL("portrait") : StrL("landscape");
        out.Append(fmt("    orientation: %s\n", s));
    }
    if (dm->dmFields & DM_PAPERSIZE) {
        out.Append(fmt("    paper size id: %d\n", (int)dm->dmPaperSize));
    }
    if (dm->dmFields & DM_PAPERLENGTH) {
        out.Append(fmt("    paper length: %.1f mm\n", dm->dmPaperLength / 10.0));
    }
    if (dm->dmFields & DM_PAPERWIDTH) {
        out.Append(fmt("    paper width: %.1f mm\n", dm->dmPaperWidth / 10.0));
    }
    if (dm->dmFields & DM_COPIES) {
        out.Append(fmt("    copies: %d\n", (int)dm->dmCopies));
    }
    if (dm->dmFields & DM_PRINTQUALITY) {
        out.Append(fmt("    print quality: %d dpi\n", (int)dm->dmPrintQuality));
    }
    if (dm->dmFields & DM_YRESOLUTION) {
        out.Append(fmt("    y resolution: %d dpi\n", (int)dm->dmYResolution));
    }
    if (dm->dmFields & DM_COLOR) {
        Str s = dm->dmColor == DMCOLOR_COLOR ? StrL("color") : StrL("monochrome");
        out.Append(fmt("    color: %s\n", s));
    }
    if (dm->dmFields & DM_DUPLEX) {
        Str s = StrL("unknown");
        if (dm->dmDuplex == DMDUP_SIMPLEX) {
            s = StrL("simplex");
        } else if (dm->dmDuplex == DMDUP_HORIZONTAL) {
            s = StrL("horizontal");
        } else if (dm->dmDuplex == DMDUP_VERTICAL) {
            s = StrL("vertical");
        }
        out.Append(fmt("    duplex: %s\n", s));
    }
    if (dm->dmFields & DM_COLLATE) {
        out.Append(fmt("    collate: %s\n", Str(dm->dmCollate == DMCOLLATE_TRUE ? "yes" : "no")));
    }
}

void GetPrintersInfo(str::Builder& out) {
    PRINTER_INFO_2* info2Arr = nullptr;
    DWORD bufSize = 0;
    DWORD printersCount = 0;
    DWORD flags = PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS;
    BOOL ok = EnumPrintersW(flags, nullptr, 2, nullptr, 0, &bufSize, &printersCount);
    if (ok != 0 || GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        info2Arr = (PRINTER_INFO_2*)calloc(bufSize, 1);
        if (info2Arr != nullptr) {
            ok = EnumPrintersW(flags, nullptr, 2, (LPBYTE)info2Arr, bufSize, &bufSize, &printersCount);
        }
    }
    if (ok == 0 || !info2Arr) {
        out.Append(fmt("Call to EnumPrinters failed with error %#x", GetLastError()));
        free(info2Arr);
        return;
    }
    TempStr defName = GetDefaultPrinterNameTemp();
    out.Append(fmt("Default printer: \"%s\"\n", defName));
    for (DWORD i = 0; i < printersCount; i++) {
        PRINTER_INFO_2& info = info2Arr[i];
        const WCHAR* nameW = info.pPrinterName;
        const WCHAR* portW = info.pPortName;
        DWORD attr = info.Attributes;
        TempStr name = ToUtf8Temp(nameW);
        TempStr port = ToUtf8Temp(portW);
        out.Append(fmt("Printer: \"%s\"\n", name));
        out.Append(fmt("  port: %s\n", port));

        if (info.pDriverName) {
            out.Append(fmt("  driver: %s\n", ToUtf8Temp(info.pDriverName)));
        }
        if (info.pShareName && info.pShareName[0]) {
            out.Append(fmt("  share name: %s\n", ToUtf8Temp(info.pShareName)));
        }
        if (info.pComment && info.pComment[0]) {
            out.Append(fmt("  comment: %s\n", ToUtf8Temp(info.pComment)));
        }
        if (info.pLocation && info.pLocation[0]) {
            out.Append(fmt("  location: %s\n", ToUtf8Temp(info.pLocation)));
        }
        if (info.pPrintProcessor) {
            out.Append(fmt("  print processor: %s\n", ToUtf8Temp(info.pPrintProcessor)));
        }
        if (info.pDatatype) {
            out.Append(fmt("  datatype: %s\n", ToUtf8Temp(info.pDatatype)));
        }

        out.Append(fmt("  queued jobs: %d\n", (int)info.cJobs));

        out.Append(fmt("  status: %#x", info.Status));
        AppendPrinterStatus(out, info.Status);
        out.Append(StrL("\n"));

        out.Append(fmt("  attributes: %#x", attr));
        AppendPrinterAttributes(out, attr);
        out.Append(StrL("\n"));

        AppendDevModeInfo(out, info.pDevMode);
        AppendDeviceCapabilities(out, nameW, portW);
        out.Append(StrL("\n"));
    }
    free(info2Arr);
}

// get all the important info about a printer
Printer* NewPrinter(Str printerName) {
    HANDLE hPrinter = nullptr;
    LONG ret = 0;
    Printer* printer = nullptr;
    WCHAR* printerNameW = CWStrTemp(printerName);
    BOOL ok = OpenPrinterW(printerNameW, &hPrinter, nullptr);
    if (!ok) {
        return nullptr;
    }

    LONG structSize = 0;
    LPDEVMODE devMode = nullptr;

    DWORD needed = 0;
    GetPrinterW(hPrinter, 2, nullptr, 0, &needed);
    PRINTER_INFO_2* info = (PRINTER_INFO_2*)AllocArray<BYTE>((int)needed);
    if (info) {
        ok = GetPrinterW(hPrinter, 2, (LPBYTE)info, needed, &needed);
    }
    if (!ok || !info || needed <= sizeof(PRINTER_INFO_2)) {
        goto Exit;
    }

    /* ask for the size of DEVMODE struct */
    structSize = DocumentPropertiesW(nullptr, hPrinter, printerNameW, nullptr, nullptr, 0);
    if (structSize < sizeof(DEVMODEW)) {
        goto Exit;
    }
    devMode = (DEVMODEW*)AllocZero(nullptr, structSize);

    // Get the default DevMode for the printer and modify it for your needs.
    ret = DocumentPropertiesW(nullptr, hPrinter, printerNameW, devMode, nullptr, DM_OUT_BUFFER);
    if (IDOK != ret) {
        goto Exit;
    }

    printer = new Printer();
    printer->name = str::Dup(printerName);
    printer->devMode = devMode;
    printer->info = info;

    {
        DWORD n = DeviceCapabilitiesW(printerNameW, nullptr, DC_PAPERS, nullptr, nullptr);
        DWORD n2 = DeviceCapabilitiesW(printerNameW, nullptr, DC_PAPERNAMES, nullptr, nullptr);
        DWORD n3 = DeviceCapabilitiesW(printerNameW, nullptr, DC_PAPERSIZE, nullptr, nullptr);
        if (n != n2 || n != n3 || 0 == n || ((DWORD)-1 == n)) {
            delete printer;
            return nullptr;
        }
        printer->nPaperSizes = (int)n;
        int paperNameSize = 64;
        printer->papers = AllocArray<WORD>((int)n);
        WCHAR* paperNamesSeq = AllocArrayTemp<WCHAR>((paperNameSize * (int)n) + 1);
        printer->paperSizes = AllocArray<POINT>((int)n);

        DeviceCapabilitiesW(printerNameW, nullptr, DC_PAPERS, (WCHAR*)printer->papers, nullptr);
        DeviceCapabilitiesW(printerNameW, nullptr, DC_PAPERNAMES, paperNamesSeq, nullptr);
        DeviceCapabilitiesW(printerNameW, nullptr, DC_PAPERSIZE, (WCHAR*)printer->paperSizes, nullptr);

        for (int i = 0; i < (int)n; i++) {
            TempStr name = ToUtf8Temp(WStr(paperNamesSeq + ((size_t)i * paperNameSize)));
            printer->paperNames.Append(name);
        }
    }

    {
        DWORD n = DeviceCapabilitiesW(printerNameW, nullptr, DC_BINS, nullptr, nullptr);
        DWORD n2 = DeviceCapabilitiesW(printerNameW, nullptr, DC_BINNAMES, nullptr, nullptr);
        if (n != n2 || ((DWORD)-1 == n)) {
            delete printer;
            return nullptr;
        }
        printer->nBins = (int)n;
        // it's ok for nBins to be 0, it means there's only one, default bin
        if (n > 0) {
            int binNameSize = 24;
            printer->bins = AllocArray<WORD>((int)n);
            WCHAR* binNamesSeq = AllocArrayTemp<WCHAR>((binNameSize * (int)n) + 1);
            DeviceCapabilitiesW(printerNameW, nullptr, DC_BINS, (WCHAR*)printer->bins, nullptr);
            DeviceCapabilitiesW(printerNameW, nullptr, DC_BINNAMES, binNamesSeq, nullptr);
            for (int i = 0; i < (int)n; i++) {
                TempStr name = ToUtf8Temp(WStr(binNamesSeq + ((size_t)i * binNameSize)));
                printer->binNames.Append(name);
            }
        }
    }

    {
        DWORD n;

        n = DeviceCapabilitiesW(printerNameW, nullptr, DC_COLLATE, nullptr, nullptr);
        printer->canCallate = (n > 0);

        n = DeviceCapabilitiesW(printerNameW, nullptr, DC_COLORDEVICE, nullptr, nullptr);
        printer->isColor = (n > 0);

        n = DeviceCapabilitiesW(printerNameW, nullptr, DC_DUPLEX, nullptr, nullptr);
        printer->isDuplex = (n > 0);

        n = DeviceCapabilitiesW(printerNameW, nullptr, DC_STAPLE, nullptr, nullptr);
        printer->canStaple = (n > 0);

        n = DeviceCapabilitiesW(printerNameW, nullptr, DC_ORIENTATION, nullptr, nullptr);
        printer->orientation = (int)n;
    }

Exit:
    ClosePrinter(hPrinter);
    return printer;
}

static void SetCustomPaperSize(Printer* printer, SizeF size) {
    auto* devMode = printer->devMode;
    devMode->dmPaperSize = 0;
    devMode->dmPaperWidth = (short)size.dx;
    devMode->dmPaperLength = (short)size.dy;
    devMode->dmFields |= DM_PAPERSIZE | DM_PAPERWIDTH | DM_PAPERLENGTH;
}

// Make sure dy > dx i.e. it's tall not wide
static Size NormalizePaperSize(Size s) {
    if (s.dy > s.dx) {
        return {s.dx, s.dy};
    }
    return {s.dy, s.dx};
}

static void MessageBoxWarningCond(bool show, Str msg, Str title) {
    logf("%s: %s\n", title, msg);
    if (!show) {
        return;
    }
    MessageBoxWarning(nullptr, msg, title);
}

static RectF BoundSelectionOnPage(const Vec<SelectionOnPage>& sel, int pageNo) {
    RectF bounds;
    for (int i = 0; i < len(sel); i++) {
        if (sel[i].pageNo == pageNo) {
            bounds = bounds.Union(sel[i].rect);
        }
    }
    return bounds;
}

static short GetPaperSize(EngineBase* engine, int pageNo = 1);
static void SetDevModePaperSizeForPage(DEVMODEW* devMode, EngineBase* engine, int pageNo);

// Positive finite zoom only. Virtual printers (e.g. Foxit PDF) sometimes return
// zero LOGPIXELS / HORZRES / PHYSICAL*, which produced zoom <= 0 and tripped
// EngineImages::TransformPoint ReportIf (debug report 8c15f2fd1000001).
static bool IsValidPrintZoom(float zoom) {
    return zoom > 0 && isfinite(zoom);
}

static float SafePrintDiv(float num, float den) {
    if (!(den > 0) || !isfinite(den) || !isfinite(num)) {
        return 0;
    }
    float q = num / den;
    return isfinite(q) ? q : 0;
}

// If zoom is invalid, log printer geometry once and fall back to dpiFactor or 1.
static float SanitizePrintZoom(float zoom, float fallback, Str why, Size paperSize, Rect printable, float dpiX,
                               float dpiY, float fileDPI, float dpiFactor, Str printerName) {
    if (IsValidPrintZoom(zoom)) {
        return zoom;
    }
    logf(
        "PrintToDevice: invalid zoom=%g (%s); paper=%dx%d printable=(%d,%d %dx%d) "
        "LOGPIXELS=%g/%g fileDPI=%g dpiFactor=%g printer='%s'\n",
        zoom, why, paperSize.dx, paperSize.dy, printable.x, printable.y, printable.dx, printable.dy, dpiX, dpiY,
        fileDPI, dpiFactor, printerName);
    if (IsValidPrintZoom(fallback)) {
        return fallback;
    }
    return 1.f;
}

bool CalculatePrintPageLayout(EngineBase& engine, int pageNo, const Print_Advanced_Data& advanced, Size paperSize,
                              Rect printable, float dpiX, float dpiY, bool printPortrait, Str printerName,
                              PrintPageLayout& layout) {
    float fileDPI = engine.GetFileDPI();
    if (!(fileDPI > 0) || !isfinite(fileDPI)) {
        fileDPI = 96.f;
    }
    float dpiFactor = std::min(SafePrintDiv(dpiX, fileDPI), SafePrintDiv(dpiY, fileDPI));
    if (!IsValidPrintZoom(dpiFactor)) {
        dpiFactor = 1.f;
    }

    SizeF pageSize = engine.PageMediabox(pageNo).Size();
    int rotation = 0;
    if (advanced.autoRotate && pageSize.dx > pageSize.dy) {
        rotation += 90;
        std::swap(pageSize.dx, pageSize.dy);
    }
    rotation = (rotation % 180) == 0 ? 0 : 270;
    if (!printPortrait) {
        rotation = (rotation + 90) % 360;
        std::swap(pageSize.dx, pageSize.dy);
    }
    if (advanced.extraRotation != 0) {
        rotation = (rotation + advanced.extraRotation) % 360;
        if (advanced.extraRotation == 90 || advanced.extraRotation == 270) {
            std::swap(pageSize.dx, pageSize.dy);
        }
    }

    float zoom = dpiFactor;
    Point offset(-printable.x, -printable.y);
    float pageDx = pageSize.dx > 0 ? pageSize.dx : 1.f;
    float pageDy = pageSize.dy > 0 ? pageSize.dy : 1.f;
    Rect stretch;
    bool isStretch = advanced.scale == PrintScaleAdv::Stretch;

    if (isStretch) {
        zoom = std::max(SafePrintDiv((float)printable.dx, pageDx), SafePrintDiv((float)printable.dy, pageDy));
        offset = Point(0, 0);
        stretch = Rect(0, 0, printable.dx, printable.dy);
    } else if (advanced.scale != PrintScaleAdv::None) {
        RectF rect = engine.PageContentBox(pageNo, RenderTarget::Print);
        if (rect.IsEmpty() || rect.dx <= 0 || rect.dy <= 0) {
            rect = engine.PageMediabox(pageNo);
        }
        RectF contentBox = engine.Transform(rect, pageNo, 1.0, rotation);
        float contentDx = contentBox.dx > 0 ? contentBox.dx : pageDx;
        float contentDy = contentBox.dy > 0 ? contentBox.dy : pageDy;
        zoom = std::min(
            SafePrintDiv((float)printable.dx, contentDx),
            std::min(SafePrintDiv((float)printable.dy, contentDy),
                     std::min(SafePrintDiv((float)paperSize.dx, pageDx), SafePrintDiv((float)paperSize.dy, pageDy))));
        if (advanced.scale == PrintScaleAdv::Shrink && dpiFactor < zoom) {
            zoom = dpiFactor;
        }
        offset.x += (int)((float)paperSize.dx - (pageSize.dx * zoom)) / 2;
        offset.y += (int)((float)paperSize.dy - (pageSize.dy * zoom)) / 2;
        RectF onPaper((float)printable.x + (float)offset.x + (contentBox.x * zoom),
                      (float)printable.y + (float)offset.y + (contentBox.y * zoom), contentBox.dx * zoom,
                      contentBox.dy * zoom);
        if (onPaper.x < (float)printable.x) {
            offset.x += (int)((float)printable.x - onPaper.x);
        } else if (onPaper.BR().x > (float)printable.BR().x) {
            offset.x -= (int)(onPaper.BR().x - (float)printable.BR().x);
        }
        if (onPaper.y < (float)printable.y) {
            offset.y += (int)((float)printable.y - onPaper.y);
        } else if (onPaper.BR().y > (float)printable.BR().y) {
            offset.y -= (int)(onPaper.BR().y - (float)printable.BR().y);
        }
    }

    zoom = SanitizePrintZoom(zoom, dpiFactor, StrL("page"), paperSize, printable, dpiX, dpiY, fileDPI, dpiFactor,
                             printerName);
    if (advanced.centerHorizontally && advanced.scale == PrintScaleAdv::None) {
        offset.x += (int)((float)paperSize.dx - (pageSize.dx * zoom)) / 2;
    }

    layout.zoom = zoom;
    layout.rotation = rotation;
    layout.offset = offset;
    layout.stretch = stretch;
    layout.isStretch = isStretch;
    return true;
}

// Rasterize a page (or, for selections, a page-space sub-rectangle of it) onto
// the printer HDC in horizontal device-pixel bands. Banding bounds peak memory
// regardless of page size / printer DPI, replacing the old "shrink to half
// resolution if the whole-page bitmap can't be allocated" fallback that quietly
// degraded print quality. `pageRectFull` is the page-space area to print (the
// page mediabox, or a selection rectangle). When `stretchTo` is null the content
// is placed 1:1 at `offset`; otherwise it is scaled to fill `*stretchTo`.
// Returns true if at least one band was rendered and blitted.
static bool PrintPageInBands(EngineBase& engine, HDC hdc, int pageNo, float zoom, int rotation,
                             const RectF& pageRectFull, Point offset, const Rect* stretchTo, RenderTarget target,
                             AbortCookieManager* abortCookie, const ProgressUpdateCb& progressCb) {
    if (!IsValidPrintZoom(zoom)) {
        logf("PrintPageInBands: rejecting zoom=%g page=%d\n", zoom, pageNo);
        return false;
    }
    RectF devFull = engine.Transform(pageRectFull, pageNo, zoom, rotation);
    int fullW = (int)lroundf(devFull.dx);
    int fullH = (int)lroundf(devFull.dy);
    if (fullW <= 0 || fullH <= 0) {
        return false;
    }
    Rect devTarget = stretchTo ? *stretchTo : Rect(offset.x, offset.y, fullW, fullH);

    // cap peak bitmap memory per band (RGBA pixels); 16 MB keeps memory small
    // while keeping the band/blit count low for normal pages
    const i64 kMaxBandBytes = 16LL * 1024 * 1024;
    int bandH = (int)std::max((i64)1, kMaxBandBytes / ((i64)fullW * 4));
    bandH = std::min(bandH, fullH);

    bool anyOk = false;
    int dy0 = 0;
    while (dy0 < fullH) {
        if (WasCanceled(progressCb)) {
            break;
        }
        int h = std::min(bandH, fullH - dy0);
        // the page-space rectangle whose rendering produces exactly this band of
        // device rows; the inverse transform handles rotation (a horizontal
        // device band maps to a page strip along whatever axis matches)
        RectF devBand(devFull.x, devFull.y + (float)dy0, devFull.dx, (float)h);
        RectF pageBand = engine.Transform(devBand, pageNo, zoom, rotation, /* inverse */ true);

        RenderPageArgs args(pageNo, zoom, rotation, &pageBand, target);
        if (abortCookie) {
            args.cookie_out = &abortCookie->cookie;
        }
        Pixmap* bmp = engine.RenderPage(args);
        if (abortCookie) {
            abortCookie->Clear();
        }
        if (!bmp || !bmp->hbmp) {
            FreePixmap(bmp);
            // couldn't allocate even a band: try thinner bands before giving up,
            // so we still print at full resolution (never the old whole-page shrink)
            if (bandH > 1) {
                bandH = std::max(1, bandH / 2);
                continue; // retry the same dy0 with a thinner band
            }
            break;
        }

        // place this band: device rows [dy0, dy0+h) of the full [0, fullH) output,
        // mapped into devTarget (1:1 when not stretching). Compute the bottom edge
        // the same way so adjacent bands meet with no seam.
        Rect rc;
        rc.x = devTarget.x;
        rc.dx = devTarget.dx;
        rc.y = devTarget.y + (int)((i64)dy0 * devTarget.dy / fullH);
        int yBot = devTarget.y + (int)((i64)(dy0 + h) * devTarget.dy / fullH);
        rc.dy = yBot - rc.y;
        if (BlitPixmap(bmp, hdc, rc)) {
            anyOk = true;
        }
        FreePixmap(bmp);
        dy0 += h;
    }
    return anyOk;
}

static bool PrintToDevice(const PrintData& pd) {
    ReportIf(!pd.engine);
    if (!pd.engine) {
        logf("PrintToDevice: !pd.engine\n");
        return false;
    }
    ReportIf(!pd.printer);
    if (!pd.printer) {
        logf("PrintToDevice: !pd.printer\n");
        return false;
    }

    logf("PrintToDevice: printer: '%s', file: '%s'\n", pd.printer->name, pd.engine->FilePath());
    auto progressCb = pd.progressCb;
    auto* abortCookie = pd.abortCookie;
    int res;

    EngineBase& engine = *pd.engine;

    pd.engine->AddRef();
    defer {
        pd.engine->Release();
    };

    DOCINFOW di{};
    di.cbSize = sizeof(DOCINFO);
    if (pd.printer->docName) {
        di.lpszDocName = CWStrTemp(pd.printer->docName);
    } else if (gPluginMode) {
        TempStr fileName = url::GetFileNameTemp(gPluginURL);
        // fall back to a generic "filename" instead of the more confusing temporary filename
        if (!fileName) {
            fileName = StrL("filename");
        }
        di.lpszDocName = CWStrTemp(fileName);
    } else {
        // use just the file name (not the full path) as the print job name:
        // other apps do the same, and some printer drivers/spoolers choke on
        // long or non-ASCII full paths as the job name (issue #2166)
        di.lpszDocName = CWStrTemp(path::GetBaseNameTemp(engine.FilePath()));
    }
    if (pd.printer->output) {
        di.lpszOutput = CWStrTemp(pd.printer->output);
    }

    int current = 1, total = 0;
    if (len(pd.sel) == 0) {
        for (int i = 0; i < len(pd.ranges); i++) {
            if (pd.ranges[i].nToPage < pd.ranges[i].nFromPage) {
                total += (int)pd.ranges[i].nFromPage - (int)pd.ranges[i].nToPage + 1;
            } else {
                total += (int)pd.ranges[i].nToPage - (int)pd.ranges[i].nFromPage + 1;
            }
        }
    } else {
        for (int pageNo = 1; pageNo <= engine.PageCount(); pageNo++) {
            if (!BoundSelectionOnPage(pd.sel, pageNo).IsEmpty()) {
                total++;
            }
        }
    }
    ReportIf(total <= 0);
    if (0 == total) {
        logf("PrintToDevice: total == 0\n");
        return false;
    }

    UpdateProgress(progressCb, current, total);

    auto* devMode = pd.printer->devMode;
    // http://blogs.msdn.com/b/oldnewthing/archive/2012/11/09/10367057.aspx
    WCHAR* printerName = CWStrTemp(pd.printer->name);

    {
        // validate printer settings as per
        // https://docs.microsoft.com/en-us/windows/win32/printdocs/documentproperties
        // TODO: maybe do this in NewPrinter?
        DWORD mode = DM_IN_BUFFER | DM_OUT_BUFFER;
        HANDLE hPrinter = nullptr;
        BOOL ok = OpenPrinterW(printerName, &hPrinter, nullptr);
        if (ok && hPrinter) {
            DocumentPropertiesW(nullptr, hPrinter, printerName, devMode, devMode, mode);
        }
        ClosePrinter(hPrinter);
    }

    AutoDeleteDC hdc{CreateDCW(nullptr, printerName, nullptr, devMode)};
    if (!hdc) {
        logf("PrintToDevice: CreateDCW('%s') failed\n", pd.printer->name);
        return false;
    }

    // for PDF Printer, this shows a file dialog to pick file name for destination PDF
    res = StartDoc(hdc, &di);
    if (res <= 0) {
        logf("PrintToDevice: StartDoc() failed with %d\n", res);
        return false;
    }

    // MM_TEXT: Each logical unit is mapped to one device pixel.
    // Positive x is to the right; positive y is down.
    SetMapMode(hdc, MM_TEXT);

    float fileDPI = engine.GetFileDPI();
    if (!(fileDPI > 0) || !isfinite(fileDPI)) {
        logf("PrintToDevice: bad fileDPI=%g, using 96\n", fileDPI);
        fileDPI = 96.f;
    }
    // paper geometry; recomputed per page when printing mixed page sizes (#533)
    Size paperSize;
    Rect printable;
    float dpiFactor = 1.0f;
    float logPixelsX = 96.f;
    float logPixelsY = 96.f;
    bool bPrintPortrait = false;
    auto computeGeometry = [&] {
        int physW = GetDeviceCaps(hdc, PHYSICALWIDTH);
        int physH = GetDeviceCaps(hdc, PHYSICALHEIGHT);
        int offX = GetDeviceCaps(hdc, PHYSICALOFFSETX);
        int offY = GetDeviceCaps(hdc, PHYSICALOFFSETY);
        int horz = GetDeviceCaps(hdc, HORZRES);
        int vert = GetDeviceCaps(hdc, VERTRES);
        logPixelsX = (float)GetDeviceCaps(hdc, LOGPIXELSX);
        logPixelsY = (float)GetDeviceCaps(hdc, LOGPIXELSY);

        paperSize = Size(physW, physH);
        printable = Rect(offX, offY, horz, vert);

        bool capsOk = paperSize.dx > 0 && paperSize.dy > 0 && printable.dx > 0 && printable.dy > 0 && logPixelsX > 0 &&
                      logPixelsY > 0 && isfinite(logPixelsX) && isfinite(logPixelsY);
        if (!capsOk) {
            // Virtual PDF printers sometimes return zeros for paper/printable/DPI
            // (debug report 8c15f2fd1000001: Foxit Reader PDF Printer + CBR).
            logf(
                "PrintToDevice: bad printer caps PHYSICAL=%dx%d printable=(%d,%d %dx%d) "
                "LOGPIXELS=%g/%g printer='%s' — applying fallbacks\n",
                physW, physH, offX, offY, horz, vert, logPixelsX, logPixelsY, pd.printer->name);
            if (!(logPixelsX > 0) || !isfinite(logPixelsX)) {
                logPixelsX = 96.f;
            }
            if (!(logPixelsY > 0) || !isfinite(logPixelsY)) {
                logPixelsY = 96.f;
            }
            if (paperSize.dx <= 0) {
                paperSize.dx = (int)(8.5f * logPixelsX); // Letter-ish
            }
            if (paperSize.dy <= 0) {
                paperSize.dy = (int)(11.f * logPixelsY);
            }
            if (printable.dx <= 0) {
                printable.dx = paperSize.dx;
            }
            if (printable.dy <= 0) {
                printable.dy = paperSize.dy;
            }
        }

        dpiFactor = std::min(logPixelsX / fileDPI, logPixelsY / fileDPI);
        if (!IsValidPrintZoom(dpiFactor)) {
            logf("PrintToDevice: bad dpiFactor=%g (LOGPIXELS=%g/%g fileDPI=%g), using 1\n", dpiFactor, logPixelsX,
                 logPixelsY, fileDPI);
            dpiFactor = 1.f;
        }

        bPrintPortrait = paperSize.dx < paperSize.dy;
        if (devMode && (devMode->dmFields & DM_ORIENTATION)) {
            bPrintPortrait = DMORIENT_PORTRAIT == devMode->dmOrientation;
        }
        if (pd.advData.rotation == PrintRotationAdv::Portrait) {
            bPrintPortrait = true;
        } else if (pd.advData.rotation == PrintRotationAdv::Landscape) {
            bPrintPortrait = false;
        }
    };
    computeGeometry();

    if (len(pd.sel) > 0) {
        for (int pageNo = 1; pageNo <= engine.PageCount(); pageNo++) {
            RectF bounds = BoundSelectionOnPage(pd.sel, pageNo);
            if (bounds.IsEmpty()) {
                continue;
            }

            UpdateProgress(progressCb, current, total);

            StartPage(hdc);

            SizeF bSize = bounds.Size();
            float bdx = bSize.dx > 0 ? bSize.dx : 1.f;
            float bdy = bSize.dy > 0 ? bSize.dy : 1.f;
            float zoom = std::min(SafePrintDiv((float)printable.dx, bdx), SafePrintDiv((float)printable.dy, bdy));
            // use the correct zoom values, if the page fits otherwise
            // and the user didn't ask for anything else (default setting)
            if (PrintScaleAdv::Shrink == pd.advData.scale) {
                zoom = std::min(dpiFactor, zoom);
            } else if (PrintScaleAdv::None == pd.advData.scale) {
                zoom = dpiFactor;
            }
            zoom = SanitizePrintZoom(zoom, dpiFactor, StrL("selection"), paperSize, printable, logPixelsX, logPixelsY,
                                     fileDPI, dpiFactor, pd.printer->name);

            for (int i = 0; i < len(pd.sel); i++) {
                if (pd.sel[i].pageNo != pageNo) {
                    continue;
                }

                RectF* clipRegion = &pd.sel[i].rect;
                Point offset((int)((clipRegion->x - bounds.x) * zoom), (int)((clipRegion->y - bounds.y) * zoom));
                if (pd.advData.scale != PrintScaleAdv::None) {
                    // center the selection on the physical paper
                    offset.x += (int)((float)printable.dx - (bSize.dx * zoom)) / 2;
                    offset.y += (int)((float)printable.dy - (bSize.dy * zoom)) / 2;
                }

                PrintPageInBands(engine, hdc, pd.sel[i].pageNo, zoom, pd.rotation, *clipRegion, offset, nullptr,
                                 RenderTarget::Print, abortCookie, progressCb);
            }
            // TODO: abort if !ok?

            res = EndPage(hdc);
            bool wasCanceled = WasCanceled(progressCb);
            if (res <= 0 || wasCanceled) {
                logf("PrintToDevice: EndPage() failed with %d or wasCanceled: %d\n", res, (int)wasCanceled);
                AbortDoc(hdc);
                return false;
            }
            current++;
        }

        res = EndDoc(hdc);
        if (res <= 0) {
            logf("PrintToDevice: EndDoc() failed with %d\n", res);
            return false;
        }
        return true;
    }

    // print all the pages the user requested
    for (int i = 0; i < len(pd.ranges); i++) {
        int dir = pd.ranges[i].nFromPage > pd.ranges[i].nToPage ? -1 : 1;
        for (DWORD pageNo = pd.ranges[i].nFromPage; pageNo != pd.ranges[i].nToPage + dir; pageNo += dir) {
            if ((PrintRangeAdv::Even == pd.advData.range && pageNo % 2 != 0) ||
                (PrintRangeAdv::Odd == pd.advData.range && pageNo % 2 == 0)) {
                continue;
            }
            UpdateProgress(progressCb, current, total);

            // for mixed page size documents, set the paper size to match this
            // page before printing it, so each page goes to the right paper/tray
            // (issue #533). ResetDC must be called outside of StartPage/EndPage.
            if (pd.advData.perPagePaperSize && devMode) {
                SetDevModePaperSizeForPage(devMode, &engine, (int)pageNo);
                ResetDCW(hdc, devMode);
                computeGeometry();
            }

            res = StartPage(hdc);
            if (res <= 0) {
                logf("PrintToDevice: StartPage() failed with %d\n", res);
                continue;
            }

            PrintPageLayout layout;
            CalculatePrintPageLayout(engine, (int)pageNo, pd.advData, paperSize, printable, logPixelsX, logPixelsY,
                                     bPrintPortrait, pd.printer->name, layout);
            RectF mediabox = engine.PageMediabox((int)pageNo);
            if (layout.isStretch) {
                PrintPageInBands(engine, hdc, (int)pageNo, layout.zoom, layout.rotation, mediabox, Point(0, 0),
                                 &layout.stretch, RenderTarget::Print, abortCookie, progressCb);
            } else {
                PrintPageInBands(engine, hdc, (int)pageNo, layout.zoom, layout.rotation, mediabox, layout.offset,
                                 nullptr, RenderTarget::Print, abortCookie, progressCb);
            }

            res = EndPage(hdc);
            bool wasCanceled = WasCanceled(progressCb);
            if (res <= 0 || wasCanceled) {
                logf("PrintToDevice: EndPage() failed with %d or wasCanceled: %d\n", res, (int)wasCanceled);
                AbortDoc(hdc);
                return false;
            }
            current++;
        }
    }

    res = EndDoc(hdc);
    if (res <= 0) {
        logf("PrintToDevice: EndDoc() failed with %d\n", res);
        return false;
    }
    logf("PrintToDevice: finished ok\n");
    return true;
}

struct UpdatePrintProgressData {
    NotificationWnd* wnd;
    int current;
    int total;
};

static void UpdatePrintProgress(UpdatePrintProgressData* d) {
    int perc = CalcPerc(d->current, d->total);
    TempStr msg = fmt(_TRA("Printing page %d of %d...").s, d->current, d->total);
    UpdateNotificationProgress(d->wnd, msg, perc);
    delete d;
}

class PrintThreadData {
  public:
    NotificationWnd* wnd = nullptr;
    AbortCookieManager cookie;
    bool isCanceled = false;
    MainWindow* win = nullptr;

    PrintData* data = nullptr;
    ThreadHandle thread = nullptr; // close the print thread handle after execution

    // called when printing has been canceled
    void RemovePrintNotification(NotificationWnd* = nullptr) {
        isCanceled = true;
        cookie.Abort();
        if (this->wnd && IsMainWindowValid(win)) {
            RemoveNotification(this->wnd);
        }
        this->wnd = nullptr;
    }

    PrintThreadData(MainWindow* win, PrintData* data) {
        this->win = win;
        this->data = data;
        NotificationCreateArgs args;
        args.hwndParent = win->hwndCanvas;
        args.timeoutMs = 0;
        auto fn = MkMethod1<PrintThreadData, NotificationWnd*, &PrintThreadData::RemovePrintNotification>(this);
        args.onRemoved = fn;
        // don't use a groupId for this notification so that
        // multiple printing notifications could coexist between tabs
        args.groupId = nullptr;
        this->wnd = ShowNotification(args);
    }
    PrintThreadData(PrintThreadData const&) = delete;
    PrintThreadData& operator=(PrintThreadData const&) = delete;

    ~PrintThreadData() {
        SafeCloseThreadHandle(&thread);
        delete data;
        RemovePrintNotification();
    }

    void UpdateProgress(int current, int total) {
        auto* data = new UpdatePrintProgressData;
        data->wnd = wnd;
        data->current = current;
        data->total = total;
        auto fn = MkFunc0<UpdatePrintProgressData>(UpdatePrintProgress, data);
        uitask::Post(fn, nullptr);
    }

    bool WasCanceled() { return isCanceled || !IsMainWindowValidAndNotClosing(win) || win->printCanceled; }
};

struct DeletePrinterThreadData {
    MainWindow* win;
    ThreadHandle thread;
    PrintThreadData* threadData;
};

static void DeletePrinterThread(DeletePrinterThreadData* d) {
    auto* win = d->win;
    if (IsMainWindowValid(win) && d->thread == win->printThread) {
        win->printThread = nullptr;
    }
    delete d->threadData;
    delete d;
}

static void UpdatePrintProgress(PrintThreadData* ftd, ProgressUpdateData* data) {
    if (data->wasCancelled) {
        bool wasCancelled = ftd->WasCanceled();
        *data->wasCancelled = wasCancelled;
        return;
    }
    ftd->UpdateProgress(data->current, data->total);
}

static void PrintThread(PrintThreadData* ptd) {
    MainWindow* win = ptd->win;
    // wait for PrintToDeviceOnThread to return so that we
    // close the correct handle to the current printing thread
    while (!win->printThread) {
        Sleep(1);
    }

    ThreadHandle thread = ptd->thread = win->printThread;

    PrintData* pd = ptd->data;
    pd->progressCb = MkFunc1<PrintThreadData, ProgressUpdateData*>(UpdatePrintProgress, ptd);
    ;
    pd->abortCookie = &ptd->cookie;
    PrintToDevice(*pd);

    auto* data = new DeletePrinterThreadData;
    data->win = win;
    data->thread = thread;
    data->threadData = ptd;
    auto fn = MkFunc0<DeletePrinterThreadData>(DeletePrinterThread, data);
    uitask::Post(fn, "PrintDeleteThread");
    DestroyTempArena();
}

static void PrintToDeviceOnThread(MainWindow* win, PrintData* data) {
    AbortPrinting(win);
    PrintThreadData* threadData = new PrintThreadData(win, data);
    win->printThread = nullptr;
    auto fn = MkFunc0(PrintThread, threadData);
    win->printThread = StartThread(fn, StrL("PrintThread"));
}

void AbortPrinting(MainWindow* win) {
    if (win->printThread) {
        win->printCanceled = true;
        WaitForSingleObject(win->printThread, INFINITE);
    }
    win->printCanceled = false;
}

static HGLOBAL GlobalMemDup(const void* data, size_t n) {
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, n);
    if (!hGlobal) {
        return nullptr;
    }

    void* globalData = GlobalLock(hGlobal);
    if (!globalData) {
        GlobalFree(hGlobal);
        return nullptr;
    }

    memcpy(globalData, data, n);
    GlobalUnlock(hGlobal);
    return hGlobal;
}

// Build a DEVNAMES handle for the given device (printer) name. PrintDlgEx wants
// hDevMode and hDevNames to be a consistent pair; passing hDevMode alone makes
// COMDLG32 GlobalLock the missing (null) hDevNames, which is harmless normally
// but crashes under ASan's GlobalLock interceptor. The offsets are in WCHARs
// from the start of the structure; PrintDlgEx overwrites this on return, so the
// driver/output hints just need to be present, not exact.
static HGLOBAL GlobalMemDevNames(const WCHAR* device) {
    const WCHAR* driver = L"winspool";
    const WCHAR* output = L"";
    size_t headerW = sizeof(DEVNAMES) / sizeof(WCHAR);
    size_t driverLen = wcslen(driver);
    size_t deviceLen = wcslen(device);
    size_t outputLen = wcslen(output);
    size_t total = headerW + (driverLen + 1) + (deviceLen + 1) + (outputLen + 1);
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, total * sizeof(WCHAR));
    if (!hGlobal) {
        return nullptr;
    }
    auto* dn = (DEVNAMES*)GlobalLock(hGlobal);
    if (!dn) {
        GlobalFree(hGlobal);
        return nullptr;
    }
    auto* base = (WCHAR*)dn;
    size_t off = headerW;
    dn->wDriverOffset = (WORD)off;
    memcpy(base + off, driver, (driverLen + 1) * sizeof(WCHAR));
    off += driverLen + 1;
    dn->wDeviceOffset = (WORD)off;
    memcpy(base + off, device, (deviceLen + 1) * sizeof(WCHAR));
    off += deviceLen + 1;
    dn->wOutputOffset = (WORD)off;
    memcpy(base + off, output, (outputLen + 1) * sizeof(WCHAR));
    dn->wDefault = 0;
    GlobalUnlock(hGlobal);
    return hGlobal;
}

// the PrinterDefaults.Collate setting (issue #1558).
// returns 1 to force collate, 0 to force no-collate, -1 to leave the driver default
static int CollateDefaultPref() {
    Str s = gSettings->printerDefaults.collate;
    if (str::EqI(s, StrL("collate"))) {
        return 1;
    }
    if (str::EqI(s, StrL("nocollate"))) {
        return 0;
    }
    return -1;
}

// apply a collate preference (1 = collate, 0 = no-collate) to a DEVMODE handle
static void SetDevModeCollate(HGLOBAL hDevMode, int collate) {
    if (!hDevMode || collate < 0) {
        return;
    }
    DEVMODEW* dm = (DEVMODEW*)GlobalLock(hDevMode);
    if (dm) {
        dm->dmCollate = collate ? DMCOLLATE_TRUE : DMCOLLATE_FALSE;
        dm->dmFields |= DM_COLLATE;
        GlobalUnlock(hDevMode);
    }
}

// PD_USEDEVMODECOPIESANDCOLLATE makes PrintDlgEx ignore PRINTDLGEX::nCopies.
// Reset the DEVMODE value so a previous print job's copy count doesn't carry
// over to the next dialog (issue #5981).
static void SetDevModeCopies(HGLOBAL hDevMode, short copies) {
    if (!hDevMode) {
        return;
    }
    DEVMODEW* dm = (DEVMODEW*)GlobalLock(hDevMode);
    if (dm) {
        dm->dmCopies = copies;
        dm->dmFields |= DM_COPIES;
        GlobalUnlock(hDevMode);
    }
}

enum {
    MAXPAGERANGES = 10
};
void PrintCurrentFile(MainWindow* win, bool waitForCompletion) {
    // we remember some printer settings per process
    static ScopedMem<DEVMODE> defaultDevMode;
    static PrintScaleAdv defaultScaleAdv = PrintScaleAdv::Shrink;
    static bool hasDefaults = false;

    Printer* printer = nullptr;

    if (!hasDefaults) {
        hasDefaults = true;
        if (str::EqI(gSettings->printerDefaults.printScale, StrL("fit"))) {
            defaultScaleAdv = PrintScaleAdv::Fit;
        } else if (str::EqI(gSettings->printerDefaults.printScale, StrL("stretch"))) {
            defaultScaleAdv = PrintScaleAdv::Stretch;
        } else if (str::EqI(gSettings->printerDefaults.printScale, StrL("none"))) {
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
        win->AsChm()->PrintCurrentPage(true);
        return;
    }
    if (win->AsMarkdown()) {
        win->AsMarkdown()->PrintCurrentPage(true);
        return;
    }
    DisplayModel* dm = win->AsFixed();
    ReportIf(!dm);
    if (!dm) {
        return;
    }
    auto* engine = dm->GetEngine();
    EngineBase* pinnedEngine = nullptr;
    ReportIf(!engine);
    if (!engine) {
        return;
    }
    int rotation;
    int nPages = dm->PageCount();

#ifndef DISABLE_DOCUMENT_RESTRICTIONS
    if (!engine->AllowsPrinting()) {
        return;
    }
#endif

    if (!waitForCompletion && TryPrintCurrentFileWin11(win, defaultScaleAdv)) {
        return;
    }

    if (win->printThread) {
        uint type = MB_ICONEXCLAMATION | MB_YESNO | MbRtlReadingMaybe();
        Str title = _TRA("Printing in progress.");
        Str msg = _TRA("Printing is still in progress. Abort and start over?");
        int res = MsgBox(win->hwndFrame, msg, title, type);
        if (res == IDNO) {
            return;
        }
    }
    AbortPrinting(win);

    PRINTDLGEXW pdex{};
    pdex.lStructSize = sizeof(PRINTDLGEXW);
    pdex.hwndOwner = win->hwndFrame;
    pdex.Flags = PD_USEDEVMODECOPIESANDCOLLATE | PD_COLLATE;
    if (!win->CurrentTab()->selectionOnPage) {
        pdex.Flags |= PD_NOSELECTION;
    }
    pdex.nCopies = 1;
    /* by default print all pages */
    pdex.nPageRanges = 1;
    pdex.nMaxPageRanges = MAXPAGERANGES;
    PRINTPAGERANGE* ppr = AllocArray<PRINTPAGERANGE>(MAXPAGERANGES);
    pdex.lpPageRanges = ppr;
    ppr->nFromPage = 1;
    ppr->nToPage = nPages;
    pdex.nMinPage = 1;
    pdex.nMaxPage = nPages;
    pdex.nStartPage = START_PAGE_GENERAL;

    Print_Advanced_Data advanced(PrintRangeAdv::All, defaultScaleAdv);
    ScopedMem<DLGTEMPLATE> dlgTemplate; // needed for RTL languages
    HPROPSHEETPAGE hPsp = CreatePrintAdvancedPropSheet(&advanced, dlgTemplate);
    pdex.lphPropertyPages = &hPsp;
    pdex.nPropertyPages = 1;

    PrintData* pd = nullptr;
    DEVMODE* devMode = nullptr;
    // restore remembered settings
    if (defaultDevMode) {
        DEVMODE* p = defaultDevMode.Get();
        pdex.hDevMode = GlobalMemDup(p, p->dmSize + p->dmDriverExtra);
    }

    // Always hand PrintDlgEx a DEVMODE: seed one from the default printer when
    // there's no remembered one. This also seeds the Collate checkbox from the
    // PrinterDefaults.Collate setting (issue #1558). Crucially, starting from a
    // null hDevMode/hDevNames makes PrintDlgEx GlobalLock a null handle
    // internally, which crashes under ASan's GlobalLock interceptor.
    if (!pdex.hDevMode) {
        TempStr defName = GetDefaultPrinterNameTemp();
        Printer* seed = defName ? NewPrinter(defName) : nullptr;
        if (seed && seed->devMode) {
            auto* p = seed->devMode;
            pdex.hDevMode = GlobalMemDup(p, p->dmSize + p->dmDriverExtra);
        }
        delete seed;
    }

    SetDevModeCopies(pdex.hDevMode, 1);
    int collatePref = CollateDefaultPref();
    if (collatePref >= 0) {
        SetDevModeCollate(pdex.hDevMode, collatePref);
    }

    // pair the hDevMode with a matching hDevNames so the two are consistent and
    // PrintDlgEx never GlobalLocks a null handle (see the ASan note above)
    if (pdex.hDevMode && !pdex.hDevNames) {
        auto* dmp = (DEVMODE*)GlobalLock(pdex.hDevMode);
        if (dmp) {
            pdex.hDevNames = GlobalMemDevNames(dmp->dmDeviceName);
            GlobalUnlock(pdex.hDevMode);
        }
    }

    HRESULT res = PrintDlgExW(&pdex);

    // PrintDlgExW pumps messages, so the window may have been closed/destroyed while the dialog was open
    if (!IsMainWindowValidAndNotClosing(win)) {
        logf("PrintCurrentFile: window closed during PrintDlgEx\n");
        free(ppr);
        GlobalFree(pdex.hDevMode);
        GlobalFree(pdex.hDevNames);
        return;
    }

    if (res != S_OK) {
        logf("PrintCurrentFile: PrintDlgEx failed\n");
        MessageBoxWarning(win->hwndFrame, _TRA("Couldn't initialize printer"), _TRA("Printing problem."));
    }
    auto action = pdex.dwResultAction;
    if (action != PD_RESULT_PRINT) {
        // it's cancel or apply so silently ignore as it's not an error
        goto Exit;
    }

    // re-validate after modal dialog - tab/document may have changed while dialog was open
    dm = win->AsFixed();
    if (!dm) {
        goto Exit;
    }
    engine = dm->GetEngine();
    if (!engine) {
        goto Exit;
    }
    pinnedEngine = engine;
    pinnedEngine->AddRef();
    rotation = dm->GetRotation();
    nPages = dm->PageCount();

    if (!pdex.hDevNames) {
        MessageBoxWarning(win->hwndFrame, _TRA("Couldn't get printer name"), _TRA("Printing problem."));
        goto Exit;
    }

    {
        DEVNAMES* devNames = (DEVNAMES*)GlobalLock(pdex.hDevNames);
        if (devNames) {
            // printerInfo.pDriverName = (LPWSTR)devNames + devNames->wDriverOffset;
            WCHAR* printerName = (WCHAR*)devNames + devNames->wDeviceOffset;
            TempStr name = ToUtf8Temp(printerName);
            printer = NewPrinter(name);
            // printerInfo.pPortName = (LPWSTR)devNames + devNames->wOutputOffset;
            GlobalUnlock(pdex.hDevNames);
        }
    }

    if (!printer) {
        MessageBoxWarning(win->hwndFrame, _TRA("Couldn't initialize printer"), _TRA("Printing problem."));
        goto Exit;
    }

    devMode = (DEVMODEW*)GlobalLock(pdex.hDevMode);

    if (pdex.dwResultAction == PD_RESULT_PRINT || pdex.dwResultAction == PD_RESULT_APPLY) {
        // remember settings for this process
        if (devMode) {
            defaultDevMode.Set((DEVMODEW*)MemDup(nullptr, devMode, (size_t)(devMode->dmSize + devMode->dmDriverExtra)));
        }
        defaultScaleAdv = advanced.scale;
    }

    if (devMode && advanced.paperSourceByPageSize) {
        // let the printer pick the input tray whose paper matches the document's
        // page size, independent of the page-scaling option (issue #349). Applied
        // after the settings are remembered above so it doesn't stick to the next
        // print job in this session.
        devMode->dmDefaultSource = DMBIN_FORMSOURCE;
        devMode->dmFields |= DM_DEFAULTSOURCE;
    }

    if (devMode) {
        auto* dmCopy = (DEVMODEW*)MemDup(nullptr, devMode, (size_t)(devMode->dmSize + devMode->dmDriverExtra));
        printer->SetDevMode(dmCopy);
        GlobalUnlock(pdex.hDevMode);
    }

    if (pdex.dwResultAction != PD_RESULT_PRINT) {
        goto Exit;
    }

    if (pdex.Flags & PD_CURRENTPAGE) {
        PRINTPAGERANGE pr;
        if (pdex.nPageRanges == 1 && pdex.lpPageRanges[0].nFromPage == pdex.lpPageRanges[0].nToPage) {
            // Unified Print Dialog (which doesn't have a "Current page" option)
            // sets PD_CURRENTPAGE when the custom range contains one page (2 or 2-2)
            // with nFromPage and nToPage equal to the chosen page.
            // PD_PAGENUMS isn't set in that case.
            pr = pdex.lpPageRanges[0];
        } else {
            pr = {(DWORD)dm->CurrentPageNo(), (DWORD)dm->CurrentPageNo()};
        }
        ranges.Append(pr);
    } else if (win->CurrentTab()->selectionOnPage && (pdex.Flags & PD_SELECTION)) {
        printSelection = true;
    } else if (!(pdex.Flags & PD_PAGENUMS)) {
        PRINTPAGERANGE pr = {1, (DWORD)nPages};
        ranges.Append(pr);
    } else {
        ReportIf(pdex.nPageRanges <= 0);
        for (DWORD i = 0; i < pdex.nPageRanges; i++) {
            ranges.Append(pdex.lpPageRanges[i]);
        }
    }

    // re-validate engine - it may have been invalidated while message boxes were shown
    dm = win->AsFixed();
    if (!dm) {
        goto Exit;
    }
    engine = dm->GetEngine();
    if (!engine) {
        goto Exit;
    }

    sel = printSelection ? win->CurrentTab()->selectionOnPage : nullptr;
    pd = new PrintData(pinnedEngine, printer, ranges, advanced, rotation, sel);
    SafeEngineRelease(&pinnedEngine);

    if (pd->failedEngineClone) {
        logf("PrintCurrentFile: failed to create engine for printing\n");
        delete pd;
        goto Exit;
    }
    if (!waitForCompletion) {
        PrintToDeviceOnThread(win, pd);
    } else {
        PrintToDevice(*pd);
        delete pd;
    }

Exit:
    SafeEngineRelease(&pinnedEngine);
    free(ppr);
    GlobalFree(pdex.hDevNames);
    GlobalFree(pdex.hDevMode);
}

struct PaperSizeDesc {
    float minDx, maxDx;
    float minDy, maxDy;
    PaperFormat paperFormat;
};

// clang-format off
static PaperSizeDesc gPaperSizes[] = {
    // common ISO 216 formats (metric)
    {
        16.53f, 16.55f,
        23.38f, 23.40f,
        PaperFormat::A2,
    },
    {
        11.68f, 11.70f,
        16.53f, 16.55f,
        PaperFormat::A3,
    },
    {
        8.26f, 8.28f,
        11.68f, 11.70f,
        PaperFormat::A4,
    },
    {
        5.82f, 5.85f,
        8.26f, 8.28f,
        PaperFormat::A5,
    },
    {
        4.08f, 4.10f,
        5.82f, 5.85f,
        PaperFormat::A6,
    },
    // common US/ANSI formats (imperial)
    {
        8.49f, 8.51f,
        10.99f, 11.01f,
        PaperFormat::Letter,
    },
    {
        8.49f, 8.51f,
        13.99f, 14.01f,
        PaperFormat::Legal,
    },
    {
        10.99f, 11.01f,
        16.99f, 17.01f,
        PaperFormat::Tabloid,
    },
    {
        5.49f, 5.51f,
        8.49f, 8.51f,
        PaperFormat::Statement,
    }
};
// clang-format on

static bool fInRange(float x, float min, float max) {
    return x >= min && x <= max;
}

PaperFormat GetPaperFormatFromSizeApprox(SizeF size) {
    float dx = size.dx;
    float dy = size.dy;
    if (dx > dy) {
        std::swap(dx, dy);
    }
    for (const PaperSizeDesc& desc : gPaperSizes) {
        bool ok = fInRange(dx, desc.minDx, desc.maxDx) && fInRange(dy, desc.minDy, desc.maxDy);
        if (ok) {
            return desc.paperFormat;
        }
    }
    return PaperFormat::Other;
}

static short GetPaperSize(EngineBase* engine, int pageNo) {
    RectF mediabox = engine->PageMediabox(pageNo);
    SizeF size = engine->Transform(mediabox, pageNo, 1.0f / engine->GetFileDPI(), 0).Size();

    switch (GetPaperFormatFromSizeApprox(size)) {
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

// set the DEVMODE paper size to match a specific page, for mixed page size
// documents (issue #533). Uses a standard paper kind when the page matches one,
// otherwise a custom paper size (in tenths of a millimeter).
static void SetDevModePaperSizeForPage(DEVMODEW* devMode, EngineBase* engine, int pageNo) {
    short ps = GetPaperSize(engine, pageNo);
    if (ps != 0) {
        devMode->dmPaperSize = ps;
        devMode->dmFields |= DM_PAPERSIZE;
        devMode->dmFields &= ~(DM_PAPERLENGTH | DM_PAPERWIDTH);
        return;
    }
    // non-standard size: size in tenths of a millimeter. Normalize to portrait
    // (width <= length), like the standard paper kinds above, so that landscape
    // pages are handled by the auto-rotation logic rather than landscape paper.
    RectF mediabox = engine->PageMediabox(pageNo);
    SizeF size = engine->Transform(mediabox, pageNo, 254.0f / engine->GetFileDPI(), 0).Size();
    float w = size.dx, h = size.dy;
    if (w > h) {
        std::swap(w, h);
    }
    devMode->dmPaperSize = 0;
    devMode->dmPaperWidth = (short)w;
    devMode->dmPaperLength = (short)h;
    devMode->dmFields |= DM_PAPERSIZE | DM_PAPERLENGTH | DM_PAPERWIDTH;
}

static short GetStandardPaperByName(Str paperName) {
    if (str::EqI(paperName, StrL("letter"))) {
        return DMPAPER_LETTER;
    }
    if (str::EqI(paperName, StrL("legal"))) {
        return DMPAPER_LEGAL;
    }
    if (str::EqI(paperName, StrL("tabloid"))) {
        return DMPAPER_TABLOID;
    }
    if (str::EqI(paperName, StrL("statement"))) {
        return DMPAPER_STATEMENT;
    }
    if (str::EqI(paperName, StrL("A2"))) {
        return DMPAPER_A2;
    }
    if (str::EqI(paperName, StrL("A3"))) {
        return DMPAPER_A3;
    }
    if (str::EqI(paperName, StrL("A4"))) {
        return DMPAPER_A4;
    }
    if (str::EqI(paperName, StrL("A5"))) {
        return DMPAPER_A5;
    }
    if (str::EqI(paperName, StrL("A6"))) {
        return DMPAPER_A6;
    }
    return 0;
}

// wantedName can be a paper name, like "A6" or number for DMPAPER_* contstants like DMPAPER_LETTER
static short GetPaperByName(Printer* printer, Str wantedName) {
    auto* devMode = printer->devMode;

    TempStr name = str::DupTemp(wantedName);
    str::TrimWSInPlace(name, str::TrimOpt::Both);
    int nameLen = len(name);
    Str wanted = name;
    if (nameLen >= 2 && name.s[0] == '"' && name.s[nameLen - 1] == '"') {
        name.s[nameLen - 1] = '\0';
        wanted = Str(name.s + 1);
    }

    int n = printer->nPaperSizes;
    for (int i = 0; i < n; i++) {
        Str paperName = printer->paperNames[i];
        if (str::EqIS(wanted, paperName)) {
            return (short)printer->papers[i];
        }
        // e.g. "A3" matches driver names like "A3 297 x 420 mm"
        int wantedLen = len(wanted);
        if (wantedLen > 0 && str::StartsWithI(paperName, wanted)) {
            char next = paperName.s[wantedLen];
            if (next == '\0' || next == ' ') {
                return (short)printer->papers[i];
            }
        }
    }

    // alternatively allow indicating the paper directly by number
    uint paperId = 0;
    if (!str::IsNull(str::Parse(wantedName, "%u%$", &paperId))) {
        return (short)paperId;
    }

    short standard = GetStandardPaperByName(wantedName);
    if (standard != 0) {
        return standard;
    }
    return devMode->dmPaperSize;
}

static short GetPaperKind(Str kindName) {
    uint kind;
    if (!str::IsNull(str::Parse(kindName, "%u%$", &kind))) {
        return (short)kind;
    }
    return DMPAPER_USER;
}

static short GetPaperSourceByName(Printer* printer, Str binName) {
    auto* devMode = printer->devMode;
    // "auto" lets the printer pick the input tray whose paper matches the
    // document's page size (matches Adobe's "Choose paper source by PDF page
    // size"; issues #349, #534)
    if (str::EqIS(binName, StrL("auto"))) {
        return DMBIN_FORMSOURCE;
    }
    if (!(devMode->dmFields & DM_DEFAULTSOURCE)) {
        return devMode->dmDefaultSource;
    }
    int n = printer->nBins;
    if (n == 0) {
        return devMode->dmDefaultSource;
    }
    for (int i = 0; i < n; i++) {
        Str currName = printer->binNames[i];
        if (str::EqIS(currName, binName)) {
            return (short)printer->bins[i];
        }
    }
    uint count = 0;
    // alternatively allow indicating the paper bin directly by number
    if (!str::IsNull(str::Parse(binName, "%u%$", &count))) {
        return (short)count;
    }
    return devMode->dmDefaultSource;
}

// the -print-settings token that disables honoring a PDF's /ViewerPreferences
// print defaults (issue #534)
static Str kIgnorePdfPrintSettingsToken = StrL("ignore-pdf-print-settings");

static bool PrintSettingsHaveToken(Str settings, Str token) {
    if (!settings) {
        return false;
    }
    StrVec list;
    Split(&list, settings, StrL(","), true);
    for (Str s : list) {
        if (str::EqI(s, token)) {
            return true;
        }
    }
    return false;
}

// apply the print defaults from a PDF's /ViewerPreferences (issue #534) to the
// DEVMODE and advanced data. These are defaults only: any explicit value in
// -print-settings is applied afterwards and overrides them.
static void ApplyPdfViewerPrintPrefs(const PdfViewerPrintPrefs& prefs, DEVMODEW* devMode,
                                     Print_Advanced_Data& advanced) {
    if (prefs.hasPickTrayByPdfSize && prefs.pickTrayByPdfSize) {
        // PickTrayByPDFSize: let the printer pick the tray by page size
        devMode->dmDefaultSource = DMBIN_FORMSOURCE;
        devMode->dmFields |= DM_DEFAULTSOURCE;
    }
    if (prefs.hasNumCopies && prefs.numCopies >= 1) {
        short copies = (short)std::min(prefs.numCopies, 9999);
        devMode->dmCopies = copies;
        devMode->dmFields |= DM_COPIES;
    }
    if (prefs.hasDuplex) {
        if (prefs.duplex == PdfDuplexPref::Simplex) {
            devMode->dmDuplex = DMDUP_SIMPLEX;
        } else if (prefs.duplex == PdfDuplexPref::FlipShortEdge) {
            devMode->dmDuplex = DMDUP_HORIZONTAL;
        } else if (prefs.duplex == PdfDuplexPref::FlipLongEdge) {
            devMode->dmDuplex = DMDUP_VERTICAL;
        }
        devMode->dmFields |= DM_DUPLEX;
    }
    if (prefs.hasPrintScaling && prefs.printScalingNone) {
        // PrintScaling /None means print at the original size (no scaling)
        advanced.scale = PrintScaleAdv::None;
    }
}

static void ApplyPrintSettings(Printer* printer, Str settings, int pageCount, Vec<PRINTPAGERANGE>& ranges,
                               Print_Advanced_Data& advanced) {
    auto* devMode = printer->devMode;
    auto suffix = [](Str s, int n) -> Str { return Str(s.s + n, s.len - n); };

    StrVec rangeList;
    if (settings) {
        Split(&rangeList, settings, StrL(","), true);
    }

    for (Str s : rangeList) {
        int val, val2;
        PRINTPAGERANGE pr{};
        if (str::EqI(s, StrL("last"))) {
            pr.nFromPage = pr.nToPage = (DWORD)pageCount;
            ranges.Append(pr);
        } else if (!str::IsNull(str::Parse(s, "%d-%d%$", &val, &val2))) {
            int from = val;
            int to = val2;
            if (from < 0) {
                from = pageCount + from + 1;
            }
            if (to < 0) {
                to = pageCount + to + 1;
            }
            pr.nFromPage = limitValue((DWORD)from, (DWORD)1, (DWORD)pageCount);
            pr.nToPage = limitValue((DWORD)to, (DWORD)1, (DWORD)pageCount);
            ranges.Append(pr);
        } else if (!str::IsNull(str::Parse(s, "%d%$", &val))) {
            if (val < 0) {
                val = pageCount + val + 1;
            }
            pr.nFromPage = pr.nToPage = limitValue((DWORD)val, (DWORD)1, (DWORD)pageCount);
            ranges.Append(pr);
        } else if (str::EqI(s, StrL("even"))) {
            advanced.range = PrintRangeAdv::Even;
        } else if (str::EqI(s, StrL("odd"))) {
            advanced.range = PrintRangeAdv::Odd;
        } else if (str::EqI(s, StrL("noscale"))) {
            advanced.scale = PrintScaleAdv::None;
        } else if (str::EqI(s, StrL("shrink"))) {
            advanced.scale = PrintScaleAdv::Shrink;
        } else if (str::EqI(s, StrL("fit"))) {
            advanced.scale = PrintScaleAdv::Fit;
        } else if (str::EqI(s, StrL("stretch"))) {
            advanced.scale = PrintScaleAdv::Stretch;
        } else if (str::EqI(s, StrL("portrait"))) {
            advanced.rotation = PrintRotationAdv::Portrait;
            devMode->dmOrientation = DMORIENT_PORTRAIT;
            devMode->dmFields |= DM_ORIENTATION;
        } else if (str::EqI(s, StrL("landscape"))) {
            advanced.rotation = PrintRotationAdv::Landscape;
            devMode->dmOrientation = DMORIENT_LANDSCAPE;
            devMode->dmFields |= DM_ORIENTATION;
        } else if (str::EqI(s, StrL("disable-auto-rotation"))) {
            advanced.autoRotate = false;
        } else if (str::StartsWithI(s, StrL("rotate="))) {
            // extra rotation of the printout in degrees: 90, 180 or 270 (#1246)
            int deg = 0;
            if (!str::IsNull(str::Parse(suffix(s, 7), "%d%$", &deg))) {
                deg = ((deg % 360) + 360) % 360;
                if (deg == 90 || deg == 180 || deg == 270) {
                    advanced.extraRotation = deg;
                }
            }
        } else if (str::EqI(s, StrL("center"))) {
            advanced.centerHorizontally = true;
        } else if (!str::IsNull(str::Parse(s, "%dx%$", &val))) {
            if (val < 0) {
                val = 1;
            }
            if (val > 9999) {
                logf("limiting number of print copies from %d to %d\n", val, 9999);
                val = 9999;
            }
            devMode->dmCopies = (short)val;
            devMode->dmFields |= DM_COPIES;
        } else if (str::EqI(s, StrL("simplex"))) {
            devMode->dmDuplex = DMDUP_SIMPLEX;
            devMode->dmFields |= DM_DUPLEX;
        } else if (str::EqI(s, StrL("duplex")) || str::EqI(s, StrL("duplexlong"))) {
            devMode->dmDuplex = DMDUP_VERTICAL;
            devMode->dmFields |= DM_DUPLEX;
        } else if (str::EqI(s, StrL("duplexshort"))) {
            devMode->dmDuplex = DMDUP_HORIZONTAL;
            devMode->dmFields |= DM_DUPLEX;
        } else if (str::EqI(s, StrL("color"))) {
            devMode->dmColor = DMCOLOR_COLOR;
            devMode->dmFields |= DM_COLOR;
        } else if (str::EqI(s, StrL("monochrome"))) {
            devMode->dmColor = DMCOLOR_MONOCHROME;
            devMode->dmFields |= DM_COLOR;
        } else if (str::EqI(s, StrL("collate"))) {
            devMode->dmCollate = DMCOLLATE_TRUE;
            devMode->dmFields |= DM_COLLATE;
        } else if (str::EqI(s, StrL("nocollate"))) {
            devMode->dmCollate = DMCOLLATE_FALSE;
            devMode->dmFields |= DM_COLLATE;
        } else if (str::StartsWithI(s, StrL("bin="))) {
            devMode->dmDefaultSource = GetPaperSourceByName(printer, suffix(s, 4));
            devMode->dmFields |= DM_DEFAULTSOURCE;
        } else if (str::StartsWithI(s, StrL("paper="))) {
            float mmW = 0, mmH = 0;
            if (str::EqI(suffix(s, 6), StrL("auto"))) {
                // set the paper size per page from the document's page size, for
                // mixed page size documents (issue #533)
                advanced.perPagePaperSize = true;
            } else if (!str::IsNull(str::Parse(suffix(s, 6), "%fmm x %fmm%$", &mmW, &mmH)) && mmW > 0 && mmH > 0) {
                // custom paper size specified as dimensions e.g. "paper=76mm x 130mm"
                // SetCustomPaperSize expects tenths of a millimeter
                SizeF size(mmW * 10.f, mmH * 10.f);
                SetCustomPaperSize(printer, size);
            } else {
                devMode->dmPaperSize = GetPaperByName(printer, suffix(s, 6));
                devMode->dmFields |= DM_PAPERSIZE;
            }
        } else if (str::StartsWithI(s, StrL("paperkind="))) {
            // alternatively allow indicating the paper kind directly by number
            devMode->dmPaperSize = GetPaperKind(suffix(s, 10));
            devMode->dmFields |= DM_PAPERSIZE;
        } else if (str::StartsWithI(s, StrL("output="))) {
            printer->output = str::Dup(suffix(s, 7));
        } else if (str::StartsWithI(s, StrL("docname="))) {
            printer->docName = str::Dup(suffix(s, 8));
        } else if (str::EqI(s, kIgnorePdfPrintSettingsToken)) {
            // handled before ApplyPrintSettings (see PrintFile2); ignore here
        }
    }

    if (len(ranges) == 0) {
        PRINTPAGERANGE pr = {1, (DWORD)pageCount};
        ranges.Append(pr);
    }
}

static short DetectPrinterPaperSize(EngineBase* engine, Printer* printer) {
    // get size of first page in tenths of a millimeter in portrait mode
    RectF mediabox = engine->PageMediabox(1);
    SizeF size = engine->Transform(mediabox, 1, 254.0f / engine->GetFileDPI(), 0).Size();
    Size sizeP = NormalizePaperSize(Size((int)size.dx, (int)size.dy));

    int n = printer->nPaperSizes;
    auto* sizes = printer->paperSizes;
    // find equivalent paper size with 1mm tolerance
    for (int i = 0; i < n; i++) {
        POINT sz = sizes[i];
        Size pSizeP = NormalizePaperSize(Size(sz.x, sz.y));
        if (abs(sizeP.dx - pSizeP.dx) <= 10 && abs(sizeP.dy - pSizeP.dy) <= 10) {
            return (short)printer->papers[i];
        }
    }
    return 0;
}

// let the driver validate and canonicalize the devmode; returns false if the
// driver rejects it (e.g. doesn't support a custom paper size, see issue #2188)
static bool ValidateDevMode(Printer* printer) {
    WCHAR* nameW = CWStrTemp(printer->name);
    HANDLE hPrinter = nullptr;
    if (!OpenPrinterW(nameW, &hPrinter, nullptr)) {
        return false;
    }
    LONG ret =
        DocumentPropertiesW(nullptr, hPrinter, nameW, printer->devMode, printer->devMode, DM_IN_BUFFER | DM_OUT_BUFFER);
    ClosePrinter(hPrinter);
    return ret == IDOK;
}

static bool SetPrinterCustomPaperSizeForEngine(EngineBase* engine, Printer* printer) {
    // get size of first page in tenths of a millimeter
    RectF mediabox = engine->PageMediabox(1);
    SizeF size = engine->Transform(mediabox, 1, 254.0f / engine->GetFileDPI(), 0).Size();

    auto* devMode = printer->devMode;
    size_t devModeSize = devMode->dmSize + devMode->dmDriverExtra;
    char* backup = (char*)MemDup(GetTempArena(), devMode, devModeSize);
    SetCustomPaperSize(printer, size);
    if (ValidateDevMode(printer)) {
        return true;
    }
    // the driver doesn't support this custom paper size; many printers
    // (e.g. laser printers with fixed paper trays) don't and would create
    // a print job that never prints (issue #2188)
    memcpy(devMode, backup, devModeSize);
    return false;
}

PrintResult PrintFile2(EngineBase* engine, Str printerName, bool displayErrors, Str settings) {
    bool ok = false;
    Printer* printer = nullptr;

    if (!HasPermission(Perm::PrinterAccess)) {
        return PrintResult::NoPermission;
    }

#ifndef DISABLE_DOCUMENT_RESTRICTIONS
    if (engine && !engine->AllowsPrinting()) {
        MessageBoxWarningCond(displayErrors, _TRA("Cannot print this file"), _TRA("Printing problem."));
        logf("PrintFile2: printing not allowed by the document\n");
        return PrintResult::PrintingNotAllowed;
    }
#endif

    if (!engine) {
        MessageBoxWarningCond(displayErrors, _TRA("Cannot print this file"), _TRA("Printing problem."));
        logf("PrintFile2: engine is null\n");
        return PrintResult::CannotLoadFile;
    }

    logf("PrintFile2: file: '%s', printer: '%s'\n", engine->FilePath(), printerName);

    if (printerName) {
        printer = NewPrinter(printerName);
    } else {
        TempStr defName = GetDefaultPrinterNameTemp();
        if (!defName) {
            logf("PrintFile: GetDefaultPrinterName() failed\n");
            return PrintResult::PrinterNotFound;
        }
        printer = NewPrinter(defName);
    }

    if (!printer) {
        TempStr msg = fmt(_TRA("Printer '%s' doesn't exist").s, printerName);
        MessageBoxWarningCond(displayErrors, msg, _TRA("Printing problem."));
        return PrintResult::PrinterNotFound;
    }

    // set paper size to match the size of the document's first page
    // (will be overridden by any paper= value in -print-settings)
    auto* devMode = printer->devMode;
    short printerDefaultPaper = devMode->dmPaperSize;
    devMode->dmPaperSize = GetPaperSize(engine);
    {
        Print_Advanced_Data advanced;
        Vec<PRINTPAGERANGE> ranges;

        // apply print defaults from the PDF's /ViewerPreferences (issue #534),
        // unless the caller opted out. Done before ApplyPrintSettings so that an
        // explicit -print-settings value overrides the PDF's default.
        if (!PrintSettingsHaveToken(settings, kIgnorePdfPrintSettingsToken)) {
            PdfViewerPrintPrefs prefs;
            if (GetPdfViewerPrintPrefs(engine, prefs)) {
                ApplyPdfViewerPrintPrefs(prefs, devMode, advanced);
            }
        }

        ApplyPrintSettings(printer, settings, engine->PageCount(), ranges, advanced);

        if (advanced.rotation == PrintRotationAdv::Auto && devMode->dmPaperSize == 0) {
            devMode->dmPaperSize = DetectPrinterPaperSize(engine, printer);
            if (devMode->dmPaperSize) {
                devMode->dmFields |= DM_PAPERSIZE;
            } else if (!SetPrinterCustomPaperSizeForEngine(engine, printer)) {
                // can't print on paper matching the document's page size;
                // print on the printer's default paper and rely on scaling
                // (shrink by default) to fit the page (issue #2188)
                devMode->dmPaperSize = printerDefaultPaper;
            }
        }

        // takes ownership of printer
        PrintData pd(engine, printer, ranges, advanced);
        ok = PrintToDevice(pd);
        if (!ok) {
            logf("PrintToDevice: failed\n");
            MessageBoxWarningCond(displayErrors, _TRA("Couldn't initialize printer"), _TRA("Printing problem."));
        }
    }
    if (!ok) {
        return PrintResult::PrintFailed;
    }
    logf("PrintFile2: finished ok\n");
    return PrintResult::Ok;
}

PrintResult PrintFile(Str fileName, Str printerName, bool displayErrors, Str settings) {
    logf("PrintFile: file: '%s', printer: '%s'\n", fileName, printerName);
    fileName = path::NormalizeTemp(fileName);
    EngineBase* engine = CreateEngineFromFile(fileName, nullptr, true);
    if (!engine) {
        TempStr msg = fmt("Couldn't open file '%s' for printing", fileName);
        MessageBoxWarningCond(displayErrors, msg, _TRA("Error"));
        return PrintResult::CannotLoadFile;
    }
    PrintResult res = PrintFile2(engine, printerName, displayErrors, settings);
    SafeEngineRelease(&engine);
    logf("PrintFile: finished ok\n");
    return res;
}
