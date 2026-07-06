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
    StrVec fileNames;
    // pathsToBenchmark contain 2 strings per each file to benchmark:
    // - name of the file to benchmark
    // - optional (nullptr if not available) string that represents which pages
    //   to benchmark. It can also be a string "loadonly" which means we'll
    //   only benchmark loading of the catalog
    StrVec pathsToBenchmark;
    bool exitWhenDone = false;
    bool printDialog = false;
    Str printerName;
    Str printSettings;
    Str forwardSearchOrigin;
    int forwardSearchLine = 0;
    bool reuseDdeInstance = false;
    Str namedDest;
    int pageNumber = -1;
    bool restrictedUse = false;
    bool enterPresentation = false;
    bool enterFullScreen = false;
    DisplayMode startView = DisplayMode::Automatic;
    float startZoom = kInvalidZoom;
    Point startScroll = {-1, -1};
    bool showConsole = false;
    HWND hwndPluginParent = nullptr;
    Str pluginURL;
    bool exitImmediately = false;
    // installer: doesn't show any UI
    bool silent = false;
    // installer: starts the install immediately and launches the app at end
    bool fastInstall = false;
    Str appdataDir;
    Str inverseSearchCmdLine;
    bool invertColors = false;
    bool regress = false;
    bool tester = false;
    // -new-window, if true and we're using tabs, opens
    // the document in new window
    bool inNewWindow = false;
    Str search;
    Str password;

    // stress-testing related
    Str stressTestPath;
    // empty is equivalent to "*" (i.e. all files)
    Str stressTestFilter;
    Str stressTestRanges;
    int stressTestCycles = 1;
    int stressParallelCount = 1;
    bool stressRandomizeFiles = false;
    int stressTestMax = 0;
    int maxFiles = 0;

    // related to testing
    // -for-testing: for ad-hoc testing by humans or agents. Always starts
    // a new instance, doesn't restore session, doesn't save settings
    bool forTesting = false;
    Str controlPipeName; // -dbg-control <named-pipe>
    bool testRenderPage = false;
    bool testExtractPage = false;
    int testPageNo = 0;
    bool testApp = false;
    bool testPlugin = false;
    bool testPreview = false;
    Str upgradeFrom;
    Str dde;
    bool engineDump = false; // -engine-dump
    bool dumpExif = false;   // -dump-exif
    bool dumpChm = false;    // -dump-chm
    bool unitTests = false;  // -unit-tests (debug builds only)
    bool showPrintersDialog = false;

    bool crashOnOpen = false;

    // deprecated flags
    Str lang;
    StrVec globalPrefArgs;

    // related to installer
    bool showHelp = false;
    Str installDir;
    bool install = false;
    bool uninstall = false;
    bool withFilter = false;
    bool withPreview = false;
    bool justExtractFiles = false;
    bool log = false;
    Str logFile;
    bool allUsers = false;
    bool runInstallNow = false;
    bool storeInstaller = false;

    // for internal use
    Str updateSelfTo;
    Str deleteFile;

    // for some commands, will sleep for sleepMs milliseconds
    // before proceeding
    int sleepMs = 0;

    Flags() = default;
    ~Flags() = default;
};

#if OS_WIN
void ParseFlags(Arena* a, WStr cmdLine, Flags&, Str toolNames = {});
#endif
void ShowPrintersDialog();

bool IsValidPageRange(Str ranges);
bool IsBenchPagesInfo(Str s);
bool ParsePageRanges(Str ranges, Vec<PageRange>& result);
