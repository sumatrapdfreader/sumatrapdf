/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// utils
#include "BaseUtil.h"
#include "CmdLineParser.h"
#include "WinUtil.h"
// layout controllers
#include "SettingsStructs.h"
#include "GlobalPrefs.h"
// ui
#include "SumatraPDF.h"
#include "ParseCommandLine.h"
#include "StressTesting.h"

#ifdef DEBUG
static void EnumeratePrinters()
{
    str::Str<WCHAR> output;

    PRINTER_INFO_5 *info5Arr = nullptr;
    DWORD bufSize = 0, printersCount;
    bool fOk = EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, nullptr,
        5, (LPBYTE)info5Arr, bufSize, &bufSize, &printersCount);
    if (fOk) {
        info5Arr = (PRINTER_INFO_5 *)malloc(bufSize);
        fOk = EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, nullptr,
            5, (LPBYTE)info5Arr, bufSize, &bufSize, &printersCount);
    }
    if (!fOk || !info5Arr) {
        output.AppendFmt(L"Call to EnumPrinters failed with error %#x", GetLastError());
        MessageBox(nullptr, output.Get(), L"SumatraPDF - EnumeratePrinters", MB_OK | MB_ICONERROR);
        return;
    }
    ScopedMem<WCHAR> defName(GetDefaultPrinterName());
    for (DWORD i = 0; i < printersCount; i++) {
        const WCHAR *printerName = info5Arr[i].pPrinterName;
        const WCHAR *printerPort = info5Arr[i].pPortName;
        bool fDefault = str::Eq(defName, printerName);
        output.AppendFmt(L"%s (Port: %s, attributes: %#x%s)\n", printerName, printerPort, info5Arr[i].Attributes, fDefault ? L", default" : L"");

        DWORD bins = DeviceCapabilities(printerName, printerPort, DC_BINS, nullptr, nullptr);
        DWORD binNames = DeviceCapabilities(printerName, printerPort, DC_BINNAMES, nullptr, nullptr);
        CrashIf(bins != binNames);
        if (0 == bins) {
            output.Append(L" - no paper bins available\n");
        }
        else if (bins == (DWORD)-1) {
            output.AppendFmt(L" - Call to DeviceCapabilities failed with error %#x\n", GetLastError());
        }
        else {
            ScopedMem<WORD> binValues(AllocArray<WORD>(bins));
            DeviceCapabilities(printerName, printerPort, DC_BINS, (WCHAR *)binValues.Get(), nullptr);
            ScopedMem<WCHAR> binNameValues(AllocArray<WCHAR>(24 * binNames));
            DeviceCapabilities(printerName, printerPort, DC_BINNAMES, binNameValues.Get(), nullptr);
            for (DWORD j = 0; j < bins; j++) {
                output.AppendFmt(L" - '%s' (%d)\n", binNameValues.Get() + 24 * j, binValues.Get()[j]);
            }
        }
    }
    free(info5Arr);
    MessageBox(nullptr, output.Get(), L"SumatraPDF - EnumeratePrinters", MB_OK | MB_ICONINFORMATION);
}
#endif

/* Parse 'txt' as hex color and return the result in 'destColor' */
static void ParseColor(COLORREF *destColor, const WCHAR *txt)
{
    if (!destColor)
        return;
    if (str::StartsWith(txt, L"0x"))
        txt += 2;
    else if (str::StartsWith(txt, L"#"))
        txt += 1;

    unsigned int r, g, b;
    if (str::Parse(txt, L"%2x%2x%2x%$", &r, &g, &b))
        *destColor = RGB(r, g, b);
}

// -view [continuous][singlepage|facing|bookview]
static void ParseViewMode(DisplayMode *mode, const WCHAR *txt)
{
    *mode = prefs::conv::ToDisplayMode(txt, DM_AUTOMATIC);
}

// -zoom [fitwidth|fitpage|fitcontent|100%] (with 100% meaning actual size)
static void ParseZoomValue(float *zoom, const WCHAR *txt)
{
    if (str::EqIS(txt, L"fit page"))
        *zoom = ZOOM_FIT_PAGE;
    else if (str::EqIS(txt, L"fit width"))
        *zoom = ZOOM_FIT_WIDTH;
    else if (str::EqIS(txt, L"fit content"))
        *zoom = ZOOM_FIT_CONTENT;
    else
        str::Parse(txt, L"%f", zoom);
}

// -scroll x,y
static void ParseScrollValue(PointI *scroll, const WCHAR *txt)
{
    int x, y;
    if (str::Parse(txt, L"%d,%d%$", &x, &y))
        *scroll = PointI(x, y);
}

