/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ParseCommandLine_h
#define ParseCommandLine_h

#include "DisplayState.h"

class CommandLineInfo {
public:
    WStrVec     fileNames;
    // pathsToBenchmark contain 2 strings per each file to benchmark:
    // - name of the file to benchmark
    // - optional (NULL if not available) string that represents which pages
    //   to benchmark. It can also be a string "loadonly" which means we'll
    //   only benchmark loading of the catalog
    WStrVec     pathsToBenchmark;
    bool        makeDefault;
    bool        exitWhenDone;
    bool        printDialog;
    ScopedMem<WCHAR> printerName;
    ScopedMem<WCHAR> printSettings;
    ScopedMem<WCHAR> inverseSearchCmdLine;
    ScopedMem<WCHAR> forwardSearchOrigin;
    int         forwardSearchLine;
    bool        reuseDdeInstance;
    ScopedMem<WCHAR> destName;
    int         pageNumber;
    bool        restrictedUse;
    bool        invertColors;
    bool        enterPresentation;
    bool        enterFullScreen;
    DisplayMode startView;
    float       startZoom;
    PointI      startScroll;
    bool        showConsole;
    HWND        hwndPluginParent;
    ScopedMem<WCHAR> pluginURL;
    bool        exitImmediately;
    bool        silent;

    // stress-testing related
    ScopedMem<WCHAR> stressTestPath;
    ScopedMem<WCHAR> stressTestFilter; // NULL is equivalent to "*" (i.e. all files)
    ScopedMem<WCHAR> stressTestRanges;
    int         stressTestCycles;
    int         stressParallelCount;
    bool        stressRandomizeFiles;

    bool        crashOnOpen;

    // deprecated flags
    bool        escToExit;
    COLORREF    bgColor;
    ScopedMem<char> lang;
    COLORREF    textColor;
    COLORREF    backgroundColor;
    ForwardSearch forwardSearch;
    bool        cbxMangaMode;

    CommandLineInfo() : makeDefault(false), exitWhenDone(false), printDialog(false),
        printerName(NULL), printSettings(NULL), bgColor((COLORREF)-1),
        escToExit(false), reuseDdeInstance(false), lang(NULL),
        destName(NULL), pageNumber(-1), inverseSearchCmdLine(NULL),
        restrictedUse(false), pluginURL(NULL), invertColors(false),
        enterPresentation(false), enterFullScreen(false), hwndPluginParent(NULL),
        startView(DM_AUTOMATIC), startZoom(INVALID_ZOOM), startScroll(PointI(-1, -1)),
        showConsole(false), exitImmediately(false), silent(false), cbxMangaMode(false),
        forwardSearchOrigin(NULL), forwardSearchLine(0),
        stressTestPath(NULL), stressTestFilter(NULL),
        stressTestRanges(NULL), stressTestCycles(1), stressParallelCount(1),
        stressRandomizeFiles(false),
        crashOnOpen(false)
    {
        textColor = RGB(0, 0, 0); // black
        backgroundColor = RGB(0xFF, 0xFF, 0xFF); // white
        forwardSearch.highlightOffset = 0;
        forwardSearch.highlightWidth = 0;
        forwardSearch.highlightColor = (COLORREF)-1;
        forwardSearch.highlightPermanent = false;
    }

    ~CommandLineInfo() { }

    void ParseCommandLine(WCHAR *cmdLine);
};

#endif
