/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/CmdLineParser.h"
#include "utils/WinUtil.h"
#include "utils/Log.h"

#include "DisplayMode.h"
#include "SettingsStructs.h"
#include "GlobalPrefs.h"
#include "Flags.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "WindowInfo.h"
#include "StressTesting.h"
#include "SumatraConfig.h"

Flags::~Flags() {
    str::Free(printerName);
    str::Free(printSettings);
    str::Free(forwardSearchOrigin);
    str::Free(destName);
    str::Free(pluginURL);
    str::Free(appdataDir);
    str::Free(inverseSearchCmdLine);
    str::Free(stressTestPath);
    str::Free(stressTestFilter);
    str::Free(stressTestRanges);
    str::Free(lang);
    str::Free(copySelfToPath);
    str::Free(deleteFilePath);
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
            AutoFreeWstr binNameValues(AllocArray<WCHAR>(24 * (size_t)binNames));
            DeviceCapabilities(printerName, printerPort, DC_BINNAMES, binNameValues.Get(), nullptr);
            for (DWORD j = 0; j < bins; j++) {
                output.AppendFmt(L" - '%s' (%d)\n", binNameValues.Get() + 24 * (size_t)j, binValues.Get()[j]);
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
    if (!ranges) {
        return false;
    }

    WStrVec rangeList;
    rangeList.Split(ranges, L",", true);
    rangeList.SortNatural();

    for (size_t i = 0; i < rangeList.size(); i++) {
        int start, end;
        if (str::Parse(rangeList.at(i), L"%d-%d%$", &start, &end) && 0 < start && start <= end) {
            result.Append(PageRange{start, end});
        } else if (str::Parse(rangeList.at(i), L"%d-%$", &start) && 0 < start) {
            result.Append(PageRange{start, INT_MAX});
        } else if (str::Parse(rangeList.at(i), L"%d%$", &start) && 0 < start) {
            result.Append(PageRange{start, start});
        } else {
            return false;
        }
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
    auto s = ToUtf8Temp(txt);
    *mode = DisplayModeFromString(s.Get(), DisplayMode::Automatic);
}

static const char* zoomValues =
    "fit page\0fitpage\0fit-page\0fit width\0fitwidth\0fit-width\0fit "
    "content\0fitcontent\0fit-content\0";

// -zoom [fitwidth|fitpage|fitcontent|n]
// if a number, it's in percent e.g. 12.5 means 12.5%
// 100 means 100% i.e. actual size as e.g. given in PDF file
static void ParseZoomValue(float* zoom, const WCHAR* txtOrig) {
    auto txtDup = ToUtf8Temp(txtOrig);
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
    if (*zoom < 1.f) {
        *zoom = ZOOM_ACTUAL_SIZE;
    }
}

// -scroll x,y
static void ParseScrollValue(Point* scroll, const WCHAR* txt) {
    int x, y;
    if (str::Parse(txt, L"%d,%d%$", &x, &y)) {
        *scroll = Point(x, y);
    }
}

// order of args matters: first must be from the user, second is from our code
// fromCmdLine must start with "-" or "/".
// Ours shouldn't have the prefix
static bool IsArgEq(WCHAR* fromCmdLine, const WCHAR* compareAgainst) {
    WCHAR c = *fromCmdLine;
    if (c == L'-' || c == L'/') {
        ++fromCmdLine;
    } else {
        return false;
    }
    return str::EqI(fromCmdLine, compareAgainst);
}

// TODO: work in progress
struct CmdLineArgsIter {
    WStrVec& args;
    int curr{0};
    int nArgs{0};

    CmdLineArgsIter(WStrVec& v);
    const WCHAR* Next();
    const WCHAR* GetParam();
    int GetIntParam();
    int MaxParams() const;
    bool HasParam() const;
};

CmdLineArgsIter::CmdLineArgsIter(WStrVec& v) : args(v) {
    nArgs = args.isize();
}

const WCHAR* CmdLineArgsIter::Next() {
    if (curr >= nArgs) {
        return nullptr;
    }
    const WCHAR* res = args[curr];
    curr++;
    return res;
}

const WCHAR* CmdLineArgsIter::GetParam() {
    return Next();
}

int CmdLineArgsIter::GetIntParam() {
    const WCHAR* s = GetParam();
    CrashIf(!s);
    if (!s) {
        return 0;
    }
    return _wtoi(s);
}

int CmdLineArgsIter::MaxParams() const {
    return nArgs - curr;
}

bool CmdLineArgsIter::HasParam() const {
    return curr < nArgs;
}

/* parse argument list. we assume that all unrecognized arguments are file names. */
void ParseCommandLine(const WCHAR* cmdLine, Flags& i) {
    WStrVec args;
    // TODO: use CommandLineToArgvW() instead of ParseCmdLine
    ParseCmdLine(cmdLine, args);
    size_t argCount = args.size();

#define additional_param() args.at(n + 1)
#define has_additional_param() ((argCount > n + 1) && ('-' != additional_param()[0]))
#define eatStringParam(name) name = str::Dup(args.at(++n))
#define eatIntParam(name) name = _wtoi(args.at(++n))
#define isArg(__name) IsArgEq(argName, __name)

    WCHAR* param = nullptr;
    WCHAR* argName;
    for (size_t n = 1; n < argCount; n++) {
        argName = args.at(n);
        param = nullptr;
        if (argCount > n + 1) {
            param = args.at(n + 1);
        }
        if (isArg(L"register-for-pdf") || isArg(L"register")) {
            i.registerAsDefault = true;
            i.exitImmediately = true;
            return;
        }
        if (isArg(L"s") || isArg(L"silent")) {
            // silences errors happening during -print-to and -print-to-default
            i.silent = true;
            continue;
        }
        if (isArg(L"print-to-default")) {
            i.printerName = GetDefaultPrinterName();
            if (!i.printerName) {
                i.printDialog = true;
            }
            i.exitWhenDone = true;
            continue;
        }
        if (isArg(L"print-dialog")) {
            i.printDialog = true;
            continue;
        }
        if (isArg(L"h") || isArg(L"?") || isArg(L"help")) {
            i.showHelp = true;
            continue;
        }
        if (isArg(L"exit-when-done") || isArg(L"exit-on-print")) {
            // only affects -print-dialog (-print-to and -print-to-default
            // always exit on print) and -stress-test (useful for profiling)
            i.exitWhenDone = true;
            continue;
        }
        if (isArg(L"restrict")) {
            i.restrictedUse = true;
            continue;
        }
        if (isArg(L"presentation")) {
            i.enterPresentation = true;
            continue;
        }
        if (isArg(L"fullscreen")) {
            i.enterFullScreen = true;
            continue;
        }
        if (isArg(L"invertcolors") || isArg(L"invert-colors")) {
            // -invertcolors is for backwards compat (was used pre-1.3)
            // -invert-colors is for consistency
            // -invert-colors used to be a shortcut for -set-color-range 0xFFFFFF 0x000000
            // now it non-permanently swaps textColor and backgroundColor
            i.invertColors = true;
            continue;
        }
        if (isArg(L"console")) {
            i.showConsole = true;
            continue;
        }
        if (isArg(L"install")) {
            i.install = true;
            continue;
        }
        if (isArg(L"uninstall")) {
            i.uninstall = true;
            continue;
        }
        if (isArg(L"with-filter")) {
            i.withFilter = true;
            continue;
        }
        if (isArg(L"with-preview")) {
            i.withPreview = true;
            continue;
        }
        if (isArg(L"rand")) {
            i.stressRandomizeFiles = true;
            continue;
        }
        if (isArg(L"regress")) {
            i.regress = true;
            continue;
        }
        if (isArg(L"autoupdate")) {
            i.autoUpdate = true;
            continue;
        }
        if (isArg(L"x")) {
            // silently extract files to directory given if /d
            // or current directory if no /d given
            i.justExtractFiles = true;
            i.silent = true;
            continue;
        }
        if (isArg(L"tester")) {
            i.tester = true;
            continue;
        }
        if (isArg(L"testapp")) {
            i.testApp = true;
            continue;
        }
        if (isArg(L"new-window")) {
            i.inNewWindow = true;
            continue;
        }
        if (isArg(L"log")) {
            i.log = true;
            continue;
        }
        if (isArg(L"crash-on-open")) {
            // to make testing of crash reporting system in pre-release/release
            // builds possible
            i.crashOnOpen = true;
            continue;
        }
        if (isArg(L"reuse-instance")) {
            // for backwards compatibility, -reuse-instance reuses whatever
            // instance has registered as DDE server
            i.reuseDdeInstance = true;
            continue;
        }
        if (isArg(L"esc-to-exit")) {
            i.globalPrefArgs.Append(str::Dup(args.at(n)));
            continue;
        }
        if (gIsDebugBuild && isArg(L"enum-printers")) {
            EnumeratePrinters();
            /* this is for testing only, exit immediately */
            i.exitImmediately = true;
            return;
        }
        if (isArg(L"test-auto-update")) {
            i.testAutoUpdate = true;
            continue;
        }

        // follwing args require at least one param
        // if no params here, assume this is a file
        if (!param) {
            goto CollectFile;
        }

        if (isArg(L"print-to")) {
            eatStringParam(i.printerName);
            i.exitWhenDone = true;
            continue;
        }
        if (isArg(L"print-settings")) {
            // argument is a comma separated list of page ranges and
            // advanced options [even|odd], [noscale|shrink|fit] and [autorotation|portrait|landscape]
            // e.g. -print-settings "1-3,5,10-8,odd,fit"
            eatStringParam(i.printSettings);
            str::RemoveCharsInPlace(i.printSettings, L" ");
            str::TransCharsInPlace(i.printSettings, L";", L",");
            continue;
        }
        if (isArg(L"inverse-search")) {
            i.inverseSearchCmdLine = str::Dup(args.at(++n));
            continue;
        }
        if ((isArg(L"forward-search") || isArg(L"fwdsearch")) && argCount > n + 2) {
            // -forward-search is for consistency with -inverse-search
            // -fwdsearch is for consistency with -fwdsearch-*
            eatStringParam(i.forwardSearchOrigin);
            eatIntParam(i.forwardSearchLine);
            continue;
        }
        if (isArg(L"nameddest") || isArg(L"named-dest")) {
            // -nameddest is for backwards compat (was used pre-1.3)
            // -named-dest is for consistency
            eatStringParam(i.destName);
            continue;
        }
        if (isArg(L"page")) {
            eatIntParam(i.pageNumber);
            continue;
        }
        if (isArg(L"view")) {
            ParseViewMode(&i.startView, param);
            ++n;
            continue;
        }
        if (isArg(L"zoom")) {
            ParseZoomValue(&i.startZoom, param);
            ++n;
            continue;
        }
        if (isArg(L"scroll")) {
            ParseScrollValue(&i.startScroll, param);
            ++n;
            continue;
        }
        if (isArg(L"appdata")) {
            i.appdataDir = str::Dup(param);
            ++n;
            continue;
        }
        if (isArg(L"plugin")) {
            // -plugin [<URL>] <parent HWND>
            if (argCount > n + 2 && !str::IsDigit(*args.at(n + 1)) && *args.at(n + 2) != '-') {
                eatStringParam(i.pluginURL);
            }
            // the argument is a (numeric) window handle to
            // become the parent of a frameless SumatraPDF
            // (used e.g. for embedding it into a browser plugin)
            i.hwndPluginParent = (HWND)(INT_PTR)_wtol(args.at(++n));
            continue;
        }
        if (isArg(L"stress-test")) {
            // -stress-test <file or dir path> [<file filter>] [<page/file range(s)>] [<cycle
            // count>x]
            // e.g. -stress-test file.pdf 25x  for rendering file.pdf 25 times
            //      -stress-test file.pdf 1-3  render only pages 1, 2 and 3 of file.pdf
            //      -stress-test dir 301- 2x   render all files in dir twice, skipping first 300
            //      -stress-test dir *.pdf;*.xps  render all files in dir that are either PDF or XPS
            eatStringParam(i.stressTestPath);
            int num;
            if (has_additional_param() && str::FindChar(additional_param(), '*')) {
                eatStringParam(i.stressTestFilter);
            }
            if (has_additional_param() && IsValidPageRange(additional_param())) {
                eatStringParam(i.stressTestRanges);
            }
            if (has_additional_param() && str::Parse(additional_param(), L"%dx%$", &num) && num > 0) {
                i.stressTestCycles = num;
                n++;
            }
            continue;
        }
        if (isArg(L"n")) {
            eatIntParam(i.stressParallelCount);
            continue;
        }
        if (isArg(L"render")) {
            eatIntParam(i.pageNumber);
            i.testRenderPage = true;
            continue;
        }
        if (isArg(L"extract-text")) {
            eatIntParam(i.pageNumber);
            i.testExtractPage = true;
            continue;
        }
        if (isArg(L"bench")) {
            WCHAR* s = str::Dup(param);
            ++n;
            i.pathsToBenchmark.Append(s);
            s = nullptr;
            if (has_additional_param() && IsBenchPagesInfo(additional_param())) {
                s = str::Dup(args.at(++n));
            }
            i.pathsToBenchmark.Append(s);
            i.exitImmediately = true;
            continue;
        }
        if (isArg(L"d")) {
            i.installDir = str::Dup(param);
            ++n;
            continue;
        }
        if (isArg(L"lang")) {
            // TODO: remove the following deprecated options within
            // a release or two
            i.lang = strconv::WstrToUtf8(param);
            ++n;
            continue;
        }
        if (isArg(L"copy-self-to")) {
            i.copySelfToPath = str::Dup(param);
            ++n;
            continue;
        }
        if (isArg(L"delete-file")) {
            i.deleteFilePath = str::Dup(param);
            ++n;
            continue;
        }
        if (isArg(L"bgcolor") || isArg(L"bg-color") || isArg(L"fwdsearch-offset") || isArg(L"fwdsearch-width") ||
            isArg(L"fwdsearch-color") || isArg(L"fwdsearch-permanent") || isArg(L"manga-mode")) {
            i.globalPrefArgs.Append(str::Dup(args.at(n)));
            i.globalPrefArgs.Append(str::Dup(args.at(++n)));
            continue;
        }
        if (isArg(L"set-color-range") && argCount > n + 2) {
            i.globalPrefArgs.Append(str::Dup(args.at(n)));
            i.globalPrefArgs.Append(str::Dup(args.at(++n)));
            i.globalPrefArgs.Append(str::Dup(args.at(++n)));
            continue;
        }
        if (isArg(L"autoupdate")) {
            // this should have been handled already by AutoUpdateMain
            n++;
            continue;
        }

    CollectFile:
        // Remember this argument as a filename to open
        WCHAR* filePath = nullptr;
        if (str::EndsWithI(argName, L".lnk")) {
            filePath = ResolveLnk(argName);
        }
        if (!filePath) {
            filePath = str::Dup(argName);
        }
        i.fileNames.Append(filePath);
    }

    if (i.justExtractFiles) {
        if (!i.installDir) {
            i.installDir = str::Dup(L".");
        }
    }
}
