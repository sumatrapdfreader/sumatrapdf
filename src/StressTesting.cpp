/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/DirIter.h"
#include "utils/FileUtil.h"
#include "utils/GuessFileType.h"
#include "utils/HtmlParserLookup.h"
#include "utils/Timer.h"
#include "utils/WinUtil.h"
#include "utils/StrQueue.h"

#include "wingui/UIModels.h"

#include "Settings.h"
#include "DocProperties.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "GlobalPrefs.h"
#include "ChmModel.h"
#include "DisplayModel.h"
#include "RenderCache.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Flags.h"
#include "SearchAndDDE.h"
#include "StressTesting.h"

#include "utils/Log.h"

#define FIRST_STRESS_TIMER_ID 101

static bool gIsStressTesting = false;
static int gCurrStressTimerId = FIRST_STRESS_TIMER_ID;
static Kind kNotifStressTestBenchmark = "stressTestBenchmark";
static Kind kNotifStressTestSummary = "stressTestSummary";

bool IsStressTesting() {
    return gIsStressTesting;
}

static bool IsInRange(Vec<PageRange>& ranges, int pageNo) {
    for (auto&& range : ranges) {
        if (range.start <= pageNo && pageNo <= range.end) {
            return true;
        }
    }
    return false;
}

static bool IsFullRange(Vec<PageRange>& ranges) {
    if (ranges.size() != 1) {
        return false;
    }
    auto&& range = ranges[0];
    bool isFull = range.start == 1 && range.end == INT_MAX;
    return isFull;
}

static void BenchLoadRender(EngineBase* engine, int pagenum) {
    auto t = TimeGet();
    bool ok = engine->BenchLoadPage(pagenum);

    if (!ok) {
        logf("Error: failed to load page %d\n", pagenum);
        return;
    }
    double timeMs = TimeSinceInMs(t);
    logf("pageload   %3d: %.2f ms\n", pagenum, timeMs);

    t = TimeGet();
    RenderPageArgs args(pagenum, 1.0, 0);
    RenderedBitmap* rendered = engine->RenderPage(args);

    if (!rendered) {
        logf("Error: failed to render page %d\n", pagenum);
        return;
    }
    delete rendered;
    timeMs = TimeSinceInMs(t);
    logf("pagerender %3d: %.2f ms\n", pagenum, timeMs);
}

static void BenchChmLoadOnly(const char* filePath) {
    auto total = TimeGet();
    logf("Starting: %s\n", filePath);

    auto t = TimeGet();
    ChmModel* chmModel = ChmModel::Create(filePath, nullptr);
    if (!chmModel) {
        logf("Error: failed to load %s\n", filePath);
        return;
    }

    double timeMs = TimeSinceInMs(t);
    logf("load: %.2f ms\n", timeMs);

    delete chmModel;

    logf("Finished (in %.2f ms): %s\n", TimeSinceInMs(total), filePath);
}

static void BenchFile(const char* path, const char* pagesSpec) {
    if (!file::Exists(path)) {
        return;
    }

    // ad-hoc: if enabled times layout instead of rendering and does layout
    // using all text rendering methods, so that we can compare and find
    // docs that take a long time to load

    Kind kind = GuessFileType(path, true);
    if (!kind) {
        return;
    }

    if (ChmModel::IsSupportedFileType(kind) && !gGlobalPrefs->chmUI.useFixedPageUI) {
        BenchChmLoadOnly(path);
        return;
    }

    auto total = TimeGet();
    logf("Starting: %s\n", path);

    auto t = TimeGet();
    EngineBase* engine = CreateEngineFromFile(path, nullptr, true);
    if (!engine) {
        logf("Error: failed to load %s\n", path);
        return;
    }

    double timeMs = TimeSinceInMs(t);
    logf("load: %.2f ms\n", timeMs);
    int pages = engine->PageCount();
    logf("page count: %d\n", pages);

    if (!pagesSpec) {
        for (int i = 1; i <= pages; i++) {
            BenchLoadRender(engine, i);
        }
    }

    ReportIf(pagesSpec && !IsBenchPagesInfo(pagesSpec));
    Vec<PageRange> ranges;
    if (ParsePageRanges(pagesSpec, ranges)) {
        for (size_t i = 0; i < ranges.size(); i++) {
            for (int j = ranges.at(i).start; j <= ranges.at(i).end; j++) {
                if (1 <= j && j <= pages) {
                    BenchLoadRender(engine, j);
                }
            }
        }
    }

    SafeEngineRelease(&engine);

    logf("Finished (in %.2f ms): %s\n", TimeSinceInMs(total), path);
}

