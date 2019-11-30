/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct PageRange {
    PageRange() : start(1), end(INT_MAX) {
    }
    PageRange(int start, int end) : start(start), end(end) {
    }

    int start, end; // end == INT_MAX means to the last page
};

class CommandLineInfo {
  public:
    WStrVec fileNames;
    // pathsToBenchmark contain 2 strings per each file to benchmark:
    // - name of the file to benchmark
    // - optional (nullptr if not available) string that represents which pages
    //   to benchmark. It can also be a string "loadonly" which means we'll
    //   only benchmark loading of the catalog
    WStrVec pathsToBenchmark;
    bool makeDefault = false;
    bool exitWhenDone = false;
    bool printDialog = false;
    AutoFreeW printerName;
    AutoFreeW printSettings;
    AutoFreeW forwardSearchOrigin;
    int forwardSearchLine = 0;
    bool reuseDdeInstance = false;
    AutoFreeW destName;
    int pageNumber = -1;
    bool restrictedUse = false;
    bool enterPresentation = false;
    bool enterFullScreen = false;
    DisplayMode startView = DM_AUTOMATIC;
    float startZoom = INVALID_ZOOM;
    PointI startScroll{-1,-1};
    bool showConsole = false;
    HWND hwndPluginParent = nullptr;
    AutoFreeW pluginURL;
    bool exitImmediately = false;
    bool silent = false;
    AutoFreeW appdataDir;
    AutoFreeW inverseSearchCmdLine;
    bool invertColors = false;

    // stress-testing related
    AutoFreeW stressTestPath;
    AutoFreeW stressTestFilter; // nullptr is equivalent to "*" (i.e. all files)
    AutoFreeW stressTestRanges;
    int stressTestCycles = 1;
    int stressParallelCount = 1;
    bool stressRandomizeFiles = false;

    // related to testing
    bool testRenderPage = false;
    bool testExtractPage = false;
    int testPageNo = 0;

    bool crashOnOpen = false;

    // deprecated flags
    AutoFree lang;
    WStrVec globalPrefArgs;

    CommandLineInfo() = default;
    ~CommandLineInfo() = default;
};

CommandLineInfo ParseCommandLine(const WCHAR* cmdLine);

bool IsValidPageRange(const WCHAR* ranges);
bool IsBenchPagesInfo(const WCHAR* s);
bool ParsePageRanges(const WCHAR* ranges, Vec<PageRange>& result);
