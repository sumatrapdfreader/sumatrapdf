/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct PageRange {
    int start = 1;
    // end == INT_MAX means to the last page
    int end = INT_MAX;
};

struct FileArgs {
    const char* origPath = nullptr;
    const char* cleanPath = nullptr;

    // page=%d
    int pageNumber = 0;
    // dest=%s
    const char* destName = nullptr;
    // search=%s
    const char* search = nullptr;

    // annotatt=%d
    int annotAttObjNum = 0;

    // attachno=%d
    int attachmentNo = 0;

    ~FileArgs();
};

FileArgs* ParseFileArgs(const char* path);

struct Flags {
    StrVec fileNames;
    // pathsToBenchmark contain 2 strings per each file to benchmark:
    // - name of the file to benchmark
    // - optional (nullptr if not available) string that represents which pages
    //   to benchmark. It can also be a string "loadonly" which means we'll
    //   only benchmark loading of the catalog
    StrVec pathsToBenchmark;
    bool exitWhenDone = false;
    bool printDialog = false;
    char* printerName = nullptr;
    char* printSettings = nullptr;
    char* forwardSearchOrigin = nullptr;
    int forwardSearchLine = 0;
    bool reuseDdeInstance = false;
    char* destName = nullptr;
    int pageNumber = -1;
    bool restrictedUse = false;
    bool enterPresentation = false;
    bool enterFullScreen = false;
    DisplayMode startView = DisplayMode::Automatic;
    float startZoom = kInvalidZoom;
    Point startScroll = {-1, -1};
    bool showConsole = false;
    HWND hwndPluginParent = nullptr;
    char* pluginURL = nullptr;
    bool exitImmediately = false;
    // installer: doesn't show any UI
    bool silent = false;
    // installer: starts the install immediately and launches the app at end
    bool fastInstall = false;
    char* appdataDir = nullptr;
    char* inverseSearchCmdLine = nullptr;
    bool invertColors = false;
    bool regress = false;
    bool tester = false;
    // -new-window, if true and we're using tabs, opens
    // the document in new window
    bool inNewWindow = false;
    char* search = nullptr;

    // stress-testing related
    char* stressTestPath = nullptr;
    // nullptr is equivalent to "*" (i.e. all files)
    char* stressTestFilter = nullptr;
    char* stressTestRanges = nullptr;
    int stressTestCycles = 1;
    int stressParallelCount = 1;
    bool stressRandomizeFiles = false;
    int stressTestMax = 0;
    int maxFiles = 0;

    // related to testing
    bool testRenderPage = false;
    bool testExtractPage = false;
    int testPageNo = 0;
    bool testApp = false;
    char* dde = nullptr;
    bool engineDump = false; // -engine-dump

    bool crashOnOpen = false;

    // deprecated flags
    char* lang = nullptr;
    StrVec globalPrefArgs;

    // related to installer
    bool showHelp = false;
    char* installDir = nullptr;
    bool install = false;
    bool uninstall = false;
    bool withFilter = false;
    bool withPreview = false;
    bool justExtractFiles = false;
    bool log = false;
    bool allUsers = false;
    bool runInstallNow = false;

    // for internal use
    char* updateSelfTo = nullptr;
    char* deleteFile = nullptr;

    // for some commands, will sleep for sleepMs milliseconds
    // before proceeding
    int sleepMs = 0;

    // for preview pipe mode (used by previewer DLL)
    char* previewPipeName = nullptr;

    // for ifilter pipe mode (used by ifilter DLL)
    char* ifilterPipeName = nullptr;

    // for testing preview pipe functionality
    char* testPreviewPipePath = nullptr;

    Flags() = default;
    ~Flags();
};

void ParseFlags(const WCHAR* cmdLine, Flags&);

bool IsValidPageRange(const char* ranges);
bool IsBenchPagesInfo(const char* s);
bool ParsePageRanges(const char* ranges, Vec<PageRange>& result);