static bool IsFileToBench(const char* path) {
    Kind kind = GuessFileType(path, true);
    if (IsSupportedFileType(kind, true)) {
        return true;
    }
    if (DocIsSupportedFileType(kind)) {
        return true;
    }
    return false;
}

static void CollectFilesToBench(char* dir, StrVec& files) {
    DirIter di{dir};
    di.recurse = true;
    for (DirIterEntry* de : di) {
        auto path = de->filePath;
        if (IsFileToBench(path)) {
            files.Append(path);
        }
    }
}

static void BenchDir(char* dir) {
    StrVec files;
    CollectFilesToBench(dir, files);
    for (int i = 0; i < files.Size(); i++) {
        BenchFile(files.At(i), nullptr);
    }
}

void BenchFileOrDir(StrVec& pathsToBench) {
    int n = pathsToBench.Size() / 2;
    for (int i = 0; i < n; i++) {
        char* path = pathsToBench.At(2 * i);
        if (file::Exists(path)) {
            BenchFile(path, pathsToBench.At(2 * i + 1));
        } else if (dir::Exists(path)) {
            BenchDir(path);
        } else {
            logf("Error: file or dir %s doesn't exist", path);
        }
    }
}

static bool IsStressTestSupportedFile(const char* filePath, const char* filter) {
    if (filter && !path::Match(path::GetBaseNameTemp(filePath), filter)) {
        return false;
    }
    Kind kind = GuessFileType(filePath, false);
    if (!kind) {
        return false;
    }
    if (IsSupportedFileType(kind, true) || DocIsSupportedFileType(kind)) {
        return true;
    }
    if (!filter) {
        return false;
    }
    // sniff the file's content if it matches the filter but
    // doesn't have a known extension
    Kind kindSniffed = GuessFileType(filePath, true);
    if (!kindSniffed || kindSniffed == kind) {
        return false;
    }
    if (IsSupportedFileType(kindSniffed, true)) {
        return true;
    }
    return DocIsSupportedFileType(kindSniffed);
}

// return t1 - t2 in seconds
static int SystemTimeDiffInSecs(SYSTEMTIME& t1, SYSTEMTIME& t2) {
    FILETIME ft1, ft2;
    SystemTimeToFileTime(&t1, &ft1);
    SystemTimeToFileTime(&t2, &ft2);
    return FileTimeDiffInSecs(ft1, ft2);
}

static int SecsSinceSystemTime(SYSTEMTIME& time) {
    SYSTEMTIME currTime;
    GetSystemTime(&currTime);
    return SystemTimeDiffInSecs(currTime, time);
}

static TempStr FormatTimeTemp(int totalSecs) {
    int secs = totalSecs % 60;
    int totalMins = totalSecs / 60;
    int mins = totalMins % 60;
    int hrs = totalMins / 60;
    if (hrs > 0) {
        return str::FormatTemp("%d hrs %d mins %d secs", hrs, mins, secs);
    }
    if (mins > 0) {
        return str::FormatTemp("%d mins %d secs", mins, secs);
    }
    return str::FormatTemp("%d secs", secs);
}

static void FormatTime(int totalSecs, str::Str* s) {
    int secs = totalSecs % 60;
    int totalMins = totalSecs / 60;
    int mins = totalMins % 60;
    int hrs = totalMins / 60;
    if (hrs > 0) {
        s->AppendFmt("%d hrs %d mins %d secs", hrs, mins, secs);
    }
    if (mins > 0) {
        s->AppendFmt("%d mins %d secs", mins, secs);
    }
    s->AppendFmt("%d secs", secs);
}

