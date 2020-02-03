/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/CmdLineParser.h"
#include "utils/WinUtil.h"
#include "utils/Log.h"

#include "SettingsStructs.h"
#include "GlobalPrefs.h"
#include "Flags.h"
#include "StressTesting.h"
#include "SumatraConfig.h"

// order must match enum
static const char* argNames =
    "register-for-pdf\0"
    "register\0"
    "print-to-default\0"
    "print-dialog\0"
    "exit-when-done\0"
    "exit-on-print\0"
    "restrict\0"
    "invertcolors\0"
    "invert-colors\0"
    "presentation\0"
    "fullscreen\0"
    "console\0"
    "rand\0"
    "crash-on-open\0"
    "reuse-instance\0"
    "esc-to-exit\0"
    "set-color-range\0"
    "enum-printers\0"
    "print-to\0"
    "print-settings\0"
    "inverse-search\0"
    "forward-search\0"
    "fwdsearch\0"
    "nameddest\0"
    "named-dest\0"
    "page\0"
    "view\0"
    "zoom\0"
    "scroll\0"
    "appdata\0"
    "plugin\0"
    "stress-test\0"
    "n\0"
    "render\0"
    "bench\0"
    "lang\0"
    "bgcolor\0"
    "bg-color\0"
    "fwdsearch-offset\0"
    "fwdsearch-width\0"
    "fwdsearch-color\0"
    "fwdsearch-permanent\0"
    "manga-mode\0"
    "autoupdate\0"
    "extract-text\0"
    "install\0"
    "uninstall\0"
    "regress\0"
    "tester\0"
    "d\0"
    "h\0"
    "?\0"
    "help\0"
    "with-filter\0"
    "with-preview\0"
    "x\0"
    "ramicro\0"
    "ra-micro\0"
    "testapp\0"
    "s\0"
    "silent\0";

enum {
    RegisterForPdf,
    Register,
    PrintToDefault,
    PrintDialog,
    ExitWhenDone,
    ExitOnPrint,
    Restrict,
    InvertColors1,
    InvertColors2,
    Presentation,
    Fullscreen,
    Console,
    Rand,
    CrashOnOpen,
    ReuseInstance,
    EscToExit,
    SetColorRange,
    ArgEnumPrinters, // EnumPrinters conflicts with win API EnumPrinters()
    PrintTo,
    PrintSettings,
    InverseSearch,
    ForwardSearch,
    FwdSearch,
    NamedDest,
    NamedDest2,
    Page,
    View,
    Zoom,
    Scroll,
    AppData,
    Plugin,
    StressTest,
    ArgN,
    Render,
    Bench,
    Lang,
    BgColor,
    BgColor2,
    FwdSearchOffset,
    FwdSearchWidth,
    FwdSearchColor,
    FwdSearchPermanent,
    MangaMode,
    AutoUpdate,
    ExtractText,
    Install,
    Uninstall,
    Regress,
    Tester,
    InstallDir,
    Help1,
    Help2,
    Help3,
    WithFilter,
    WithPreview,
    ExtractFiles,
    RaMicro1,
    RaMicro2,
    TestApp,
    Silent2,
    Silent
};

Flags::~Flags() {
    free(printerName);
    free(printSettings);
    free(forwardSearchOrigin);
    free(destName);
    free(pluginURL);
    free(appdataDir);
    free(inverseSearchCmdLine);
    free(stressTestPath);
    free(stressTestFilter);
    free(stressTestRanges);
    free(lang);
}

