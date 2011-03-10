/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "SumatraPDF.h"
#include "BaseUtil.h"
#include "TStrUtil.h"
#include "vstrlist.h"
#include "WinUtil.h"
#include "ParseCommandLine.h"
#include "Benchmark.h"
#include "WindowInfo.h"

bool gPluginMode = false;

void MakePluginWindow(WindowInfo *win, HWND hwndParent)
{
    assert(IsWindow(hwndParent));
    assert(gPluginMode);
    win->pluginParent = hwndParent;

    long ws = GetWindowLong(win->hwndFrame, GWL_STYLE);
    ws &= ~(WS_POPUP|WS_BORDER|WS_CAPTION|WS_THICKFRAME);
    ws |= WS_CHILD;
    SetWindowLong(win->hwndFrame, GWL_STYLE, ws);

    RECT rc;
    SetParent(win->hwndFrame, hwndParent);
    GetClientRect(hwndParent, &rc);
    MoveWindow(win->hwndFrame, 0, 0, RectDx(&rc), RectDy(&rc), FALSE);
    ShowWindow(win->hwndFrame, SW_SHOW);

    // from here on, we depend on the plugin's host to resize us
    SetFocus(win->hwndFrame);
}

/* Get the name of default printer or NULL if not exists.
   The caller needs to free() the result */
static TCHAR *GetDefaultPrinterName()
{
    TCHAR buf[512];
    DWORD bufSize = dimof(buf);
    if (GetDefaultPrinter(buf, &bufSize))
        return StrCopy(buf);
    return NULL;
}

#ifdef DEBUG
static void EnumeratePrinters()
{
    PRINTER_INFO_5 *info5Arr = NULL;
    DWORD bufSize = 0, printersCount;
    BOOL fOk = EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 
        5, (LPBYTE)info5Arr, bufSize, &bufSize, &printersCount);
    if (!fOk) {
        info5Arr = (PRINTER_INFO_5 *)malloc(bufSize);
        fOk = EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 
            5, (LPBYTE)info5Arr, bufSize, &bufSize, &printersCount);
    }
    if (!info5Arr)
        return;
    assert(fOk);
    if (!fOk) return;
    printf("Printers: %ld\n", printersCount);
    for (DWORD i=0; i < printersCount; i++) {
        const TCHAR *printerName = info5Arr[i].pPrinterName;
        const TCHAR *printerPort = info5Arr[i].pPortName;
        bool fDefault = false;
        if (info5Arr[i].Attributes & PRINTER_ATTRIBUTE_DEFAULT)
            fDefault = true;
        _tprintf(_T("Name: %s, port: %s, default: %d\n"), printerName, printerPort, (int)fDefault);
    }
    TCHAR buf[512];
    bufSize = dimof(buf);
    fOk = GetDefaultPrinter(buf, &bufSize);
    if (!fOk) {
        if (ERROR_FILE_NOT_FOUND == GetLastError())
            printf("No default printer\n");
    }
    free(info5Arr);
}
#endif

/* Parse 'txt' as hex color and return the result in 'destColor' */
static void ParseColor(int *destColor, const TCHAR* txt)
{
    if (!destColor)
        return;
    if (tstr_startswith(txt, _T("0x")))
        txt += 2;
    else if (tstr_startswith(txt, _T("#")))
        txt += 1;

    int r, g, b;
    if (_stscanf(txt, _T("%2x%2x%2x"), &r, &g, &b) == 3)
        *destColor = RGB(r, g, b);
}

// compares two strings ignoring case and whitespace
static bool tstr_ieqs(const TCHAR *s1, const TCHAR *s2)
{
    while (*s1 && *s2) {
        // skip whitespace
        for (; _istspace(*s1); s1++);
        for (; _istspace(*s2); s2++);

        if (_totlower(*s1) != _totlower(*s2))
            return false;
        if (*s1) { s1++; s2++; }
    }

    return !*s1 && !*s2;
}

