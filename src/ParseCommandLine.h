/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class CommandLineInfo {
public:
    WStrVec     fileNames;
    // pathsToBenchmark contain 2 strings per each file to benchmark:
    // - name of the file to benchmark
    // - optional (nullptr if not available) string that represents which pages
    //   to benchmark. It can also be a string "loadonly" which means we'll
    //   only benchmark loading of the catalog
    WStrVec     pathsToBenchmark;
    bool        makeDefault;
    bool        exitWhenDone;
    bool        printDialog;
    ScopedMem<WCHAR> printerName;
    ScopedMem<WCHAR> printSettings;
    ScopedMem<WCHAR> forwardSearchOrigin;
    int         forwardSearchLine;
    bool        reuseDdeInstance;
    ScopedMem<WCHAR> destName;
    int         pageNumber;
    bool        restrictedUse;
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
    ScopedMem<WCHAR> stressTestFilter; // nullptr is equivalent to "*" (i.e. all files)
    ScopedMem<WCHAR> stressTestRanges;
    int         stressTestCycles;
    int         stressParallelCount;
    bool        stressRandomizeFiles;

    bool        crashOnOpen;

    // deprecated flags
    ScopedMem<char> lang;

    CommandLineInfo() : makeDefault(false), exitWhenDone(false), printDialog(false),
        printerName(nullptr), printSettings(nullptr),
        reuseDdeInstance(false), lang(nullptr),
        destName(nullptr), pageNumber(-1),
        restrictedUse(false), pluginURL(nullptr),
        enterPresentation(false), enterFullScreen(false), hwndPluginParent(nullptr),
        startView(DM_AUTOMATIC), startZoom(INVALID_ZOOM), startScroll(PointI(-1, -1)),
        showConsole(false), exitImmediately(false), silent(false),
        forwardSearchOrigin(nullptr), forwardSearchLine(0),
        stressTestPath(nullptr), stressTestFilter(nullptr),
        stressTestRanges(nullptr), stressTestCycles(1), stressParallelCount(1),
        stressRandomizeFiles(false),
        crashOnOpen(false)
    { }

    ~CommandLineInfo() { }

    void ParseCommandLine(const WCHAR *cmdLine);
};