static void EnumeratePrinters() {
    str::WStr output;

    PRINTER_INFO_5* info5Arr = nullptr;
    DWORD bufSize = 0, printersCount;
    bool fOk = EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, nullptr, 5, nullptr, bufSize, &bufSize,
                            &printersCount);
    if (fOk || GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        info5Arr = (PRINTER_INFO_5*)malloc(bufSize);
        fOk = EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, nullptr, 5, (LPBYTE)info5Arr, bufSize,
                           &bufSize, &printersCount);
    }
    if (!fOk || !info5Arr) {
        output.AppendFmt(L"Call to EnumPrinters failed with error %#x", GetLastError());
        MessageBox(nullptr, output.Get(), L"SumatraPDF - EnumeratePrinters", MB_OK | MB_ICONERROR);
        free(info5Arr);
        return;
    }
    AutoFreeWstr defName(GetDefaultPrinterName());
    for (DWORD i = 0; i < printersCount; i++) {
        const WCHAR* printerName = info5Arr[i].pPrinterName;
        const WCHAR* printerPort = info5Arr[i].pPortName;
        bool fDefault = str::Eq(defName, printerName);
        output.AppendFmt(L"%s (Port: %s, attributes: %#x%s)\n", printerName, printerPort, info5Arr[i].Attributes,
                         fDefault ? L", default" : L"");

        DWORD bins = DeviceCapabilities(printerName, printerPort, DC_BINS, nullptr, nullptr);
        DWORD binNames = DeviceCapabilities(printerName, printerPort, DC_BINNAMES, nullptr, nullptr);
        CrashIf(bins != binNames);
        if (0 == bins) {
            output.Append(L" - no paper bins available\n");
        } else if (bins == (DWORD)-1) {
            output.AppendFmt(L" - Call to DeviceCapabilities failed with error %#x\n", GetLastError());
        } else {
            ScopedMem<WORD> binValues(AllocArray<WORD>(bins));
            DeviceCapabilities(printerName, printerPort, DC_BINS, (WCHAR*)binValues.Get(), nullptr);
            AutoFreeWstr binNameValues(AllocArray<WCHAR>(24 * binNames));
            DeviceCapabilities(printerName, printerPort, DC_BINNAMES, binNameValues.Get(), nullptr);
            for (DWORD j = 0; j < bins; j++) {
                output.AppendFmt(L" - '%s' (%d)\n", binNameValues.Get() + 24 * j, binValues.Get()[j]);
            }
        }
    }
    free(info5Arr);
    MessageBox(nullptr, output.Get(), L"SumatraPDF - EnumeratePrinters", MB_OK | MB_ICONINFORMATION);
}

// parses a list of page ranges such as 1,3-5,7- (i..e all but pages 2 and 6)
// into an interable list (returns nullptr on parsing errors)
// caller must delete the result
bool ParsePageRanges(const WCHAR* ranges, Vec<PageRange>& result) {
    if (!ranges)
        return false;

    WStrVec rangeList;
    rangeList.Split(ranges, L",", true);
    rangeList.SortNatural();

    for (size_t i = 0; i < rangeList.size(); i++) {
        int start, end;
        if (str::Parse(rangeList.at(i), L"%d-%d%$", &start, &end) && 0 < start && start <= end)
            result.Append(PageRange{start, end});
        else if (str::Parse(rangeList.at(i), L"%d-%$", &start) && 0 < start)
            result.Append(PageRange{start, INT_MAX});
        else if (str::Parse(rangeList.at(i), L"%d%$", &start) && 0 < start)
            result.Append(PageRange{start, start});
        else
            return false;
    }

    return result.size() > 0;
}

// a valid page range is a non-empty, comma separated list of either
// single page ("3") numbers, closed intervals "2-4" or intervals
// unlimited to the right ("5-")
bool IsValidPageRange(const WCHAR* ranges) {
    Vec<PageRange> rangeList;
    return ParsePageRanges(ranges, rangeList);
}

// <s> can be:
// * "loadonly"
// * description of page ranges e.g. "1", "1-5", "2-3,6,8-10"
bool IsBenchPagesInfo(const WCHAR* s) {
    return str::EqI(s, L"loadonly") || IsValidPageRange(s);
}

// -view [continuous][singlepage|facing|bookview]
static void ParseViewMode(DisplayMode* mode, const WCHAR* txt) {
    *mode = prefs::conv::ToDisplayMode(txt, DM_AUTOMATIC);
}

