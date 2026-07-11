/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/CmdLineArgsIter.h"
#if OS_WIN
#include "base/Win.h"
#endif

#include "Settings.h"
#include "DisplayMode.h"
#include "Flags.h"
#if OS_WIN
#include "Print.h"
#endif
#include "SumatraLog.h"
#if OS_WIN && !defined(SUMATRA_TEST_UTIL)
#include "Translations.h"
#endif

// @gen-start flags
// clang-format off
enum class Arg {
    Unknown = -1,
    Silent = 0, Silent2 = 1, FastInstall = 2, PrintToDefault = 3,
    PrintDialog = 4, PrintDialogAdobe = 5, Help = 6, Help2 = 7,
    Help3 = 8, ExitWhenDone = 9, ExitOnPrint = 10, Restrict = 11,
    Presentation = 12, FullScreen = 13, InvertColors = 14, InvertColors2 = 15,
    Console = 16, Install = 17, UnInstall = 18, WithFilter = 19,
    WithSearch = 20, WithPreview = 21, Rand = 22, Regress = 23,
    Extract = 24, Tester = 25, TestApp = 26, TestPlugin = 27,
    TestPreview = 28, NewWindow = 29, Log = 30, LogToFile = 31,
    CrashOnOpen = 32, ReuseInstance = 33, EscToExit = 34, ArgEnumPrinters = 35,
    ListPrinters = 36, SleepMs = 37, PrintTo = 38, PrintSilent = 39,
    PrintSettings = 40, InverseSearch = 41, ForwardSearch1 = 42, ForwardSearch2 = 43,
    NamedDest = 44, NamedDest2 = 45, Page = 46, View = 47,
    Zoom = 48, Scroll = 49, AppData = 50, Plugin = 51,
    StressTest = 52, N = 53, Max = 54, MaxFiles = 55,
    Render = 56, ExtractText = 57, Bench = 58, Dir = 59,
    InstallDir = 60, Lang = 61, UpdateSelfTo = 62, ArgDeleteFile = 63,
    BgCol = 64, BgCol2 = 65, FwdSearchOffset = 66, FwdSearchWidth = 67,
    FwdSearchColor = 68, FwdSearchPermanent = 69, MangaMode = 70, Search = 71,
    AllUsers = 72, AllUsers2 = 73, RunInstallNow = 74, Adobe = 75,
    DDE = 76, Pwd = 77, EngineDump = 78, SetColorRange = 79,
    UpgradeFrom = 80, ForTesting = 81, DumpExif = 82, DumpChm = 83,
    Control = 84, UnitTests = 85,
};

static SeqStrings gArgNames =
    "s\0" "silent\0" "fast-install\0" "print-to-default\0"
    "print-dialog\0" "p\0" "h\0" "?\0"
    "help\0" "exit-when-done\0" "exit-on-print\0" "restrict\0"
    "presentation\0" "fullscreen\0" "invertcolors\0" "invert-colors\0"
    "console\0" "install\0" "uninstall\0" "with-filter\0"
    "with-search\0" "with-preview\0" "rand\0" "regress\0"
    "x\0" "tester\0" "testapp\0" "test-plugin\0"
    "test-preview\0" "new-window\0" "log\0" "log-to-file\0"
    "crash-on-open\0" "reuse-instance\0" "esc-to-exit\0" "enum-printers\0"
    "list-printers\0" "sleep-ms\0" "print-to\0" "t\0"
    "print-settings\0" "inverse-search\0" "forward-search\0" "fwdsearch\0"
    "nameddest\0" "named-dest\0" "page\0" "view\0"
    "zoom\0" "scroll\0" "appdata\0" "plugin\0"
    "stress-test\0" "n\0" "max\0" "max-files\0"
    "render\0" "extract-text\0" "bench\0" "d\0"
    "install-dir\0" "lang\0" "update-self-to\0" "delete-file\0"
    "bgcolor\0" "bg-color\0" "fwdsearch-offset\0" "fwdsearch-width\0"
    "fwdsearch-color\0" "fwdsearch-permanent\0" "manga-mode\0" "search\0"
    "all-users\0" "allusers\0" "run-install-now\0" "a\0"
    "dde\0" "pwd\0" "engine-dump\0" "set-color-range\0"
    "upgrade-from\0" "for-testing\0" "dump-exif\0" "dump-chm\0"
    "dbg-control\0" "unit-tests\0";