static void MakeRandomSelection(MainWindow* win, int pageNo) {
    DisplayModel* dm = win->AsFixed();
    if (!dm->ValidPageNo(pageNo)) {
        pageNo = 1;
    }
    if (!dm->ValidPageNo(pageNo)) {
        return;
    }

    // try a random position in the page
    int x = rand() % 640;
    int y = rand() % 480;
    if (dm->textSelection->IsOverGlyph(pageNo, x, y)) {
        dm->textSelection->StartAt(pageNo, x, y);
        dm->textSelection->SelectUpTo(pageNo, rand() % 640, rand() % 480);
    }
}

// encapsulates the logic of getting the next file to test, so
// that we can implement different strategies
struct TestFileProvider {
    virtual ~TestFileProvider() {
    }
    // returns path of the next file to test or nullptr if done (caller needs to free() the result)
    virtual TempStr NextFile() = 0;
    virtual void Restart() = 0;
    virtual int GetFilesCount() = 0;
};

struct FilesProvider : TestFileProvider {
    StrVec files;
    int provided = 0;

    explicit FilesProvider(const char* path) {
        files.Append(path);
        provided = 0;
    }
    FilesProvider(StrVec& filesIn, int n, int offset) {
        // get every n-th file starting at offset
        for (int i = offset; i < filesIn.Size(); i += n) {
            const char* f = filesIn[i];
            files.Append(f);
        }
        provided = 0;
    }

    int GetFilesCount() override {
        return files.Size();
    }

    ~FilesProvider() override {
    }

    TempStr NextFile() override {
        if (provided >= files.Size()) {
            return nullptr;
        }
        TempStr res = files.At(provided++);
        return res;
    }

    void Restart() override {
        provided = 0;
    }
};

struct DirFileProviderAsync : TestFileProvider {
    StrQueue queue;
    AutoFreeStr startDir;
    AutoFreeStr fileFilter;
    // those are only set once and then only read so
    // don't have to be atomic
    volatile int max = 0;
    volatile bool random = false;

    AtomicInt nFiles;

    DirFileProviderAsync(const char* path, const char* filter, int max = 0, bool random = false) {
        startDir.SetCopy(path);
        if (filter && !str::Eq(filter, "*")) {
            fileFilter.SetCopy(filter);
        }
        this->max = max;
        this->random = random;
        StartDirTraverseAsync(&queue, startDir.CStr(), true);
    }
    ~DirFileProviderAsync() override = default;
    TempStr NextFile() override;
    int GetFilesCount() override {
        return queue.Size();
    }

    virtual void Restart() override {
        nFiles.Set(0);
        StartDirTraverseAsync(&queue, startDir.CStr(), true);
    }
};

static void GetNextFileCb(char** path, StrQueue* q) {
    int n = q->strings.Size();
    int idx = rand() % n;
    *path = q->strings.RemoveAtFast(idx);
}

TempStr DirFileProviderAsync::NextFile() {
    if (max > 0 && nFiles.Get() >= max) {
        return nullptr;
    }
again:
    char* path = nullptr;
    if (random) {
        auto fn = MkFunc1(GetNextFileCb, &path);
        bool ok = queue.Access(fn);
        if (!ok) {
            ReportIf(path);
            return nullptr;
        }
        ReportIf(!path);
        if (!IsStressTestSupportedFile(path, fileFilter.Get())) {
            goto again;
        }
        nFiles.Inc();
        return path;
    }
    path = queue.PopFront();
    if (queue.IsSentinel(path)) {
        return nullptr;
    }
    if (!IsStressTestSupportedFile(path, fileFilter.Get())) {
        goto again;
    }
    nFiles.Inc();
    return path;
}

/* The idea of StressTest is to render a lot of PDFs sequentially, simulating
a human advancing one page at a time. This is mostly to run through a large number
of PDFs before a release to make sure we're crash proof. */

struct StressTest {
    MainWindow* win = nullptr;
    LARGE_INTEGER currPageRenderTime = {};
    Vec<int> pagesToRender;
    int currPageNo = 0;
    int pageForSearchStart = 0;
    int nFilesProcessed = 0; // number of files processed so far
    int timerId = 0;
    bool exitWhenDone = false;
    int nSlowPages = 0;

