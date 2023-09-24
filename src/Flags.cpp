/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/CmdLineArgsIter.h"
#include "utils/WinUtil.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "GlobalPrefs.h"
#include "Flags.h"
#include "MainWindow.h"
#include "StressTesting.h"
#include "SumatraConfig.h"

#include "utils/Log.h"

#define ARGS(V)                                  \
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
    V(WithSearch, "with-search")                 \
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
    V(StressTest, "stress-test")                 \
    V(N, "n")                                    \
    V(Max, "max")                                \
    V(Render, "render")                          \
    V(ExtractText, "extract-text")               \
    V(Bench, "bench")                            \
    V(Dir, "d")                                  \
    V(InstallDir, "install-dir")                 \
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
    V(Search, "search")                          \
    V(AllUsers, "all-users")                     \
    V(AllUsers2, "allusers")                     \
    V(RunInstallNow, "run-install-now")          \
    V(TestBrowser, "test-browser")               \
    V(Adobe, "a")                                \
    V(DDE, "dde")                                \
    V(SetColorRange, "set-color-range")

#define MAKE_ARG(__arg, __name) __arg,
#define MAKE_STR(__arg, __name) __name "\0"

enum class Arg { Unknown = -1, ARGS(MAKE_ARG) };

static const char* gArgNames = ARGS(MAKE_STR);

static void EnumeratePrinters() {
    str::Str out;

    PRINTER_INFO_5* info5Arr = nullptr;
    DWORD bufSize = 0;
    DWORD printersCount = 0;
    DWORD flags = PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS;
    BOOL ok = EnumPrintersW(flags, nullptr, 5, nullptr, 0, &bufSize, &printersCount);
    if (ok != 0 || GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        info5Arr = (PRINTER_INFO_5*)calloc(bufSize, 1);
        if (info5Arr != nullptr) {
            ok = EnumPrintersW(flags, nullptr, 5, (LPBYTE)info5Arr, bufSize, &bufSize, &printersCount);
        }
    }
    if (ok == 0 || !info5Arr) {
        out.AppendFmt("Call to EnumPrinters failed with error %#x", GetLastError());
        MessageBoxA(nullptr, out.Get(), "SumatraPDF - EnumeratePrinters", MB_OK | MB_ICONERROR);
        free(info5Arr);
        return;
    }
    char* defName = GetDefaultPrinterNameTemp();
    for (DWORD i = 0; i < printersCount; i++) {
        PRINTER_INFO_5& info = info5Arr[i];
        const WCHAR* nameW = info.pPrinterName;
        const WCHAR* portW = info.pPortName;
        DWORD attr = info.Attributes;
        char* name = ToUtf8Temp(nameW);
        char* port = ToUtf8Temp(portW);
        const char* defStr = str::Eq(defName, name) ? ", default" : "";
        out.AppendFmt("%s (Port: %s, attributes: %#x%s)\n", name, port, attr, defStr);

        DWORD bins = DeviceCapabilitiesW(nameW, portW, DC_BINS, nullptr, nullptr);
        DWORD binNames = DeviceCapabilitiesW(nameW, portW, DC_BINNAMES, nullptr, nullptr);
        CrashIf(bins != binNames);
        if (0 == bins) {
            out.Append(" - no paper bins available\n");
        } else if (bins == (DWORD)-1) {
            out.AppendFmt(" - Call to DeviceCapabilities failed with error %#x\n", GetLastError());
        } else {
            ScopedMem<WORD> binValues(AllocArray<WORD>(bins));
            DeviceCapabilitiesW(nameW, portW, DC_BINS, (WCHAR*)binValues.Get(), nullptr);
            ScopedMem<WCHAR> binNameValues(AllocArray<WCHAR>(24 * (size_t)binNames));
            DeviceCapabilitiesW(nameW, portW, DC_BINNAMES, binNameValues.Get(), nullptr);
            for (DWORD j = 0; j < bins; j++) {
                WCHAR* ws = binNameValues.Get() + 24 * (size_t)j;
                char* s = ToUtf8Temp(ws);
                out.AppendFmt(" - '%s' (%d)\n", s, binValues.Get()[j]);
            }
        }
    }
    free(info5Arr);
    MessageBoxA(nullptr, out.Get(), "SumatraPDF - EnumeratePrinters", MB_OK | MB_ICONINFORMATION);
}