// clang-format on
// @gen-end flags

#if OS_WIN
void ShowPrintersDialog() {
    str::Builder out;

    gLogToConsole = true;
    RedirectIOToExistingConsole();

    GetPrintersInfo(out);
    log(ToStr(out));

    gLogToConsole = false;
#ifndef SUMATRA_TEST_UTIL
    ShowTextInWindowDialog(_TRA("SumatraPDF - Show Printers"), ToStr(out));
#else
    ShowTextInWindowDialog("SumatraPDF - Show Printers", ToStr(out));
#endif
}
#else
static TempStr GetDefaultPrinterNameTemp() {
    return {};
}

static TempStr ResolveLnkTemp(Str path) {
    return str::DupTemp(path);
}

void ShowPrintersDialog() {}
#endif

// parses a list of page ranges such as 1,3-5,7- (i..e all but pages 2 and 6)
// into an interable list (returns nullptr on parsing errors)
// caller must delete the result
bool ParsePageRanges(Str ranges, Vec<PageRange>& result) {
    if (!ranges) {
        return false;
    }

    StrVec rangeList;
    Split(&rangeList, ranges, ",", true);
    SortNatural(&rangeList);

    for (Str rangeStr : rangeList) {
        int start, end;
        if (!str::IsNull(str::Parse(rangeStr, "%d-%d%$", &start, &end)) && 0 < start && start <= end) {
            result.Append(PageRange{start, end});
        } else if (!str::IsNull(str::Parse(rangeStr, "%d-%$", &start)) && 0 < start) {
            result.Append(PageRange{start, INT_MAX});
        } else if (!str::IsNull(str::Parse(rangeStr, "%d%$", &start)) && 0 < start) {
            result.Append(PageRange{start, start});
        } else {
            return false;
        }
    }

    return len(result) > 0;
}

// a valid page range is a non-empty, comma separated list of either
// single page ("3") numbers, closed intervals "2-4" or intervals
// unlimited to the right ("5-")
bool IsValidPageRange(Str ranges) {
    Vec<PageRange> rangeList;
    return ParsePageRanges(ranges, rangeList);
}

// <s> can be:
// * "loadonly"
// * description of page ranges e.g. "1", "1-5", "2-3,6,8-10"
bool IsBenchPagesInfo(Str s) {
    return str::EqI(s, "loadonly") || IsValidPageRange(s);
}

// -view [continuous][singlepage|facing|bookview]
static void ParseViewMode(DisplayMode* mode, Str s) {
    *mode = DisplayModeFromString(s, DisplayMode::Automatic);
}

static SeqStrings zoomValues =
    "fit page\0fitpage\0fit-page\0fit width\0fitwidth\0fit-width\0fit "
    "content\0fitcontent\0fit-content\0";

// -zoom [fitwidth|fitpage|fitcontent|n]
// if a number, it's in percent e.g. 12.5 means 12.5%
// 100 means 100% i.e. actual size as e.g. given in PDF file
static void ParseZoomValue(float* zoom, Str txtOrig) {
    TempStr txtDup = str::DupTemp(txtOrig);
    str::ToLowerInPlace(txtDup);
    int zoomVal = SeqStrIndex(zoomValues, txtDup);
    if (zoomVal >= 0) {
        // 0-2 : fit page
        // 3-5 : fit width
        // 6-8 : fit content
        // 9-11: shrink to fit
        *zoom = kZoomShrinkToFit;
        if (zoomVal <= 8) {
            *zoom = kZoomFitContent;
        }
        if (zoomVal <= 5) {
            *zoom = kZoomFitWidth;
        }
        if (zoomVal <= 2) {
            *zoom = kZoomFitPage;
        }
        return;
    }
    // remove trailing % in place, if exists
    if (str::EndsWith(txtDup, "%")) {
        txtDup.len--;
    }
    str::Parse(txtDup, "%f", zoom);
    // prevent really small zoom and zoom values that are not valid numbers
    // (which would be parsed as 0)
    if (*zoom < 1.f) {
        *zoom = kZoomActualSize;
    }
}

// -scroll x,y
static void ParseScrollValue(Point* scroll, Str txt) {
    int x, y;
    if (!str::IsNull(str::Parse(txt, "%d,%d%$", &x, &y))) {
        *scroll = Point(x, y);
    }
}