    SYSTEMTIME stressStartTime{};
    int cycles = 1;
    Vec<PageRange> pageRanges;
    // range of files to render (files get a new index when going through several cycles)
    Vec<PageRange> fileRanges;
    int fileIndex = 0;
    bool gotToc = false;

    // owned by StressTest
    TestFileProvider* fileProvider = nullptr;

    StressTest(MainWindow* win, bool exitWhenDone);
    ~StressTest();
};

template <typename T>
T RemoveRandomElementFromVec(Vec<T>& v) {
    auto n = v.Size();
    ReportIf(n <= 0);
    int idx = rand() % n;
    int res = v.PopAt((size_t)idx);
    return res;
}

StressTest::StressTest(MainWindow* win, bool exitWhenDone) {
    this->win = win;
    this->exitWhenDone = exitWhenDone;
    timerId = gCurrStressTimerId++;
}

StressTest::~StressTest() {
    // TODO: it can be shared so find a different way to free it
    // delete fileProvider;
}

static void TickTimer(StressTest* st) {
    SetTimer(st->win->hwndFrame, st->timerId, USER_TIMER_MINIMUM, nullptr);
}

static void Start(StressTest* st, TestFileProvider* fileProvider, int cycles) {
    GetSystemTime(&st->stressStartTime);

    st->fileProvider = fileProvider;
    st->cycles = cycles;

    if (st->pageRanges.size() == 0) {
        st->pageRanges.Append(PageRange());
    }
    if (st->fileRanges.size() == 0) {
        st->fileRanges.Append(PageRange());
    }

    TickTimer(st);
}

static void Finished(StressTest* st, bool success) {
    st->win->stressTest = nullptr; // make sure we're not double-deleted

    if (success) {
        int secs = SecsSinceSystemTime(st->stressStartTime);
        TempStr tm = FormatTimeTemp(secs);
        TempStr s = str::FormatTemp("Stress test complete, rendered %d files in %s", st->nFilesProcessed, tm);
        NotificationCreateArgs args;
        args.hwndParent = st->win->hwndCanvas;
        args.msg = s;
        args.timeoutMs = 0;
        args.groupId = kNotifStressTestSummary;
        ShowNotification(args);
    }

    CloseWindow(st->win, st->exitWhenDone && CanCloseWindow(st->win), false);
    delete st;
}

static void Start(StressTest* st, const char* path, const char* filter, const char* ranges, int cycles) {
    if (file::Exists(path)) {
        FilesProvider* filesProvider = new FilesProvider(path);
        ParsePageRanges(ranges, st->pageRanges);
        Start(st, filesProvider, cycles);
    } else if (dir::Exists(path)) {
        auto dirFileProvider = new DirFileProviderAsync(path, filter);
        ParsePageRanges(ranges, st->fileRanges);
        Start(st, dirFileProvider, cycles);
    } else {
        TempStr s = str::FormatTemp("Path '%s' doesn't exist", path);
        NotificationCreateArgs args;
        args.hwndParent = st->win->hwndCanvas;
        args.msg = s;
        args.warning = true;
        args.timeoutMs = 0;
        args.groupId = kNotifStressTestSummary;
        ShowNotification(args);
        Finished(st, false);
    }
}

