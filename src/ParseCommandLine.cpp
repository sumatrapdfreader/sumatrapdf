#include "SumatraPDF.h"
#include "base_util.h"
#include "tstr_util.h"
#include "vstrlist.h"
#include "WinUtil.hpp"
#include "ParseCommandLine.h"

/* Get the name of default printer or NULL if not exists.
   The caller needs to free() the result */
TCHAR *GetDefaultPrinterName()
{
    TCHAR buf[512];
    DWORD bufSize = dimof(buf);
    if (GetDefaultPrinter(buf, &bufSize))
        return tstr_dup(buf);
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
    if(!destColor)
        return;
    if (tstr_startswith(txt, _T("0x")))
        txt += 2;
    else if (tstr_startswith(txt, _T("#")))
        txt += 1;
    int r = hex_tstr_decode_byte(&txt);
    if (-1 == r)
        return;
    int g = hex_tstr_decode_byte(&txt);
    if (-1 == g)
        return;
    int b = hex_tstr_decode_byte(&txt);
    if (-1 == b)
        return;
    if (*txt)
        return;
    *destColor = RGB(r,g,b);
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
        strList->push_back(txt);
    }
}

/* parse argument list. we assume that all unrecognized arguments are PDF file names. */
void ParseCommandLine(CommandLineInfo& i, TCHAR *cmdLine)
{
    VStrList argList;
    VStrList_FromCmdLine(&argList, cmdLine);
    size_t argCount = argList.size();

#define is_arg(txt) tstr_ieq(_T(txt), argList[n])
#define is_arg_with_param(txt) (is_arg(txt) && n < argCount - 1)

    for (size_t n = 1; n < argCount; n++) {
        if (is_arg("-register-for-pdf")) {
            i.makeDefault = true;
            i.exitImmediately = true;
            return;
        }
        else if (is_arg("-exit-on-print")) {
            i.exitOnPrint = true;
        }
        else if (is_arg("-print-to-default")) {
            TCHAR *printerName = GetDefaultPrinterName();
            if (printerName) {
                i.SetPrinterName(printerName);
                free(printerName);
            }
        }
        else if (is_arg_with_param("-print-to")) {
            i.SetPrinterName(argList[++n]);
        }
        else if (is_arg("-print-dialog")) {
            i.printDialog = true;
        }
        else if (is_arg_with_param("-bgcolor") || is_arg_with_param("-bg-color")) {
            // -bgcolor is for backwards compat (was used pre-1.3)
            // -bg-color is for consitency
            ParseColor(&i.bgColor, argList[++n]);
        }
        else if (is_arg_with_param("-inverse-search")) {
            i.SetInverseSearchCmdLine(argList[++n]);
        }
        else if (is_arg_with_param("-fwdsearch-offset")) {
            i.fwdsearchOffset = _ttoi(argList[++n]);
        }
        else if (is_arg_with_param("-fwdsearch-width")) {
            i.fwdsearchWidth = _ttoi(argList[++n]);
        }
        else if (is_arg_with_param("-fwdsearch-color")) {
            ParseColor(&i.fwdsearchColor, argList[++n]);
        }
        else if (is_arg_with_param("-fwdsearch-permanent")) {
            i.fwdsearchPermanent = _ttoi(argList[++n]);
        }
        else if (is_arg("-esc-to-exit")) {
            i.escToExit = TRUE;
        }
        else if (is_arg("-reuse-instance")) {
            // find the window handle of a running instance of SumatraPDF
            // TODO: there should be a mutex here to reduce possibility of
            // race condition and having more than one copy launch because
            // FindWindow() in one process is called before a window is created
            // in another process
            i.reuseInstance = (FindWindow(FRAME_CLASS_NAME, 0) != NULL);
        }
        else if (is_arg_with_param("-lang")) {
            i.SetLang(argList[++n]);
        }
        else if (is_arg_with_param("-nameddest") || is_arg_with_param("-named-dest")) {
            // -nameddest is for backwards compat (was used pre-1.3)
            // -named-dest is for consitency
            i.SetDestName(argList[++n]);
        }
        else if (is_arg_with_param("-page")) {
            i.pageNumber = _ttoi(argList[++n]);
        }
        else if (is_arg("-restrict")) {
            i.restrictedUse = true;
        }
        else if (is_arg_with_param("-title")) {
            i.SetNewWindowTitle(argList[++n]);
        }
        else if (is_arg("-invertcolors") || is_arg("-invert-colors")) {
            // -invertcolors is for backwards compat (was used pre-1.3)
            // -invert-colors is for consitency
            i.invertColors = TRUE;
        }
        else if (is_arg("-presentation")) {
            i.enterPresentation = true;
        }
        else if (is_arg("-console")) {
            RedirectIOToConsole();
        }
        else if (is_arg_with_param("-plugin")) {
            i.hwndPluginParent = (HWND)_ttoi(argList[++n]);
        }
#ifdef BUILD_RM_VERSION
        else if (is_arg("-delete-these-on-close")) {
            i.deleteFilesOnClose = true;
        }
#endif
#ifdef DEBUG
        else if (is_arg("-enum-printers")) {
            EnumeratePrinters();
            /* this is for testing only, exit immediately */
            i.exitImmediately = true;
            return;
        }
#endif
        else {
            // Remember this argument as a filename to open
            TCHAR *filepath = NULL;
            if (tstr_endswithi(argList[n], _T(".lnk")))
                filepath = ResolveLnk(argList[n]);
            if (!filepath)
                filepath = tstr_dup(argList[n]);
            i.fileNames.push_back(filepath);
        }
    }
}