/* parse argument list. we assume that all unrecognized arguments are file names. */
void CommandLineInfo::ParseCommandLine(const WCHAR *cmdLine)
{
    WStrVec argList;
    ParseCmdLine(cmdLine, argList);
    size_t argCount = argList.Count();

#define is_arg(txt) str::EqI(TEXT(txt), argument)
#define is_arg_with_param(txt) (is_arg(txt) && (argCount > n + 1))
#define additional_param() argList.At(n + 1)
#define has_additional_param() ((argCount > n + 1) && ('-' != additional_param()[0]))
#define handle_string_param(name) name.Set(str::Dup(argList.At(++n)))
#define handle_int_param(name) name = _wtoi(argList.At(++n))

    for (size_t n = 1; n < argCount; n++) {
        WCHAR *argument = argList.At(n);
        if (is_arg("-register-for-pdf")) {
            makeDefault = true;
            exitImmediately = true;
            return;
        }
        else if (is_arg("-silent")) {
            // silences errors happening during -print-to and -print-to-default
            silent = true;
        }
        else if (is_arg("-print-to-default")) {
            printerName.Set(GetDefaultPrinterName());
            if (!printerName)
                printDialog = true;
            exitWhenDone = true;
        }
        else if (is_arg_with_param("-print-to")) {
            handle_string_param(printerName);
            exitWhenDone = true;
        }
        else if (is_arg("-print-dialog")) {
            printDialog = true;
        }
        else if (is_arg_with_param("-print-settings")) {
            // argument is a comma separated list of page ranges and
            // advanced options [even|odd] and [noscale|shrink|fit]
            // e.g. -print-settings "1-3,5,10-8,odd,fit"
            handle_string_param(printSettings);
            str::RemoveChars(printSettings, L" ");
            str::TransChars(printSettings, L";", L",");
        }
        else if (is_arg("-exit-when-done") || is_arg("-exit-on-print")) {
            // only affects -print-dialog (-print-to and -print-to-default
            // always exit on print) and -stress-test (useful for profiling)
            exitWhenDone = true;
        }
        else if (is_arg_with_param("-inverse-search")) {
            str::ReplacePtr(&gGlobalPrefs->inverseSearchCmdLine, argList.At(++n));
            gGlobalPrefs->enableTeXEnhancements = true;
        }
        else if ((is_arg_with_param("-forward-search") ||
                  is_arg_with_param("-fwdsearch")) && argCount > n + 2) {
            // -forward-search is for consistency with -inverse-search
            // -fwdsearch is for consistency with -fwdsearch-*
            handle_string_param(forwardSearchOrigin);
            handle_int_param(forwardSearchLine);
        }
        else if (is_arg_with_param("-nameddest") || is_arg_with_param("-named-dest")) {
            // -nameddest is for backwards compat (was used pre-1.3)
            // -named-dest is for consistency
            handle_string_param(destName);
        }
        else if (is_arg_with_param("-page")) {
            handle_int_param(pageNumber);
        }
        else if (is_arg("-restrict")) {
            restrictedUse = true;
        }
        else if (is_arg("-invertcolors") || is_arg("-invert-colors")) {
            // -invertcolors is for backwards compat (was used pre-1.3)
            // -invert-colors is for consistency
            // -invert-colors used to be a shortcut for -set-color-range 0xFFFFFF 0x000000
            // now it non-permanently swaps textColor and backgroundColor
            gGlobalPrefs->fixedPageUI.invertColors = true;
        }
        else if (is_arg("-presentation")) {
            enterPresentation = true;
        }
        else if (is_arg("-fullscreen")) {
            enterFullScreen = true;
        }
        else if (is_arg_with_param("-view")) {
            ParseViewMode(&startView, argList.At(++n));
        }
        else if (is_arg_with_param("-zoom")) {
            ParseZoomValue(&startZoom, argList.At(++n));
        }
        else if (is_arg_with_param("-scroll")) {
            ParseScrollValue(&startScroll, argList.At(++n));
        }
        else if (is_arg("-console")) {
            showConsole = true;
        }
        else if (is_arg_with_param("-plugin")) {
            // -plugin [<URL>] <parent HWND>
            if (argCount > n + 2 && !str::IsDigit(*argList.At(n + 1)) && *argList.At(n + 2) != '-')
                handle_string_param(pluginURL);
            // the argument is a (numeric) window handle to
            // become the parent of a frameless SumatraPDF
            // (used e.g. for embedding it into a browser plugin)
            hwndPluginParent = (HWND)_wtol(argList.At(++n));
        }
        else if (is_arg_with_param("-stress-test")) {
            // -stress-test <file or dir path> [<file filter>] [<page/file range(s)>] [<cycle count>x]
            // e.g. -stress-test file.pdf 25x  for rendering file.pdf 25 times
            //      -stress-test file.pdf 1-3  render only pages 1, 2 and 3 of file.pdf
            //      -stress-test dir 301- 2x   render all files in dir twice, skipping first 300
            //      -stress-test dir *.pdf;*.xps  render all files in dir that are either PDF or XPS
            handle_string_param(stressTestPath);
            int num;
            if (has_additional_param() && str::FindChar(additional_param(), '*'))
                handle_string_param(stressTestFilter);
            if (has_additional_param() && IsValidPageRange(additional_param()))
                handle_string_param(stressTestRanges);
            if (has_additional_param() && str::Parse(additional_param(), L"%dx%$", &num) && num > 0) {
                stressTestCycles = num;
                n++;
            }
        }
        else if (is_arg_with_param("-n")) {
            handle_int_param(stressParallelCount);
        }
        else if (is_arg("-rand")) {
            stressRandomizeFiles = true;
        }
        else if (is_arg_with_param("-bench")) {
            WCHAR *s = str::Dup(argList.At(++n));
            pathsToBenchmark.Push(s);
            s = nullptr;
            if (has_additional_param() && IsBenchPagesInfo(additional_param())) {
                s = str::Dup(argList.At(++n));
            }
            pathsToBenchmark.Push(s);
            exitImmediately = true;
        } else if (is_arg("-crash-on-open")) {
            // to make testing of crash reporting system in pre-release/release
            // builds possible
            crashOnOpen = true;
        }
        else if (is_arg("-reuse-instance")) {
            // for backwards compatibility, -reuse-instance reuses whatever
            // instance has registered as DDE server
            reuseDdeInstance = true;
        }
        // TODO: remove the following deprecated options within a release or two
        else if (is_arg("-esc-to-exit")) {
            gGlobalPrefs->escToExit = true;
        }
        else if (is_arg_with_param("-bgcolor") || is_arg_with_param("-bg-color")) {
            // -bgcolor is for backwards compat (was used pre-1.3)
            // -bg-color is for consistency
            ParseColor(&gGlobalPrefs->mainWindowBackground, argList.At(++n));
        }
        else if (is_arg_with_param("-lang")) {
            lang.Set(str::conv::ToAnsi(argList.At(++n)));
        }
        else if (is_arg("-set-color-range") && argCount > n + 2) {
            ParseColor(&gGlobalPrefs->fixedPageUI.textColor, argList.At(++n));
            ParseColor(&gGlobalPrefs->fixedPageUI.backgroundColor, argList.At(++n));
        }
        else if (is_arg_with_param("-fwdsearch-offset")) {
            handle_int_param(gGlobalPrefs->forwardSearch.highlightOffset);
            gGlobalPrefs->enableTeXEnhancements = true;
        }
        else if (is_arg_with_param("-fwdsearch-width")) {
            handle_int_param(gGlobalPrefs->forwardSearch.highlightWidth);
            gGlobalPrefs->enableTeXEnhancements = true;
        }
        else if (is_arg_with_param("-fwdsearch-color")) {
            ParseColor(&gGlobalPrefs->forwardSearch.highlightColor, argList.At(++n));
            gGlobalPrefs->enableTeXEnhancements = true;
        }
        else if (is_arg_with_param("-fwdsearch-permanent")) {
            handle_int_param(gGlobalPrefs->forwardSearch.highlightPermanent);
            gGlobalPrefs->enableTeXEnhancements = true;
        }
        else if (is_arg_with_param("-manga-mode")) {
            const WCHAR *s = argList.At(++n);
            gGlobalPrefs->comicBookUI.cbxMangaMode = str::EqI(L"true", s) || str::Eq(L"1", s);
        }
#if defined(SUPPORTS_AUTO_UPDATE) || defined(DEBUG)
        else if (is_arg_with_param("-autoupdate")) {
            n++; // this should have been handled already by AutoUpdateMain
        }
#endif
#ifdef DEBUG
        else if (is_arg("-enum-printers")) {
            EnumeratePrinters();
            /* this is for testing only, exit immediately */
            exitImmediately = true;
            return;
        }
#endif
        else {
            // Remember this argument as a filename to open
            WCHAR *filePath = nullptr;
            if (str::EndsWithI(argList.At(n), L".lnk"))
                filePath = ResolveLnk(argList.At(n));
            if (!filePath)
                filePath = str::Dup(argList.At(n));
            fileNames.Push(filePath);
        }
    }
#undef is_arg
#undef is_arg_with_param
#undef additional_param
#undef has_additional_param
#undef handle_string_param
#undef handle_int_param
}