static bool OpenFile(StressTest* st, const char* fileName) {
    printf("%s\n", fileName);
    fflush(stdout);

    LoadArgs args(fileName, st->win);
    // args->forceReuse = rand() % 3 != 1;
    args.forceReuse = true;
    args.noPlaceWindow = true;
    MainWindow* w = LoadDocument(&args);
    if (!w) {
        return false;
    }

    if (w == st->win) { // MainWindow reused
        if (!st->win->IsDocLoaded()) {
            return false;
        }
    } else {
        if (!w->IsDocLoaded()) { // new MainWindow
            CloseWindow(w, false, false);
            return false;
        }
    }

#if 0
    // transfer ownership of stressTest object to a new window and close the
    // current one
    ReportIf(st != st->win->stressTest);
    if (w != st->win) {
        if (st->win->IsDocLoaded()) {
            // try to provoke a crash in RenderCache cleanup code
            Rect rect = ClientRect(st->win->hwndFrame);
            rect.Inflate(rand() % 10, rand() % 10);
            SendMessageW(st->win->hwndFrame, WM_SIZE, 0, MAKELONG(rect.dx, rect.dy));
            if (st->win->AsFixed()) {
                st->win->cbHandler->RequestRendering(1);
            }
            ScheduleRepaint(st->win, 0);
        }

        MainWindow* toClose = st->win;
        w->stressTest = st->win->stressTest;
        st->win->stressTest = nullptr;
        st->win = w;
        CloseWindow(toClose, false, false);
    }
#endif
    if (!st->win->IsDocLoaded()) {
        return false;
    }

    auto ctrl = st->win->ctrl;
    ctrl->SetDisplayMode(DisplayMode::Continuous);
    ctrl->SetZoomVirtual(kZoomFitPage, nullptr);
    ctrl->GoToFirstPage();
    if (st->win->tocVisible || gGlobalPrefs->showFavorites) {
        SetSidebarVisibility(st->win, st->win->tocVisible, gGlobalPrefs->showFavorites);
    }

    st->nSlowPages = 0;
    st->pagesToRender.Clear();
    constexpr int nMaxPages = 32;
    int nPages = ctrl->PageCount();
    if (IsFullRange(st->pageRanges)) {
        Vec<int> allPages;
        for (int n = 1; n <= nPages; n++) {
            allPages.Append(n);
        }
        while ((st->pagesToRender.size() < nMaxPages) && (allPages.size() > 0)) {
            int nRandom = RemoveRandomElementFromVec(allPages);
            st->pagesToRender.Append(nRandom);
        }
    } else {
        for (int n = 1; n <= nPages; n++) {
            if (IsInRange(st->pageRanges, n)) {
                st->pagesToRender.Append(n);
            }
        }
        for (auto&& range : st->pageRanges) {
            for (int n = range.start; n <= range.end && n <= nPages; n++) {
                st->pagesToRender.Append(n);
            }
        }
        if (st->pagesToRender.size() == 0) {
            return false;
        }
    }

    int randomPageIdx = rand() % st->pagesToRender.Size();
    st->pageForSearchStart = st->pagesToRender[randomPageIdx];

    st->currPageNo = st->pagesToRender.PopAt(0);
    ctrl->GoToPage(st->currPageNo, false);
    st->currPageRenderTime = TimeGet();
    ++st->nFilesProcessed;

    // search immediately in single page documents
    if (1 == st->pageForSearchStart) {
        // use text that is unlikely to be found, so that we search all pages
        HwndSetText(st->win->hwndFindEdit, "!z_yt");
        FindTextOnThread(st->win, TextSearch::Direction::Forward, true);
    }

    int secs = SecsSinceSystemTime(st->stressStartTime);
    TempStr tm = FormatTimeTemp(secs);
    int nTotalFiles = st->fileProvider->GetFilesCount();
    TempStr s = str::FormatTemp("File %d (left: %d): %s, time: %s", st->nFilesProcessed, nTotalFiles, fileName, tm);
    NotificationCreateArgs nargs;
    nargs.hwndParent = st->win->hwndCanvas;
    nargs.msg = s;
    nargs.timeoutMs = 0;
    nargs.groupId = kNotifStressTestSummary;
    ShowNotification(nargs);
    return true;
}

static void RandomizeViewingState(StressTest* st) {
    // every 10 pages change the viewing sate (zoom etc.)
    int every10 = RAND_MAX / 10;
    if (rand() > every10) {
        return;
    }
    auto ctrl = st->win->ctrl;

    int n = rand() % 12;
    float zoom;
    switch (n) {
        case 0:
            ctrl->SetZoomVirtual(kZoomFitPage, nullptr);
            break;
        case 1:
            ctrl->SetZoomVirtual(kZoomFitWidth, nullptr);
            break;
        case 2:
            ctrl->SetZoomVirtual(kZoomFitContent, nullptr);
            break;
        case 3:
            ctrl->SetZoomVirtual(kZoomActualSize, nullptr);
            break;
        case 4:
            ctrl->SetDisplayMode(DisplayMode::SinglePage);
            break;
        case 5:
            ctrl->SetDisplayMode(DisplayMode::Facing);
            break;
        case 6:
            ctrl->SetDisplayMode(DisplayMode::BookView);
            break;
        case 7:
            ctrl->SetDisplayMode(DisplayMode::Continuous);
            break;
        case 8:
            ctrl->SetDisplayMode(DisplayMode::ContinuousFacing);
            break;
        case 9:
            ctrl->SetDisplayMode(DisplayMode::ContinuousBookView);
            break;
        case 10:
            zoom = ctrl->GetNextZoomStep(kZoomMax);
            ctrl->SetZoomVirtual(zoom, nullptr);
            break;
        case 11:
            zoom = ctrl->GetNextZoomStep(kZoomMin);
            ctrl->SetZoomVirtual(zoom, nullptr);
            break;
    }
}