#define IS_STR_ENUM(enumName) \
    if (tstr_ieqs(txt, _T(enumName##_STR))) { \
        *mode = enumName; \
        return; \
    }

// -view [continuous][singlepage|facing|bookview]
static void ParseViewMode(DisplayMode *mode, const TCHAR *txt)
{
    IS_STR_ENUM(DM_SINGLE_PAGE);
    IS_STR_ENUM(DM_CONTINUOUS);
    IS_STR_ENUM(DM_FACING);
    IS_STR_ENUM(DM_CONTINUOUS_FACING);
    IS_STR_ENUM(DM_BOOK_VIEW);
    IS_STR_ENUM(DM_CONTINUOUS_BOOK_VIEW);
    if (tstr_ieqs(txt, _T("continuous single page"))) {
        *mode = DM_CONTINUOUS;
        return;
    }
}

// -zoom [fitwidth|fitpage|fitcontent|100%] (with 100% meaning actual size)
static void ParseZoomValue(float *zoom, const TCHAR *txt)
{
    if (tstr_ieqs(txt, _T("fit page")))
        *zoom = ZOOM_FIT_PAGE;
    else if (tstr_ieqs(txt, _T("fit width")))
        *zoom = ZOOM_FIT_WIDTH;
    else if (tstr_ieqs(txt, _T("fit content")))
        *zoom = ZOOM_FIT_CONTENT;
    else
        _stscanf(txt, _T("%f"), zoom);
}

static void VStrList_FromCmdLine(VStrList *strList, TCHAR *cmdLine)
{
    assert(strList && cmdLine);
    if (!strList || !cmdLine)
        return;

    for (;;) {
        TCHAR *txt = tstr_parse_possibly_quoted(&cmdLine);
        if (!txt)
            break;
        strList->Push(txt);
    }
}

/* parse argument list. we assume that all unrecognized arguments are PDF file names. */
void CommandLineInfo::ParseCommandLine(TCHAR *cmdLine)
{
    VStrList argList;
    VStrList_FromCmdLine(&argList, cmdLine);
    size_t argCount = argList.Count();

#define is_arg(txt) tstr_ieq(_T(txt), argument)
#define is_arg_with_param(txt) (is_arg(txt) && param != NULL)

    for (size_t n = 1; n < argCount; n++) {
        TCHAR *argument = argList[n], *param = NULL;
        if (n < argCount - 1)
            param = argList[n + 1];

        if (is_arg("-register-for-pdf")) {
            this->makeDefault = true;
            this->exitImmediately = true;
            return;
        }
        else if (is_arg("-print-to-default")) {
            TCHAR *printerName = GetDefaultPrinterName();
            if (printerName) {
                this->SetPrinterName(printerName);
                free(printerName);
            }
        }
        else if (is_arg_with_param("-print-to")) {
            this->SetPrinterName(argList[++n]);
        }
        else if (is_arg("-exit-on-print")) {
            this->exitOnPrint = true;
        }
        else if (is_arg("-print-dialog")) {
            this->printDialog = true;
        }
        else if (is_arg_with_param("-bgcolor") || is_arg_with_param("-bg-color")) {
            // -bgcolor is for backwards compat (was used pre-1.3)
            // -bg-color is for consitency
            ParseColor(&this->bgColor, argList[++n]);
        }
        else if (is_arg_with_param("-inverse-search")) {
            this->SetInverseSearchCmdLine(argList[++n]);
        }
        else if (is_arg_with_param("-fwdsearch-offset")) {
            this->fwdsearchOffset = _ttoi(argList[++n]);
        }
        else if (is_arg_with_param("-fwdsearch-width")) {
            this->fwdsearchWidth = _ttoi(argList[++n]);
        }
        else if (is_arg_with_param("-fwdsearch-color")) {
            ParseColor(&this->fwdsearchColor, argList[++n]);
        }
        else if (is_arg_with_param("-fwdsearch-permanent")) {
            this->fwdsearchPermanent = _ttoi(argList[++n]);
        }
        else if (is_arg("-esc-to-exit")) {
            this->escToExit = TRUE;
        }
        else if (is_arg("-reuse-instance")) {
            // find the window handle of a running instance of SumatraPDF
            // TODO: there should be a mutex here to reduce possibility of
            // race condition and having more than one copy launch because
            // FindWindow() in one process is called before a window is created
            // in another process
            this->reuseInstance = (FindWindow(FRAME_CLASS_NAME, 0) != NULL);
        }
        else if (is_arg_with_param("-lang")) {
            this->SetLang(argList[++n]);
        }
        else if (is_arg_with_param("-nameddest") || is_arg_with_param("-named-dest")) {
            // -nameddest is for backwards compat (was used pre-1.3)
            // -named-dest is for consitency
            this->SetDestName(argList[++n]);
        }
        else if (is_arg_with_param("-page")) {
            this->pageNumber = _ttoi(argList[++n]);
        }
        else if (is_arg("-restrict")) {
            this->restrictedUse = true;
        }
        else if (is_arg("-invertcolors") || is_arg("-invert-colors")) {
            // -invertcolors is for backwards compat (was used pre-1.3)
            // -invert-colors is for consitency
            this->invertColors = TRUE;
        }
        else if (is_arg("-presentation")) {
            this->enterPresentation = true;
        }
        else if (is_arg("-fullscreen")) {
            this->enterFullscreen = true;
        }
        else if (is_arg_with_param("-view")) {
            ParseViewMode(&this->startView, argList[++n]);
        }
        else if (is_arg_with_param("-zoom")) {
            ParseZoomValue(&this->startZoom, argList[++n]);
        }
        else if (is_arg("-console")) {
            this->showConsole = true;
        }
        else if (is_arg_with_param("-plugin")) {
            // the argument is a (nummeric) window handle to
            // become the parent of a frameless SumatraPDF
            // (used e.g. for embedding it into a browser plugin)
            this->hwndPluginParent = (HWND)_ttol(argList[++n]);
        }
        else if (is_arg_with_param("-bench")) {
            TCHAR *s = StrCopy(argList[++n]);
            this->filesToBenchmark.Push(s);
            s = NULL;
            if ((n + 1 < argCount) && IsBenchPagesInfo(argList[n+1]))
                s = StrCopy(argList[++n]);
            this->filesToBenchmark.Push(s);
            this->exitImmediately = true;
        }
#ifdef DEBUG
        else if (is_arg("-enum-printers")) {
            EnumeratePrinters();
            /* this is for testing only, exit immediately */
            this->exitImmediately = true;
            return;
        }
#endif
        else {
            // Remember this argument as a filename to open
            TCHAR *filepath = NULL;
            if (tstr_endswithi(argList[n], _T(".lnk")))
                filepath = ResolveLnk(argList[n]);
            if (!filepath)
                filepath = StrCopy(argList[n]);
            this->fileNames.Push(filepath);
        }
    }
#undef is_arg
#undef is_arg_with_param
}