static const char* zoomValues =
    "fit page\0fitpage\0fit-page\0fit width\0fitwidth\0fit-width\0fit "
    "content\0fitcontent\0fit-content\0";

// -zoom [fitwidth|fitpage|fitcontent|n]
// if a number, it's in percent e.g. 12.5 means 12.5%
// 100 means 100% i.e. actual size as e.g. given in PDF file
static void ParseZoomValue(float* zoom, const WCHAR* txtOrig) {
    AutoFree txtDup(strconv::WstrToUtf8(txtOrig));
    char* txt = str::ToLowerInPlace(txtDup.Get());
    int zoomVal = seqstrings::StrToIdx(zoomValues, txt);
    if (zoomVal != -1) {
        // 0-2 : fit page
        // 3-5 : fit width
        // 6-8 : fit content
        *zoom = ZOOM_FIT_CONTENT;
        if (zoomVal <= 5) {
            *zoom = ZOOM_FIT_WIDTH;
        }
        if (zoomVal <= 2) {
            *zoom = ZOOM_FIT_PAGE;
        }
        return;
    }
    // remove trailing % in place, if exists
    if (str::EndsWith(txt, "%")) {
        txt[str::Len(txt) - 1] = 0;
    }
    str::Parse(txt, "%f", zoom);
    // prevent really small zoom and zoom values that are not valid numbers
    // (which would be parsed as 0)
    if (*zoom < 1.f)
        *zoom = ZOOM_ACTUAL_SIZE;
}

// -scroll x,y
static void ParseScrollValue(PointI* scroll, const WCHAR* txt) {
    int x, y;
    if (str::Parse(txt, L"%d,%d%$", &x, &y))
        *scroll = PointI(x, y);
}

static int GetArgNo(const WCHAR* argName) {
    if (*argName == '-' || *argName == '/') {
        argName++;
    } else {
        return -1;
    }
    AutoFreeWstr nameLowerCase = str::ToLower(argName);
    return seqstrings::StrToIdx(argNames, nameLowerCase);
}