static bool GoToNextFile(StressTest* st) {
    for (;;) {
        TempStr nextFile = st->fileProvider->NextFile();
        if (nextFile) {
            if (!IsInRange(st->fileRanges, ++st->fileIndex)) {
                continue;
            }
            if (OpenFile(st, nextFile)) {
                return true;
            }
            continue;
        }
        if (--st->cycles <= 0) {
            return false;
        }
        st->fileProvider->Restart();
    }
}

static bool GoToNextPage(StressTest* st) {
    double pageRenderTime = TimeSinceInMs(st->currPageRenderTime);
    TempStr s = str::FormatTemp("Page %d rendered in %d ms", st->currPageNo, (int)pageRenderTime);
    NotificationCreateArgs args;
    args.hwndParent = st->win->hwndCanvas;
    args.msg = s;
    args.groupId = kNotifStressTestBenchmark;
    ShowNotification(args);
    if (pageRenderTime > 700) {
        st->nSlowPages += 1;
    }
    bool goToNextFile = st->pagesToRender.size() == 0;
    if (st->nSlowPages >= 3) {
        // some files are scanned .jpx images that are slow to render
        // not much to learn from rendering them so we skip those if
        // we see more than 3 slow pages
        goToNextFile = true;
    }
    if (goToNextFile) {
        if (GoToNextFile(st)) {
            return true;
        }
        Finished(st, true);
        return false;
    }

    auto ctrl = st->win->ctrl;
    if (!st->gotToc) {
        // trigger getting toc and props
        st->gotToc = true;
        ctrl->GetToc();
        const char** props = gAllProps;
        while (*props) {
            const char* prop = *props++;
            if (str::Eq(prop, kPropFontList)) {
                // this can be expensive so skip
                continue;
            }
            ctrl->GetPropertyTemp(prop);
        }
    }

    RandomizeViewingState(st);
    st->currPageNo = st->pagesToRender.PopAt(0);
    ctrl->GoToPage(st->currPageNo, false);
    st->currPageRenderTime = TimeGet();

    // start text search when we're in the middle of the document, so that
    // search thread touches both pages that were already rendered and not yet
    // rendered
    // TODO: it would be nice to also randomize search starting page but the
    // current API doesn't make it easy
    if (st->currPageNo == st->pageForSearchStart) {
        // use text that is unlikely to be found, so that we search all pages
        HwndSetText(st->win->hwndFindEdit, "!z_yt");
        FindTextOnThread(st->win, TextSearch::Direction::Forward, true);
    }

    if (1 == rand() % 3) {
        Rect rect = ClientRect(st->win->hwndFrame);
        int deltaX = (rand() % 40) - 23;
        rect.dx += deltaX;
        if (rect.dx < 300) {
            rect.dx += (abs(deltaX) * 3);
        }
        int deltaY = (rand() % 40) - 23;
        rect.dy += deltaY;
        if (rect.dy < 300) {
            rect.dy += (abs(deltaY) * 3);
        }
        SendMessageW(st->win->hwndFrame, WM_SIZE, 0, MAKELONG(rect.dx, rect.dy));
    }
    return true;
}

