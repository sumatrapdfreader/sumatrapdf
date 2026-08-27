/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct PageRange {
    int start = 1;
    // end == INT_MAX means to the last page
    int end = INT_MAX;
};

struct FileArgs {
    Str origPath;
    Str cleanPath;

    // page=%d
    int pageNumber = 0;
    // dest=%s
    Str destName;
    // search=%s
    Str search;

    // annotatt=%d
    int annotAttObjNum = 0;

    // attachno=%d
    int attachmentNo = 0;

    ~FileArgs();
};

FileArgs* ParseFileArgs(Str path);

struct Flags {
    HWND hwndPluginParent = nullptr;
    Str printerName;
    Str printSettings;
    Str forwardSearchOrigin;
    Str namedDest;
    Str pluginURL;
    Str appdataDir;
    Str inverseSearchCmdLine;
    Str search;
    Str password;
    Str stressTestPath;
    // empty is equivalent to "*" (i.e. all files)
    Str stressTestFilter;
    Str stressTestRanges;
    Str controlPipeName; // -dbg-control <named-pipe>
    Str upgradeFrom;
    Str dde;
    Str lang;
    Str installDir;
    Str logFile;
    Str updateSelfTo;
    Str deleteFile;
    StrVec fileNames;
    // pathsToBenchmark contain 2 strings per each file to benchmark:
    // - name of the file to benchmark
    // - optional (nullptr if not available) string that represents which pages
    //   to benchmark. It can also be a string "loadonly" which means we'll
    //   only benchmark loading of the catalog
    StrVec pathsToBenchmark;
    StrVec globalPrefArgs;
    int forwardSearchLine = 0;
    int pageNumber = -1;
    DisplayMode startView = DisplayMode::Automatic;
    float startZoom = kInvalidZoom;
    int stressTestCycles = 1;
    int stressParallelCount = 1;
    int stressTestMax = 0;
    int maxFiles = 0;
    int testPageNo = 0;
    int sleepMs = 0;
    Point startScroll = {-1, -1};
    // -window-pos <width>x<height>@<x>x<y>: open every window exactly there,
    // e.g. 960x540@960x0. Empty unless given. Meant for automated tests, where
    // a window a quarter of the screen renders and captures four times faster
    Rect windowPos;

    bool exitWhenDone = false;
    bool printDialog = false;
    bool reuseDdeInstance = false;
    bool restrictedUse = false;
    bool enterPresentation = false;
    bool enterFullScreen = false;
    bool showConsole = false;
    bool exitImmediately = false;
    // installer: doesn't show any UI
    bool silent = false;
    // installer: starts the install immediately and launches the app at end
    bool fastInstall = false;
    bool invertColors = false;
    bool regress = false;
    bool tester = false;
    // -new-window: each file in its own new window (not a tab of an existing one)
    bool inNewWindow = false;
    // -new-window-tabs: one new window, all files as tabs in that window
    bool inNewWindowTabs = false;
    bool stressRandomizeFiles = false;
    // -for-testing: for ad-hoc testing by humans or agents. Always starts
    // a new instance, doesn't restore session, doesn't save settings
    bool forTesting = false;
    // -quicklook: chrome-less always-on-top preview window (Explorer Space)
    bool quickLook = false;
    // -quicklook-agent: hidden Space-bar hook for Explorer, no UI
    bool quickLookAgent = false;
    bool testRenderPage = false;
    bool testExtractPage = false;
    bool testApp = false;
    bool testPlugin = false;
    bool testPreview = false;
    bool engineDump = false; // -engine-dump
    bool dumpExif = false;   // -dump-exif
    bool dumpChm = false;    // -dump-chm
    bool unitTests = false;  // -unit-tests (debug builds only)
    bool showPrintersDialog = false;
    bool crashOnOpen = false;
    // related to installer
    bool showHelp = false;
    bool install = false;
    bool uninstall = false;
    bool withFilter = false;
    bool withPreview = false;
    bool justExtractFiles = false;
    bool log = false;
    bool allUsers = false;
    bool runInstallNow = false;
    bool storeInstaller = false;

    Flags() = default;
    ~Flags() = default;
};

#if OS_WIN
void ParseFlags(Arena* a, WStr cmdLine, Flags&, Str toolNames = {});
#endif
void ShowPrintersDialog(bool consoleOnly = false);

bool IsValidPageRange(Str ranges);
bool IsBenchPagesInfo(Str s);
bool ParsePageRanges(Str ranges, Vec<PageRange>& result);
