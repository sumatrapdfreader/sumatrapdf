/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ParseCommandLine_h
#define ParseCommandLine_h

#include "DisplayState.h"
#include "Vec.h"

class CommandLineInfo {
public:
    StrVec      fileNames;
    // filesToBenchmark contain 2 strings per each file to benchmark:
    // - name of the file to benchmark
    // - optional (NULL if not available) string that represents which pages
    //   to benchmark. It can also be a string "loadonly" which means we'll
    //   only benchmark loading of the catalog
    StrVec      filesToBenchmark;
    bool        makeDefault;
    bool        exitOnPrint;
    bool        printDialog;
    TCHAR *     printerName;
    int         bgColor;
    TCHAR *     inverseSearchCmdLine;
    int         fwdsearchOffset;
    int         fwdsearchWidth;
    int         fwdsearchColor;
    bool        fwdsearchPermanent;
    bool        escToExit;
    bool        reuseInstance;
    char *      lang;
    TCHAR *     destName;
    int         pageNumber;
    bool        restrictedUse;
    bool        invertColors;
    bool        enterPresentation;
    bool        enterFullscreen;
    DisplayMode startView;
    float       startZoom;
    PointI      startScroll;
    bool        showConsole;
    TCHAR *     consoleFile;
    HWND        hwndPluginParent;
    bool        exitImmediately;
    bool        silent;

    // stress-testing related
    TCHAR *     stressTestPath;
    TCHAR *     stressTestRanges;
    int         stressTestCycles;
    bool        disableDjvu;

    CommandLineInfo() : makeDefault(false), exitOnPrint(false), printDialog(false),
        printerName(NULL), bgColor(-1), inverseSearchCmdLine(NULL),
        fwdsearchOffset(-1), fwdsearchWidth(-1), fwdsearchColor(-1),
        fwdsearchPermanent(FALSE), escToExit(FALSE),
        reuseInstance(false), lang(NULL), destName(NULL), pageNumber(-1),
        restrictedUse(false), invertColors(FALSE),
        enterPresentation(false), enterFullscreen(false), hwndPluginParent(NULL),
        startView(DM_AUTOMATIC), startZoom(INVALID_ZOOM), startScroll(PointI(-1, -1)),
        showConsole(false), exitImmediately(false), silent(false),
        stressTestPath(NULL), stressTestRanges(NULL), stressTestCycles(1),
        consoleFile(NULL), disableDjvu(false)
    { }

    ~CommandLineInfo() {
        free(printerName);
        free(inverseSearchCmdLine);
        free(lang);
        free(destName);
        free(stressTestPath);
        free(stressTestRanges);
        free(consoleFile);
    }

    void ParseCommandLine(TCHAR *cmdLine);
};

#endif
