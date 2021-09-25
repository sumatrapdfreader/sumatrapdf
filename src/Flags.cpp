/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/CmdLineArgsIter.h"
#include "utils/WinUtil.h"

#include "DisplayMode.h"
#include "SettingsStructs.h"
#include "GlobalPrefs.h"
#include "Flags.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "WindowInfo.h"
#include "StressTesting.h"
#include "SumatraConfig.h"

#include "utils/Log.h"

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
    str::Free(updateSelfTo);
    str::Free(deleteFile);
    str::Free(search);

    // TODO: temporary
    str::Free(toEpubPath);
}

#if defined(DEBUG)
static void EnumeratePrinters() {
    str::WStr output;

    PRINTER_INFO_5* info5Arr = nullptr;
    DWORD bufSize{0};
    DWORD printersCount{0};
    BOOL ok =
        EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, nullptr, 5, nullptr, 0, &bufSize, &printersCount);
    if (ok != 0 || GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        info5Arr = (PRINTER_INFO_5*)malloc(bufSize);
        if (info5Arr != nullptr) {
            ok = EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, nullptr, 5, (LPBYTE)info5Arr, bufSize,
                               &bufSize, &printersCount);
        }
    }
    if (ok == 0 || !info5Arr) {
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
#endif

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

#define ARGS(V)                                  \
    V(RegisterForPdf, "register-for-pdf")        \
    V(RegisterForPdf2, "register")               \
    V(Silent, "s")                               \
    V(Silent2, "silent")                         \
    V(PrintToDefault, "print-to-default")        \
    V(PrintDialog, "print-dialog")               \
    V(Help, "h")                                 \
    V(Help2, "?")                                \
    V(Help3, "help")                             \
    V(ExitWhenDone, "exit-when-done")            \
    V(ExitOnPrint, "exit-on-print")              \
    V(Restrict, "restrict")                      \
    V(Presentation, "presentation")              \
    V(FullScreen, "fullscreen")                  \
    V(InvertColors, "invertcolors")              \
    V(InvertColors2, "invert-colors")            \
    V(Console, "console")                        \
    V(Install, "install")                        \
    V(UnInstall, "uninstall")                    \
    V(WithFilter, "with-filter")                 \
    V(WithPreview, "with-preview")               \
    V(Rand, "rand")                              \
    V(Regress, "regress")                        \
    V(Extract, "x")                              \
    V(Tester, "tester")                          \
    V(TestApp, "testapp")                        \
    V(NewWindow, "new-window")                   \
    V(Log, "log")                                \
    V(CrashOnOpen, "crash-on-open")              \
    V(ReuseInstance, "reuse-instance")           \
    V(EscToExit, "esc-to-exit")                  \
    V(ArgEnumPrinters, "enum-printers")          \
    V(SleepMs, "sleep-ms")                       \
    V(PrintTo, "print-to")                       \
    V(PrintSettings, "print-settings")           \
    V(InverseSearch, "inverse-search")           \
    V(ForwardSearch1, "forward-search")          \
    V(ForwardSearch2, "fwdsearch")               \
    V(NamedDest, "nameddest")                    \
    V(NamedDest2, "named-dest")                  \
    V(Page, "page")                              \
    V(View, "view")                              \
    V(Zoom, "zoom")                              \
    V(Scroll, "scroll")                          \
    V(AppData, "appdata")                        \
    V(Plugin, "plugin")                          \
    V(ArgStressTest, "stress-test")              \
    V(N, "n")                                    \
    V(Render, "render")                          \
    V(ExtractText, "extract-text")               \
    V(Bench, "bench")                            \
    V(Dir, "d")                                  \
    V(Lang, "lang")                              \
    V(UpdateSelfTo, "update-self-to")            \
    V(ArgDeleteFile, "delete-file")              \
    V(BgCol, "bgcolor")                          \
    V(BgCol2, "bg-color")                        \
    V(FwdSearchOffset, "fwdsearch-offset")       \
    V(FwdSearchWidth, "fwdsearch-width")         \
    V(FwdSearchColor, "fwdsearch-color")         \
    V(FwdSearchPermanent, "fwdsearch-permanent") \
    V(MangaMode, "manga-mode")                   \
    V(ToEpub, "to-epub")                         \
    V(Search, "search")                          \
    V(SetColorRange, "set-color-range")

#define MAKE_ARG(__arg, __name) __arg,
#define MAKE_STR(__arg, __name) __name "\0"

enum class Arg { Unknown = -1, ARGS(MAKE_ARG) };

static const char* gArgNames = ARGS(MAKE_STR);

static Arg GetArg(const WCHAR* s) {
    if (!CouldBeArg(s)) {
        return Arg::Unknown;
    }
    char* arg = ToUtf8Temp(s + 1);
    int idx = seqstrings::StrToIdxIS(gArgNames, arg);
    if (idx < 0) {
        return Arg::Unknown;
    }
    return (Arg)idx;
}

/* parse argument list. we assume that all unrecognized arguments are file names. */
void ParseFlags(const WCHAR* cmdLine, Flags& i) {
    CmdLineArgsIter args(cmdLine);

    const WCHAR* param{nullptr};
    int paramInt{0};

    for (auto argName = args.NextArg(); argName != nullptr; argName = args.NextArg()) {
        Arg arg = GetArg(argName);
        if (arg == Arg::Unknown) {
            goto CollectFile;
        }

        if (arg == Arg::RegisterForPdf || arg == Arg::RegisterForPdf2) {
            i.registerAsDefault = true;
            i.exitImmediately = true;
            return;
        }

        if (arg == Arg::Silent || arg == Arg::Silent2) {
            // silences errors happening during -print-to and -print-to-default
            i.silent = true;
            continue;
        }
        if (arg == Arg::PrintToDefault) {
            i.printerName = GetDefaultPrinterName();
            if (!i.printerName) {
                i.printDialog = true;
            }
            i.exitWhenDone = true;
            continue;
        }
        if (arg == Arg::PrintDialog) {
            i.printDialog = true;
            continue;
        }
        if (arg == Arg::Help || arg == Arg::Help2 || arg == Arg::Help3) {
            i.showHelp = true;
            continue;
        }
        if (arg == Arg::ExitWhenDone || arg == Arg::ExitOnPrint) {
            // only affects -print-dialog (-print-to and -print-to-default
            // always exit on print) and -stress-test (useful for profiling)
            i.exitWhenDone = true;
            continue;
        }
        if (arg == Arg::Restrict) {
            i.restrictedUse = true;
            continue;
        }
        if (arg == Arg::Presentation) {
            i.enterPresentation = true;
            continue;
        }
        if (arg == Arg::FullScreen) {
            i.enterFullScreen = true;
            continue;
        }
        if (arg == Arg::InvertColors || arg == Arg::InvertColors2) {
            // -invertcolors is for backwards compat (was used pre-1.3)
            // -invert-colors is for consistency
            // -invert-colors used to be a shortcut for -set-color-range 0xFFFFFF 0x000000
            // now it non-permanently swaps textColor and backgroundColor
            i.invertColors = true;
            continue;
        }
        if (arg == Arg::Console) {
            i.showConsole = true;
            continue;
        }
        if (arg == Arg::Install) {
            i.install = true;
            continue;
        }
        if (arg == Arg::UnInstall) {
            i.uninstall = true;
            continue;
        }
        if (arg == Arg::WithFilter) {
            i.withFilter = true;
            continue;
        }
        if (arg == Arg::WithPreview) {
            i.withPreview = true;
            continue;
        }
        if (arg == Arg::Rand) {
            i.stressRandomizeFiles = true;
            continue;
        }
        if (arg == Arg::Regress) {
            i.regress = true;
            continue;
        }
        if (arg == Arg::Extract) {
            i.justExtractFiles = true;
            continue;
        }
        if (arg == Arg::Tester) {
            i.tester = true;
            continue;
        }
        if (arg == Arg::TestApp) {
            i.testApp = true;
            continue;
        }
        if (arg == Arg::NewWindow) {
            i.inNewWindow = true;
            continue;
        }
        if (arg == Arg::Log) {
            i.log = true;
            continue;
        }
        if (arg == Arg::CrashOnOpen) {
            // to make testing of crash reporting system in pre-release/release
            // builds possible
            i.crashOnOpen = true;
            continue;
        }
        if (arg == Arg::ReuseInstance) {
            // for backwards compatibility, -reuse-instance reuses whatever
            // instance has registered as DDE server
            i.reuseDdeInstance = true;
            continue;
        }
        if (arg == Arg::EscToExit) {
            i.globalPrefArgs.Append(str::Dup(argName));
            continue;
        }
#if defined(DEBUG)
        if (arg == Arg::ArgEnumPrinters) {
            EnumeratePrinters();
            /* this is for testing only, exit immediately */
            i.exitImmediately = true;
            return;
        }
#endif
        param = args.EatParam();
        // follwing args require at least one param
        // if no params here, assume this is a file
        if (nullptr == param) {
            // argName starts with '-' but there are no params after that and it's not
            // one of the args without params, so assume this is a file that starts with '-'
            goto CollectFile;
        }
        paramInt = _wtoi(param);

        if (arg == Arg::SleepMs) {
            i.sleepMs = paramInt;
            continue;
        }

        if (arg == Arg::PrintTo) {
            i.printerName = str::Dup(param);
            i.exitWhenDone = true;
            continue;
        }
        if (arg == Arg::PrintSettings) {
            // argument is a comma separated list of page ranges and
            // advanced options [even|odd], [noscale|shrink|fit] and [autorotation|portrait|landscape]
            // e.g. -print-settings "1-3,5,10-8,odd,fit"
            i.printSettings = str::Dup(param);
            str::RemoveCharsInPlace(i.printSettings, L" ");
            str::TransCharsInPlace(i.printSettings, L";", L",");
            continue;
        }
        if (arg == Arg::InverseSearch) {
            i.inverseSearchCmdLine = str::Dup(param);
            continue;
        }
        if ((arg == Arg::ForwardSearch1 || arg == Arg::ForwardSearch2) && args.AdditionalParam(1)) {
            // -forward-search is for consistency with -inverse-search
            // -fwdsearch is for consistency with -fwdsearch-*
            i.forwardSearchOrigin = str::Dup(param);
            i.forwardSearchLine = _wtoi(args.EatParam());
            continue;
        }
        if (arg == Arg::NamedDest || arg == Arg::NamedDest2) {
            // -nameddest is for backwards compat (was used pre-1.3)
            // -named-dest is for consistency
            i.destName = str::Dup(param);
            continue;
        }
        if (arg == Arg::Page) {
            i.pageNumber = paramInt;
            continue;
        }
        if (arg == Arg::View) {
            ParseViewMode(&i.startView, param);
            continue;
        }
        if (arg == Arg::Zoom) {
            ParseZoomValue(&i.startZoom, param);
            continue;
        }
        if (arg == Arg::Scroll) {
            ParseScrollValue(&i.startScroll, param);
            continue;
        }
        if (arg == Arg::AppData) {
            i.appdataDir = str::Dup(param);
            continue;
        }
        if (arg == Arg::Plugin) {
            // -plugin [<URL>] <parent HWND>
            // <parent HWND> is a (numeric) window handle to
            // become the parent of a frameless SumatraPDF
            // (used e.g. for embedding it into a browser plugin)
            if (args.AdditionalParam(1) && !str::IsDigit(*param)) {
                i.pluginURL = str::Dup(param);
                i.hwndPluginParent = (HWND)(INT_PTR)_wtol(args.EatParam());
            } else {
                i.hwndPluginParent = (HWND)(INT_PTR)_wtol(param);
            }
            continue;
        }

        if (arg == Arg::ArgStressTest) {
            // -stress-test <file or dir path> [<file filter>] [<page/file range(s)>] [<cycle
            // count>x]
            // e.g. -stress-test file.pdf 25x  for rendering file.pdf 25 times
            //      -stress-test file.pdf 1-3  render only pages 1, 2 and 3 of file.pdf
            //      -stress-test dir 301- 2x   render all files in dir twice, skipping first 300
            //      -stress-test dir *.pdf;*.xps  render all files in dir that are either PDF or XPS
            i.stressTestPath = str::Dup(param);
            int num;
            const WCHAR* s = args.AdditionalParam(1);
            if (s && str::FindChar(s, '*')) {
                i.stressTestFilter = str::Dup(args.EatParam());
                s = args.AdditionalParam(1);
            }
            if (s && IsValidPageRange(s)) {
                i.stressTestRanges = str::Dup(args.EatParam());
                s = args.AdditionalParam(1);
            }
            if (s && str::Parse(s, L"%dx%$", &num) && num > 0) {
                i.stressTestCycles = num;
                args.EatParam();
            }
            continue;
        }

        if (arg == Arg::N) {
            i.stressParallelCount = paramInt;
            continue;
        }
        if (arg == Arg::Render) {
            i.testRenderPage = true;
            i.pageNumber = paramInt;
            continue;
        }
        if (arg == Arg::ExtractText) {
            i.testExtractPage = true;
            i.pageNumber = paramInt;
            continue;
        }
        if (arg == Arg::Bench) {
            i.pathsToBenchmark.Append(str::Dup(param));
            const WCHAR* s = args.AdditionalParam(1);
            if (s && IsBenchPagesInfo(s)) {
                s = str::Dup(args.EatParam());
                i.pathsToBenchmark.Append((WCHAR*)s);
            } else {
                // pathsToBenchmark are always in pairs
                // i.e. path + page spec
                // if page spec is missing, we do nullptr
                i.pathsToBenchmark.Append(nullptr);
            }
            i.exitImmediately = true;
            continue;
        }
        if (arg == Arg::Dir) {
            i.installDir = str::Dup(param);
            continue;
        }
        if (arg == Arg::ToEpub) {
            i.toEpubPath = str::Dup(param);
            continue;
        }
        if (arg == Arg::Lang) {
            // TODO: remove the following deprecated options within
            // a release or two
            i.lang = strconv::WstrToUtf8(param);
            continue;
        }
        if (arg == Arg::UpdateSelfTo) {
            i.updateSelfTo = str::Dup(param);
            continue;
        }
        if (arg == Arg::ArgDeleteFile) {
            i.deleteFile = str::Dup(param);
            continue;
        }
        if (arg == Arg::Search) {
            i.search = str::Dup(param);
            continue;
        }
        if (arg == Arg::BgCol || arg == Arg::BgCol2 || arg == Arg::FwdSearchOffset || arg == Arg::FwdSearchWidth ||
            arg == Arg::FwdSearchColor || arg == Arg::FwdSearchPermanent || arg == Arg::MangaMode) {
            i.globalPrefArgs.Append(str::Dup(argName));
            i.globalPrefArgs.Append(str::Dup(param));
            continue;
        }
        if (arg == Arg::SetColorRange && args.AdditionalParam(1)) {
            i.globalPrefArgs.Append(str::Dup(argName));
            i.globalPrefArgs.Append(str::Dup(param));
            i.globalPrefArgs.Append(str::Dup(args.EatParam()));
            continue;
        }
        // again, argName is any of the known args, so assume it's a file starting with '-'
        args.RewindParam();

    CollectFile:
        WCHAR* filePath = nullptr;
        // TODO: resolve .lnk when opening file
        if (str::EndsWithI(argName, L".lnk")) {
            filePath = ResolveLnk(argName);
        }
        if (!filePath) {
            filePath = str::Dup(argName);
        }
        i.fileNames.Append(filePath);
    }

    if (i.justExtractFiles) {
        // silently extract files to directory given if /d
        // or current directory if no /d given
        i.silent = true;
        if (!i.installDir) {
            i.installDir = str::Dup(L".");
        }
    }
}