// parses a list of page ranges such as 1,3-5,7- (i..e all but pages 2 and 6)
// into an interable list (returns nullptr on parsing errors)
// caller must delete the result
bool ParsePageRanges(const char* ranges, Vec<PageRange>& result) {
    if (!ranges) {
        return false;
    }

    StrVec rangeList;
    Split(rangeList, ranges, ",", true);
    rangeList.SortNatural();

    for (char* rangeStr : rangeList) {
        int start, end;
        if (str::Parse(rangeStr, "%d-%d%$", &start, &end) && 0 < start && start <= end) {
            result.Append(PageRange{start, end});
        } else if (str::Parse(rangeStr, "%d-%$", &start) && 0 < start) {
            result.Append(PageRange{start, INT_MAX});
        } else if (str::Parse(rangeStr, "%d%$", &start) && 0 < start) {
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
bool IsValidPageRange(const char* ranges) {
    Vec<PageRange> rangeList;
    return ParsePageRanges(ranges, rangeList);
}

// <s> can be:
// * "loadonly"
// * description of page ranges e.g. "1", "1-5", "2-3,6,8-10"
bool IsBenchPagesInfo(const char* s) {
    return str::EqI(s, "loadonly") || IsValidPageRange(s);
}

// -view [continuous][singlepage|facing|bookview]
static void ParseViewMode(DisplayMode* mode, const char* s) {
    *mode = DisplayModeFromString(s, DisplayMode::Automatic);
}

static const char* zoomValues =
    "fit page\0fitpage\0fit-page\0fit width\0fitwidth\0fit-width\0fit "
    "content\0fitcontent\0fit-content\0";

// -zoom [fitwidth|fitpage|fitcontent|n]
// if a number, it's in percent e.g. 12.5 means 12.5%
// 100 means 100% i.e. actual size as e.g. given in PDF file
static void ParseZoomValue(float* zoom, const char* txtOrig) {
    auto txtDup = str::DupTemp(txtOrig);
    char* txt = str::ToLowerInPlace(txtDup);
    int zoomVal = seqstrings::StrToIdx(zoomValues, txt);
    if (zoomVal >= 0) {
        // 0-2 : fit page
        // 3-5 : fit width
        // 6-8 : fit content
        *zoom = kZoomFitContent;
        if (zoomVal <= 5) {
            *zoom = kZoomFitWidth;
        }
        if (zoomVal <= 2) {
            *zoom = kZoomFitPage;
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
        *zoom = kZoomActualSize;
    }
}

// -scroll x,y
static void ParseScrollValue(Point* scroll, const char* txt) {
    int x, y;
    if (str::Parse(txt, "%d,%d%$", &x, &y)) {
        *scroll = Point(x, y);
    }
}

static Arg GetArg(const char* s) {
    if (!CouldBeArg(s)) {
        return Arg::Unknown;
    }
    const char* arg = s + 1;
    int idx = seqstrings::StrToIdxIS(gArgNames, arg);
    if (idx < 0) {
        return Arg::Unknown;
    }
    return (Arg)idx;
}

// https://stackoverflow.com/questions/619158/adobe-reader-command-line-reference
// https://www.robvanderwoude.com/commandlineswitches.php#Acrobat
// with Sumatra extensions
void ParseAdobeFlags(FileArgs& i, const char* s) {
    StrVec parts;
    StrVec parts2;
    char* name;
    char* val;
    int valN;

    // tha args can be separated with `#` or `?` or `:`
    // i.e. `foo#bar` or foo&bar` or `foo:bar`
    Split(parts, s, "&", true);
    if (parts.Size() == 1) {
        parts.Reset();
        Split(parts, s, "#", true);
    }
    if (parts.Size() == 1) {
        parts.Reset();
        Split(parts, s, ";", true);
    }

    for (char* part : parts) {
        parts2.Reset();
        Split(parts2, part, "=", true);
        if (parts2.Size() != 2) {
            continue;
        }
        name = parts2[0];
        val = parts2[1];
        valN = atoi(val);

        // https://pdfobject.com/pdf/pdf_open_parameters_acro8.pdf
        if (str::EqI("name", "nameddest")) {
            i.destName = str::Dup(val);
            continue;
        }
        if (str::EqI(name, "page") && valN >= 1) {
            i.pageNumber = valN;
            continue;
        }
        // comment=
        // collab=setting
        if (str::EqI(name, "zoom")) {
            // TODO: handle zoom
            // 100 is 100%
            continue;
        }
        if (str::EqI(name, "view")) {
            // TODO: Fit FitH FitH,top FitV FitV,left
            // FitB FitBH FitBH,top FitBV, FitBV,left
            continue;
        }
        // viewrect
        // pagemode=bookmarks, thumbs, none
        // scrollbar=1|0
        if (str::EqI(name, "search")) {
            if (str::Len(val) > 0) {
                i.search = str::Dup(val);
            }
            continue;
        }
        // toolbar=1|0
        // statusbar=1|0
        // messages=1|0
        // navpanes=1|0
        // highlight=lrt,rt,top,btm
        // fdf=URL

        // those are Sumatra additions

        if (str::EqI(name, "annotatt") && valN > 0) {
            // for annotations that are attachments this is pdf object number
            // representing the attachment
            i.annotAttObjNum = valN;
            continue;
        }

        if (str::EqI(name, "attachno") && valN > 0) {
            // this is attachment number, use PdfLoadAttachment() to load it
            i.attachmentNo = valN;
            continue;
        }
    }
}

FileArgs::~FileArgs() {
    str::FreePtr(&origPath);
    str::FreePtr(&cleanPath);
    str::FreePtr(&destName);
    str::FreePtr(&search);
}

// given file path `foo.pdf?page=4;dest=foo` etc., extract `?page=4;dest=foo`
// args into FileArgs
// returns nullptr if there are not args
FileArgs* ParseFileArgs(const char* path) {
    const char* hashPos = str::FindChar(path, '?');
    if (!hashPos) {
        return nullptr;
    }
    FileArgs* res = new FileArgs();
    res->origPath = str::Dup(path);
    char* s = str::DupTemp(path);
    size_t n = hashPos - path;
    res->cleanPath = str::Dup(s, n);
    ParseAdobeFlags(*res, hashPos + 1);
    return res;
}

/* parse argument list. we assume that all unrecognized arguments are file names. */
void ParseFlags(const WCHAR* cmdLine, Flags& i) {
    CmdLineArgsIter args(cmdLine);

    const char* param = nullptr;
    int paramInt = 0;

    for (const char* argName = args.NextArg(); argName != nullptr; argName = args.NextArg()) {
        Arg arg = GetArg(argName);
        if (arg == Arg::Unknown) {
            goto CollectFile;
        }

        if (arg == Arg::Silent || arg == Arg::Silent2) {
            // silences errors happening during -print-to and -print-to-default
            i.silent = true;
            continue;
        }
        if (arg == Arg::PrintToDefault) {
            i.printerName = str::Dup(GetDefaultPrinterNameTemp());
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
        if (arg == Arg::WithFilter || arg == Arg::WithSearch) {
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
        if (arg == Arg::RunInstallNow) {
            i.runInstallNow = true;
            continue;
        }
        if (arg == Arg::TestBrowser) {
            i.testBrowser = true;
            continue;
        }
        if ((arg == Arg::AllUsers) || (arg == Arg::AllUsers2)) {
            i.allUsers = true;
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
            i.globalPrefArgs.Append(argName);
            continue;
        }
        if (arg == Arg::ArgEnumPrinters && (gIsDebugBuild || gIsPreReleaseBuild)) {
            EnumeratePrinters();
            /* this is for testing only, exit immediately */
            i.exitImmediately = true;
            return;
        }
        param = args.EatParam();
        // follwing args require at least one param
        // if no params here, assume this is a file
        if (nullptr == param) {
            // argName starts with '-' but there are no params after that and it's not
            // one of the args without params, so assume this is a file that starts with '-'
            goto CollectFile;
        }
        paramInt = atoi(param);

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
            // advanced options [even|odd], [noscale|shrink|fit] and [autorotation|portrait|landscape] and
            // disable-auto-rotation e.g. -print-settings "1-3,5,10-8,odd,fit"
            i.printSettings = str::Dup(param);
            str::RemoveCharsInPlace(i.printSettings, " ");
            str::TransCharsInPlace(i.printSettings, ";", ",");
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
            i.forwardSearchLine = atoi(args.EatParam());
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
                i.hwndPluginParent = (HWND)(INT_PTR)atol(args.EatParam());
            } else {
                i.hwndPluginParent = (HWND)(INT_PTR)atol(param);
            }
            continue;
        }

        if (arg == Arg::StressTest) {
            // -stress-test <file or dir path> [<file filter>] [<page/file range(s)>] [<cycle
            // count>x]
            // e.g. -stress-test file.pdf 25x  for rendering file.pdf 25 times
            //      -stress-test file.pdf 1-3  render only pages 1, 2 and 3 of file.pdf
            //      -stress-test dir 301- 2x   render all files in dir twice, skipping first 300
            //      -stress-test dir *.pdf;*.xps  render all files in dir that are either PDF or XPS
            i.stressTestPath = str::Dup(param);
            int num;
            const char* s = args.AdditionalParam(1);
            if (s && str::FindChar(s, '*')) {
                i.stressTestFilter = str::Dup(args.EatParam());
                s = args.AdditionalParam(1);
            }
            if (s && IsValidPageRange(s)) {
                i.stressTestRanges = str::Dup(args.EatParam());
                s = args.AdditionalParam(1);
            }
            if (s && str::Parse(s, "%dx%$", &num) && num > 0) {
                i.stressTestCycles = num;
                args.EatParam();
            }
            continue;
        }

        if (arg == Arg::N) {
            i.stressParallelCount = paramInt;
            continue;
        }
        if (arg == Arg::Max) {
            i.stressTestMax = paramInt;
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
            i.pathsToBenchmark.Append(param);
            const char* s = args.AdditionalParam(1);
            if (s && IsBenchPagesInfo(s)) {
                s = args.EatParam();
                i.pathsToBenchmark.Append(s);
            } else {
                // pathsToBenchmark are always in pairs
                // i.e. path + page spec
                // if page spec is missing, we do nullptr
                i.pathsToBenchmark.Append(nullptr);
            }
            i.exitImmediately = true;
            continue;
        }
        if (arg == Arg::Dir || arg == Arg::InstallDir) {
            i.installDir = str::Dup(param);
            continue;
        }
        if (arg == Arg::DDE) {
            i.dde = str::Dup(param);
            continue;
        }
        if (arg == Arg::Lang) {
            // TODO: remove the following deprecated options within
            // a release or two
            i.lang = str::Dup(param);
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
        if (arg == Arg::Adobe) {
            FileArgs fargs;
            ParseAdobeFlags(fargs, param);
            i.search = fargs.search ? str::Dup(fargs.search) : i.search;
            i.destName = fargs.destName ? str::Dup(fargs.destName) : i.destName;
            i.pageNumber = fargs.pageNumber > 0 ? fargs.pageNumber : i.pageNumber;
            // TODO: annotAttObjNum and attachmentNo?
            continue;
        }
        if (arg == Arg::BgCol || arg == Arg::BgCol2 || arg == Arg::FwdSearchOffset || arg == Arg::FwdSearchWidth ||
            arg == Arg::FwdSearchColor || arg == Arg::FwdSearchPermanent || arg == Arg::MangaMode) {
            i.globalPrefArgs.Append(argName);
            i.globalPrefArgs.Append(param);
            continue;
        }
        if (arg == Arg::SetColorRange && args.AdditionalParam(1)) {
            i.globalPrefArgs.Append(argName);
            i.globalPrefArgs.Append(param);
            i.globalPrefArgs.Append(args.EatParam());
            continue;
        }
        // again, argName is any of the known args, so assume it's a file starting with '-'
        args.RewindParam();

    CollectFile:
        // TODO: resolve .lnk when opening file
        const char* filePath = argName;
        if (str::EndsWithI(filePath, ".lnk")) {
            filePath = ResolveLnkTemp(argName);
        }
        if (filePath) { // resolve might fail
            i.fileNames.Append(filePath);
        }
    }

    if (i.justExtractFiles) {
        // silently extract files to directory given if /d
        // or current directory if no /d given
        i.silent = true;
        if (!i.installDir) {
            i.installDir = str::Dup(".");
        }
    }
}

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
    str::Free(dde);
}