/* parse argument list. we assume that all unrecognized arguments are file names. */
void ParseCommandLine(const WCHAR* cmdLine, Flags& i) {
    WStrVec argList;
    ParseCmdLine(cmdLine, argList);
    size_t argCount = argList.size();

#define is_arg_with_param(_argNo) (param && _argNo == arg)
#define additional_param() argList.at(n + 1)
#define has_additional_param() ((argCount > n + 1) && ('-' != additional_param()[0]))
#define handle_string_param(name) name = str::Dup(argList.at(++n))
#define handle_int_param(name) name = _wtoi(argList.at(++n))

    for (size_t n = 1; n < argCount; n++) {
        WCHAR* argName = argList.at(n);
        int arg = GetArgNo(argName);
        WCHAR* param = nullptr;
        if (argCount > n + 1) {
            param = argList.at(n + 1);
        }
        if (RegisterForPdf == arg || Register == arg) {
            i.registerAsDefault = true;
            i.exitImmediately = true;
            return;
        } else if (Silent == arg || Silent2 == arg) {
            // silences errors happening during -print-to and -print-to-default
            i.silent = true;
        } else if (PrintToDefault == arg) {
            i.printerName = GetDefaultPrinterName();
            if (!i.printerName) {
                i.printDialog = true;
            }
            i.exitWhenDone = true;
        } else if (is_arg_with_param(PrintTo)) {
            handle_string_param(i.printerName);
            i.exitWhenDone = true;
        } else if (PrintDialog == arg) {
            i.printDialog = true;
        } else if (Help1 == arg || Help2 == arg || Help3 == arg) {
            i.showHelp = true;
        } else if (is_arg_with_param(PrintSettings)) {
            // argument is a comma separated list of page ranges and
            // advanced options [even|odd], [noscale|shrink|fit] and [autorotation|portrait|landscape]
            // e.g. -print-settings "1-3,5,10-8,odd,fit"
            handle_string_param(i.printSettings);
            str::RemoveChars(i.printSettings, L" ");
            str::TransChars(i.printSettings, L";", L",");
        } else if (ExitWhenDone == arg || ExitOnPrint == arg) {
            // only affects -print-dialog (-print-to and -print-to-default
            // always exit on print) and -stress-test (useful for profiling)
            i.exitWhenDone = true;
        } else if (is_arg_with_param(InverseSearch)) {
            i.inverseSearchCmdLine = str::Dup(argList.at(++n));
        } else if ((is_arg_with_param(ForwardSearch) || is_arg_with_param(FwdSearch)) && argCount > n + 2) {
            // -forward-search is for consistency with -inverse-search
            // -fwdsearch is for consistency with -fwdsearch-*
            handle_string_param(i.forwardSearchOrigin);
            handle_int_param(i.forwardSearchLine);
        } else if (is_arg_with_param(NamedDest) || is_arg_with_param(NamedDest2)) {
            // -nameddest is for backwards compat (was used pre-1.3)
            // -named-dest is for consistency
            handle_string_param(i.destName);
        } else if (is_arg_with_param(Page)) {
            handle_int_param(i.pageNumber);
        } else if (Restrict == arg) {
            i.restrictedUse = true;
        } else if (RaMicro1 == arg || RaMicro2 == arg) {
            i.ramicro = true;
        } else if (InvertColors1 == arg || InvertColors2 == arg) {
            // -invertcolors is for backwards compat (was used pre-1.3)
            // -invert-colors is for consistency
            // -invert-colors used to be a shortcut for -set-color-range 0xFFFFFF 0x000000
            // now it non-permanently swaps textColor and backgroundColor
            i.invertColors = true;
        } else if (Presentation == arg) {
            i.enterPresentation = true;
        } else if (Fullscreen == arg) {
            i.enterFullScreen = true;
        } else if (is_arg_with_param(View)) {
            ParseViewMode(&i.startView, param);
            ++n;
        } else if (is_arg_with_param(Zoom)) {
            ParseZoomValue(&i.startZoom, param);
            ++n;
        } else if (is_arg_with_param(Scroll)) {
            ParseScrollValue(&i.startScroll, param);
            ++n;
        } else if (Console == arg) {
            i.showConsole = true;
        } else if (is_arg_with_param(AppData)) {
            i.appdataDir = str::Dup(param);
            ++n;
        } else if (is_arg_with_param(Plugin)) {
            // -plugin [<URL>] <parent HWND>
            if (argCount > n + 2 && !str::IsDigit(*argList.at(n + 1)) && *argList.at(n + 2) != '-')
                handle_string_param(i.pluginURL);
            // the argument is a (numeric) window handle to
            // become the parent of a frameless SumatraPDF
            // (used e.g. for embedding it into a browser plugin)
            i.hwndPluginParent = (HWND)(INT_PTR)_wtol(argList.at(++n));
        } else if (is_arg_with_param(StressTest)) {
            // -stress-test <file or dir path> [<file filter>] [<page/file range(s)>] [<cycle
            // count>x]
            // e.g. -stress-test file.pdf 25x  for rendering file.pdf 25 times
            //      -stress-test file.pdf 1-3  render only pages 1, 2 and 3 of file.pdf
            //      -stress-test dir 301- 2x   render all files in dir twice, skipping first 300
            //      -stress-test dir *.pdf;*.xps  render all files in dir that are either PDF or XPS
            handle_string_param(i.stressTestPath);
            int num;
            if (has_additional_param() && str::FindChar(additional_param(), '*'))
                handle_string_param(i.stressTestFilter);
            if (has_additional_param() && IsValidPageRange(additional_param()))
                handle_string_param(i.stressTestRanges);
            if (has_additional_param() && str::Parse(additional_param(), L"%dx%$", &num) && num > 0) {
                i.stressTestCycles = num;
                n++;
            }
        } else if (is_arg_with_param(ArgN)) {
            handle_int_param(i.stressParallelCount);
        } else if (is_arg_with_param(Render)) {
            handle_int_param(i.pageNumber);
            i.testRenderPage = true;
        } else if (is_arg_with_param(ExtractText)) {
            handle_int_param(i.pageNumber);
            i.testExtractPage = true;
        } else if (Install == arg) {
            i.install = true;
        } else if (Uninstall == arg) {
            i.uninstall = true;
        } else if (WithFilter == arg) {
            i.withFilter = true;
        } else if (WithPreview == arg) {
            i.withPreview = true;
        } else if (Rand == arg) {
            i.stressRandomizeFiles = true;
        } else if (Regress == arg) {
            i.regress = true;
        } else if (AutoUpdate == arg) {
            i.autoUpdate = true;
        } else if (ExtractFiles == arg) {
            i.justExtractFiles = true;
            // silently extract files to the current directory
            // (if /d isn't used)
            i.silent = true;
            if (!i.installDir) {
                i.installDir = str::Dup(L".");
            }
        } else if (Tester == arg) {
            i.tester = true;
        } else if (TestApp == arg) {
            i.testApp = true;
        } else if (is_arg_with_param(Bench)) {
            WCHAR* s = str::Dup(param);
            ++n;
            i.pathsToBenchmark.Push(s);
            s = nullptr;
            if (has_additional_param() && IsBenchPagesInfo(additional_param())) {
                s = str::Dup(argList.at(++n));
            }
            i.pathsToBenchmark.Push(s);
            i.exitImmediately = true;
        } else if (CrashOnOpen == arg) {
            // to make testing of crash reporting system in pre-release/release
            // builds possible
            i.crashOnOpen = true;
        } else if (ReuseInstance == arg) {
            // for backwards compatibility, -reuse-instance reuses whatever
            // instance has registered as DDE server
            i.reuseDdeInstance = true;
        } else if (is_arg_with_param(InstallDir)) {
            i.installDir = str::Dup(param);
            ++n;
        } else if (is_arg_with_param(Lang)) {
            // TODO: remove the following deprecated options within
            // a release or two
            auto tmp = strconv::WstrToUtf8(param);
            i.lang = (char*)tmp.data();
            ++n;
        } else if (EscToExit == arg) {
            i.globalPrefArgs.Append(str::Dup(argList.at(n)));
        } else if (is_arg_with_param(BgColor) || is_arg_with_param(BgColor2) || is_arg_with_param(FwdSearchOffset) ||
                   is_arg_with_param(FwdSearchWidth) || is_arg_with_param(FwdSearchColor) ||
                   is_arg_with_param(FwdSearchPermanent) || is_arg_with_param(MangaMode)) {
            i.globalPrefArgs.Append(str::Dup(argList.at(n)));
            i.globalPrefArgs.Append(str::Dup(argList.at(++n)));
        } else if (SetColorRange == arg && argCount > n + 2) {
            i.globalPrefArgs.Append(str::Dup(argList.at(n)));
            i.globalPrefArgs.Append(str::Dup(argList.at(++n)));
            i.globalPrefArgs.Append(str::Dup(argList.at(++n)));
        } else if (gIsDebugBuild && ArgEnumPrinters == arg) {
            EnumeratePrinters();
            /* this is for testing only, exit immediately */
            i.exitImmediately = true;
            return;
        }
        // this should have been handled already by AutoUpdateMain
        else if (is_arg_with_param(AutoUpdate)) {
            n++;
        } else {
            // Remember this argument as a filename to open
            WCHAR* filePath = nullptr;
            if (str::EndsWithI(argName, L".lnk"))
                filePath = ResolveLnk(argName);
            if (!filePath)
                filePath = str::Dup(argName);
            i.fileNames.Push(filePath);
        }
    }
}