// Adobe Reader /t accepts optional driver and port after the printer name; we ignore them.
static void SkipOptionalAdobePrinterParams(CmdLineArgsIter& args) {
    Str driver = args.AdditionalParam(1);
    if (!driver || CouldBeArg(driver)) {
        return;
    }
    args.EatParam();
    Str port = args.AdditionalParam(1);
    if (!port || CouldBeArg(port)) {
        return;
    }
    args.EatParam();
}

static Arg GetArg(Str s) {
    if (!CouldBeArg(s)) {
        return Arg::Unknown;
    }
    Str arg(s.s + 1, s.len - 1);
    int idx = SeqStrIndexIS(gArgNames, arg);
    if (idx < 0) {
        return Arg::Unknown;
    }
    return (Arg)idx;
}

// https://stackoverflow.com/questions/619158/adobe-reader-command-line-reference
// https://www.robvanderwoude.com/commandlineswitches.php#Acrobat
// with Sumatra extensions
void ParseAdobeFlags(FileArgs& i, Str s) {
    StrVec parts;
    StrVec parts2;
    Str name;
    Str val;
    int valN;

    // tha args can be separated with `#` or `?` or `:`
    // i.e. `foo#bar` or foo&bar` or `foo:bar`
    Split(&parts, s, "&", true);
    if (len(parts) == 1) {
        parts.Reset();
        Split(&parts, s, "#", true);
    }
    if (len(parts) == 1) {
        parts.Reset();
        Split(&parts, s, ";", true);
    }

    for (Str part : parts) {
        parts2.Reset();
        Split(&parts2, part, "=", true);
        if (len(parts2) != 2) {
            continue;
        }
        name = parts2[0];
        val = parts2[1];
        valN = ParseInt(val);

        // https://pdfobject.com/pdf/pdf_open_parameters_acro8.pdf
        if (str::EqI(name, "nameddest")) {
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
            if (len(val) > 0) {
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
    str::Free(origPath);
    str::Free(cleanPath);
    str::Free(destName);
    str::Free(search);
}

// given file path `foo.pdf?page=4;dest=foo` etc., extract `?page=4;dest=foo`
// args into FileArgs
// returns nullptr if there are not args
FileArgs* ParseFileArgs(Str path) {
    Str before, after;
    if (!str::CutCharLast(path, '?', &before, &after)) {
        return nullptr;
    }
    // don't mutilate long file paths that start with "\\?\"
    if (before.len == 2) {
        return nullptr;
    }
    FileArgs* res = new FileArgs();
    res->origPath = str::Dup(path);
    res->cleanPath = str::Dup(before);
    ParseAdobeFlags(*res, after);
    return res;
}

/* parse argument list. we assume that all unrecognized arguments are file names. */
#if OS_WIN
void ParseFlags(Arena* a, WStr cmdLine, Flags& i, Str toolNames) {
    ReportIf(!a);
    // logf("ParseFlags: cmdLine: '%s'\n", ToUtf8Temp(cmdLine));
    CmdLineArgsIter args(cmdLine);

    // if the first argument is a tool name, skip parsing flags entirely
    if (toolNames && args.curr < args.nArgs) {
        Str firstArg = args.at(args.curr);
        if (firstArg && SeqStrIndexIS(toolNames.s, firstArg) >= 0) {
            return;
        }
    }

    Str param = {};
    int paramInt = 0;

    for (Str argName = args.NextArg(); argName; argName = args.NextArg()) {
        // we register SumatraPDF with "%1" "%2" "%3" "%4"
        // for some reason that makes Directory Opus "Open With" provide the file twice
        // and gives "%3" and "%4' on cmd-line.
        // this is a hack to ignore that
        if (str::Eq(argName, "%2") || str::Eq(argName, "%3") || str::Eq(argName, "%4")) {
            logf("ParseFlags: skipping '%s'\n", argName);
            continue;
        }
        Arg arg = GetArg(argName);
        if (arg == Arg::Unknown) {
            goto CollectFile;
        }
        logf("ParseFlags: argName: '%s', arg: %d\n", argName, (int)arg);

        if (arg == Arg::Silent || arg == Arg::Silent2) {
            // silences errors happening during -print-to and -print-to-default
            i.silent = true;
            continue;
        }
        if (arg == Arg::PrintToDefault) {
            i.printerName = str::Dup(a, GetDefaultPrinterNameTemp());
            if (!i.printerName) {
                i.printDialog = true;
            }
            i.exitWhenDone = true;
            continue;
        }
        if (arg == Arg::PrintDialog || arg == Arg::PrintDialogAdobe) {
            i.printDialog = true;
            continue;
        }
        if (arg == Arg::PrintSilent) {
            // Adobe Reader: /t <file> <printer> [<driver> [<port>]]
            // also: <file> /t <printer> when the file is given earlier on the cmd-line
            Str p1 = args.EatParam();
            if (!p1) {
                goto CollectFile;
            }
            Str p2 = args.AdditionalParam(1);
            if (p2 && !CouldBeArg(p2)) {
                if (len(i.fileNames) == 0 || !str::Eq(i.fileNames[len(i.fileNames) - 1], p1)) {
                    i.fileNames.Append(p1);
                }
                i.printerName = str::Dup(a, args.EatParam());
            } else if (len(i.fileNames) > 0) {
                i.printerName = str::Dup(a, p1);
            } else {
                i.fileNames.Append(p1);
                i.printerName = str::Dup(a, GetDefaultPrinterNameTemp());
                if (!i.printerName) {
                    i.printDialog = true;
                }
            }
            i.exitWhenDone = true;
            SkipOptionalAdobePrinterParams(args);
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
            // -invert-colors is for consistency; maps to DocumentColorsFollowTheme = smart
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
        if (arg == Arg::FastInstall) {
            i.fastInstall = true;
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
        if (arg == Arg::TestPlugin) {
            i.testPlugin = true;
            // remaining args are for the plugin test
            break;
        }
        if (arg == Arg::TestPreview) {
            i.testPreview = true;
            // remaining args are for the preview test
            break;
        }
        if (arg == Arg::NewWindow) {
            i.inNewWindow = true;
            continue;
        }
        if (arg == Arg::ForTesting) {
            i.forTesting = true;
            continue;
        }
        if (arg == Arg::UnitTests) {
            i.unitTests = true;
            i.exitImmediately = true;
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
        if ((arg == Arg::ArgEnumPrinters) || (arg == Arg::ListPrinters)) {
            // defer UI until after SetCurrentLang() so _TRA resolves (issue #5697)
            i.showPrintersDialog = true;
            i.exitImmediately = true;
            return;
        }
        param = args.EatParam();
        // following args require at least one param
        // if no params here, assume this is a file
        if (!param) {
            // argName starts with '-' but there are no params after that and it's not
            // one of the args without params, so assume this is a file that starts with '-'
            goto CollectFile;
        }
        paramInt = ParseInt(param);

        if (arg == Arg::LogToFile) {
            i.logFile = str::Dup(a, param);
            i.log = true;
            continue;
        }

        if (arg == Arg::SleepMs) {
            i.sleepMs = paramInt;
            continue;
        }

        if (arg == Arg::PrintTo) {
            i.printerName = str::Dup(a, param);
            i.exitWhenDone = true;
            continue;
        }
        if (arg == Arg::PrintSettings) {
            // argument is a comma separated list of page ranges and
            // advanced options [even|odd|last], [noscale|shrink|fit] and [autorotation|portrait|landscape] and
            // disable-auto-rotation; page numbers can be negative (-1 = last page)
            // e.g. -print-settings "1-3,5,10-8,odd,fit" or "last" or "-1"
            i.printSettings = str::Dup(a, param);
            str::RemoveCharsInPlace(i.printSettings, " ");
            str::TransCharsInPlace(i.printSettings, StrL(";"), StrL(","));
            continue;
        }
        if (arg == Arg::InverseSearch) {
            i.inverseSearchCmdLine = str::Dup(a, param);
            continue;
        }
        if (arg == Arg::Control) {
            i.controlPipeName = str::Dup(a, param);
            continue;
        }
        if ((arg == Arg::ForwardSearch1 || arg == Arg::ForwardSearch2) && args.AdditionalParam(1)) {
            // -forward-search is for consistency with -inverse-search
            // -fwdsearch is for consistency with -fwdsearch-*
            i.forwardSearchOrigin = str::Dup(a, param);
            i.forwardSearchLine = ParseInt(args.EatParam());
            continue;
        }
        if (arg == Arg::NamedDest || arg == Arg::NamedDest2) {
            // -nameddest is for backwards compat (was used pre-1.3)
            // -named-dest is for consistency
            i.namedDest = str::Dup(a, param);
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
            i.appdataDir = str::Dup(a, param);
            continue;
        }
        if (arg == Arg::Plugin) {
            // -plugin [<URL>] <parent HWND>
            // <parent HWND> is a (numeric) window handle to
            // become the parent of a frameless SumatraPDF
            // (used e.g. for embedding it into a browser plugin)
            if (args.AdditionalParam(1) && !str::IsDigit(param.s[0])) {
                i.pluginURL = str::Dup(a, param);
                i.hwndPluginParent = (HWND)(intptr_t)ParseInt64(args.EatParam());
            } else {
                i.hwndPluginParent = (HWND)(intptr_t)ParseInt64(param);
            }
            continue;
        }

        if (arg == Arg::StressTest) {
            // -stress-test <file or dir path> [<file filter>] [<page/file range(s)>] [<cycle
            // count>x]
            // e.g. -stress-test file.pdf 25x    for rendering file.pdf 25 times
            //      -stress-test file.pdf 1-3  render only pages 1, 2 and 3 of file.pdf
            //      -stress-test dir 301-  2x  render all files in dir twice, skipping first 300
            //      -stress-test dir *.pdf;*.xps  render all files in dir that are either PDF or XPS
            i.stressTestPath = str::Dup(a, param);
            Str s = args.AdditionalParam(1);
            if (str::ContainsChar(s, '*')) {
                i.stressTestFilter = str::Dup(a, args.EatParam());
                s = args.AdditionalParam(1);
            }
            if (s && IsValidPageRange(s)) {
                i.stressTestRanges = str::Dup(a, args.EatParam());
                s = args.AdditionalParam(1);
            }
            int num;
            if (s && !str::IsNull(str::Parse(s, "%dx%$", &num)) && num > 0) {
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
            continue;
        }
        if (arg == Arg::MaxFiles) {
            i.maxFiles = paramInt;
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
            i.pathsToBenchmark.Append(param);
            Str s = args.AdditionalParam(1);
            if (s && IsBenchPagesInfo(s)) {
                s = args.EatParam();
                i.pathsToBenchmark.Append(s);
            } else {
                // pathsToBenchmark are always in pairs
                // i.e. path + page spec
                // if page spec is missing, we do empty Str
                i.pathsToBenchmark.Append({});
            }
            i.exitImmediately = true;
            continue;
        }
        if (arg == Arg::Dir || arg == Arg::InstallDir) {
            i.installDir = str::Dup(a, param);
            continue;
        }
        if (arg == Arg::DDE) {
            i.dde = str::Dup(a, param);
            continue;
        }
        if (arg == Arg::Pwd) {
            i.password = str::Dup(a, param);
            continue;
        }
        if (arg == Arg::DumpExif) {
            i.fileNames.Append(str::Dup(a, param));
            i.dumpExif = true;
            i.exitImmediately = true;
            continue;
        }
        if (arg == Arg::DumpChm) {
            i.fileNames.Append(str::Dup(a, param));
            i.dumpChm = true;
            i.exitImmediately = true;
            continue;
        }
        if (arg == Arg::Lang) {
            // deprecated: prefer the UiLanguage setting
            i.lang = str::Dup(a, param);
            continue;
        }
        if (arg == Arg::UpdateSelfTo) {
            i.updateSelfTo = str::Dup(a, param);
            continue;
        }
        if (arg == Arg::UpgradeFrom) {
            i.upgradeFrom = str::Dup(a, param);
            continue;
        }
        if (arg == Arg::ArgDeleteFile) {
            i.deleteFile = str::Dup(a, param);
            continue;
        }
        if (arg == Arg::Search) {
            i.search = str::Dup(a, param);
            continue;
        }
        if (arg == Arg::Adobe) {
            FileArgs fargs;
            ParseAdobeFlags(fargs, param);
            i.search = fargs.search ? str::Dup(a, fargs.search) : i.search;
            i.namedDest = fargs.destName ? str::Dup(a, fargs.destName) : i.namedDest;
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
        Str filePath = argName;
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
            i.installDir = str::Dup(a, ".");
        }
    }
}
#endif