static void OnTimer(StressTest* st, int timerIdGot) {
    DisplayModel* dm;
    bool didRender;

    ReportIf(st->timerId != timerIdGot);
    KillTimer(st->win->hwndFrame, st->timerId);
    if (!st->win->IsDocLoaded()) {
        if (!GoToNextFile(st)) {
            Finished(st, true);
            return;
        }
        TickTimer(st);
        return;
    }

    // chm documents aren't rendered and we block until we show them
    // so we can assume previous page has been shown and go to next page
    if (!st->win->AsFixed()) {
        if (!GoToNextPage(st)) {
            return;
        }
        goto Next;
    }

    // For non-image files, we detect if a page was rendered by checking the cache
    // (but we don't wait more than 3 seconds).
    // Image files are always fully rendered in WM_PAINT, so we know the page
    // has already been rendered.
    dm = st->win->AsFixed();
    didRender = gRenderCache->Exists(dm, st->currPageNo, dm->GetRotation());
    if (!didRender && dm->ShouldCacheRendering(st->currPageNo)) {
        double timeInMs = TimeSinceInMs(st->currPageRenderTime);
        if (timeInMs > 3.0 * 1000) {
            if (!GoToNextPage(st)) {
                return;
            }
        }
    } else if (!GoToNextPage(st)) {
        return;
    }
    MakeRandomSelection(st->win, st->currPageNo);

Next:
    TickTimer(st);
}

// note: used from CrashHandler, shouldn't allocate memory
static void GetLogInfo(StressTest* st, str::Str* s) {
    s->AppendFmt(", stress test rendered %d files in ", st->nFilesProcessed);
    FormatTime(SecsSinceSystemTime(st->stressStartTime), s);
    s->AppendFmt(", currPage: %d", st->currPageNo);
}

// note: used from CrashHandler.cpp, should not allocate memory
void GetStressTestInfo(str::Str* s) {
    // only add paths to files encountered during an explicit stress test
    // (for privacy reasons, users should be able to decide themselves
    // whether they want to share what files they had opened during a crash)
    if (!IsStressTesting()) {
        return;
    }

    for (size_t i = 0; i < gWindows.size(); i++) {
        MainWindow* w = gWindows.at(i);
        if (!w || !w->CurrentTab() || !w->CurrentTab()->filePath) {
            continue;
        }

        s->Append("File: ");
        const char* filePath = w->CurrentTab()->filePath;
        s->Append(filePath);
        GetLogInfo(w->stressTest, s);
        s->Append("\r\n");
    }
}

void StartStressTest(Flags* i, MainWindow* win) {
    gIsStressTesting = true;
    // TODO: for now stress testing only supports the non-ebook ui
    gGlobalPrefs->chmUI.useFixedPageUI = true;
    // TODO: make stress test work with tabs?
    gGlobalPrefs->useTabs = false;
    // forbid entering sleep mode during tests
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
    srand((unsigned int)time(nullptr));

    // redirect stderr to NUL to disable (MuPDF) logging
    FILE* nul;
    freopen_s(&nul, "NUL", "w", stderr);

    int n = i->stressParallelCount;
    if (n > 1 || i->stressRandomizeFiles) {
        MainWindow** windows = AllocArray<MainWindow*>(n);
        windows[0] = win;
        for (int j = 1; j < n; j++) {
            windows[j] = CreateAndShowMainWindow();
            if (!windows[j]) {
                return;
            }
        }

        printf("Scanning for files in directory %s\n", i->stressTestPath);
        fflush(stdout);

        auto filesProvider =
            new DirFileProviderAsync(i->stressTestPath, i->stressTestFilter, i->stressTestMax, i->stressRandomizeFiles);

        for (int j = 0; j < n; j++) {
            // dst will be deleted when the stress ends
            win = windows[j];
            StressTest* dst = new StressTest(win, i->exitWhenDone);
            win->stressTest = dst;
            Start(dst, filesProvider, i->stressTestCycles);
        }

        free(windows);
    } else {
        // dst will be deleted when the stress ends
        StressTest* st = new StressTest(win, i->exitWhenDone);
        win->stressTest = st;
        Start(st, i->stressTestPath, i->stressTestFilter, i->stressTestRanges, i->stressTestCycles);
    }
}

void OnStressTestTimer(MainWindow* win, int timerId) {
    OnTimer(win->stressTest, timerId);
}

void FinishStressTest(MainWindow* win) {
    delete win->stressTest;
}
