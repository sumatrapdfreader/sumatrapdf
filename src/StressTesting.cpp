/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include <time.h>
#include "WinUtil.h"
#include "FileUtil.h"
#include "SimpleLog.h"

#include "StressTesting.h"
#include "EngineManager.h"
#include "WindowInfo.h"
#include "AppTools.h"
#include "RenderCache.h"
#include "SumatraPDF.h"
#include "Notifications.h"

static slog::Logger *gLog;
#define logbench(msg, ...) gLog->LogFmt(_T(msg), __VA_ARGS__)

struct PageRange {
    PageRange() : start(1), end(INT_MAX) { }
    PageRange(int start, int end) : start(start), end(end) { }

    int start, end; // end == INT_MAX means to the last page
};

// parses a list of page ranges such as 1,3-5,7- (i..e all but pages 2 and 6)
// into an interable list (returns NULL on parsing errors)
// caller must delete the result
static bool ParsePageRanges(const TCHAR *ranges, Vec<PageRange>& result)
{
    if (!ranges)
        return false;

    StrVec rangeList;
    rangeList.Split(ranges, _T(","), true);
    rangeList.SortNatural();

    for (size_t i = 0; i < rangeList.Count(); i++) {
        int start, end;
        if (str::Parse(rangeList.At(i), _T("%d-%d%$"), &start, &end) && 0 < start && start <= end)
            result.Append(PageRange(start, end));
        else if (str::Parse(rangeList.At(i), _T("%d-%$"), &start) && 0 < start)
            result.Append(PageRange(start, INT_MAX));
        else if (str::Parse(rangeList.At(i), _T("%d%$"), &start) && 0 < start)
            result.Append(PageRange(start, start));
        else
            return false;
    }

    return result.Count() > 0;
}

// a valid page range is a non-empty, comma separated list of either
// single page ("3") numbers, closed intervals "2-4" or intervals
// unlimited to the right ("5-")
bool IsValidPageRange(const TCHAR *ranges)
{
    Vec<PageRange> rangeList;
    return ParsePageRanges(ranges, rangeList);
}

inline bool IsInRange(Vec<PageRange>& ranges, int pageNo)
{
    for (size_t i = 0; i < ranges.Count(); i++)
        if (ranges.At(i).start <= pageNo && pageNo <= ranges.At(i).end)
            return true;
    return false;
}

static void BenchLoadRender(BaseEngine *engine, int pagenum)
{
    MillisecondTimer t;

    t.Start();
    bool ok = engine->BenchLoadPage(pagenum);
    t.Stop();

    if (!ok) {
        logbench("Error: failed to load page %d", pagenum);
        return;
    }
    double timems = t.GetTimeInMs();
    logbench("pageload   %3d: %.2f ms", pagenum, timems);

    t.Start();
    RenderedBitmap *rendered = engine->RenderBitmap(pagenum, 1.0, 0);
    t.Stop();

    if (!rendered) {
        logbench("Error: failed to render page %d", pagenum);
        return;
    }
    delete rendered;
    timems = t.GetTimeInMs();
    logbench("pagerender %3d: %.2f ms", pagenum, timems);
}

// <s> can be:
// * "loadonly"
// * description of page ranges e.g. "1", "1-5", "2-3,6,8-10"
bool IsBenchPagesInfo(const TCHAR *s)
{
    return str::EqI(s, _T("loadonly")) || IsValidPageRange(s);
}

static void BenchFile(TCHAR *filePath, const TCHAR *pagesSpec)
{
    if (!file::Exists(filePath)) {
        logbench("Error: file %s doesn't exist", filePath);
        return;
    }

    MillisecondTimer total;
    total.Start();

    logbench("Starting: %s", filePath);

    MillisecondTimer t;
    t.Start();
    BaseEngine *engine = EngineManager::CreateEngine(filePath);
    t.Stop();

    if (!engine) {
        logbench("Error: failed to load %s", filePath);
        return;
    }

    double timems = t.GetTimeInMs();
    logbench("load: %.2f ms", timems);
    int pages = engine->PageCount();
    logbench("page count: %d", pages);

    if (NULL == pagesSpec) {
        for (int i = 1; i <= pages; i++)
            BenchLoadRender(engine, i);
    }

    assert(!pagesSpec || IsBenchPagesInfo(pagesSpec));
    Vec<PageRange> ranges;
    if (ParsePageRanges(pagesSpec, ranges)) {
        for (size_t i = 0; i < ranges.Count(); i++) {
            for (int j = ranges.At(i).start; j <= ranges.At(i).end; j++)
                if (1 <= j && j <= pages)
                    BenchLoadRender(engine, j);
        }
    }

    delete engine;
    total.Stop();

    logbench("Finished (in %.2f ms): %s", total.GetTimeInMs(), filePath);
}

void Bench(StrVec& filesToBench)
{
    gLog = new slog::StderrLogger();

    size_t n = filesToBench.Count() / 2;
    for (size_t i = 0; i < n; i++)
        BenchFile(filesToBench.At(2 * i), filesToBench.At(2 * i + 1));

    delete gLog;
}

inline bool IsSpecialDir(const TCHAR *s)
{
    return str::Eq(s, _T(".")) || str::Eq(s, _T(".."));
}

bool CollectPathsFromDirectory(const TCHAR *pattern, StrVec& paths, bool dirsInsteadOfFiles)
{
    ScopedMem<TCHAR> dirPath(path::GetDir(pattern));

    WIN32_FIND_DATA fdata;
    HANDLE hfind = FindFirstFile(pattern, &fdata);
    if (INVALID_HANDLE_VALUE == hfind)
        return false;

    do {
        bool append = !dirsInsteadOfFiles;
        if ((fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            append = dirsInsteadOfFiles && !IsSpecialDir(fdata.cFileName);
        if (append)
            paths.Append(path::Join(dirPath, fdata.cFileName));
    } while (FindNextFile(hfind, &fdata));
    FindClose(hfind);

    return paths.Count() > 0;
}

static bool IsStressTestSupportedFile(const TCHAR *fileName, const TCHAR *filter)
{
    if (filter && !path::Match(fileName, filter))
        return false;

    return PdfEngine::IsSupportedFile(fileName)  ||
           XpsEngine::IsSupportedFile(fileName)  ||
           DjVuEngine::IsSupportedFile(fileName) ||
           CbxEngine::IsSupportedFile(fileName);
}

static bool CollectStressTestSupportedFilesFromDirectory(const TCHAR *dirPath, const TCHAR *filter, StrVec& paths)
{
    ScopedMem<TCHAR> pattern(path::Join(dirPath, _T("*")));

    WIN32_FIND_DATA fdata;
    HANDLE hfind = FindFirstFile(pattern, &fdata);
    if (INVALID_HANDLE_VALUE == hfind)
        return false;

    do {
        if (!(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            if (IsStressTestSupportedFile(fdata.cFileName, filter)) {
                paths.Append(path::Join(dirPath, fdata.cFileName));
            }
        }
    } while (FindNextFile(hfind, &fdata));
    FindClose(hfind);

    return paths.Count() > 0;
}

// return t1 - t2 in seconds
static int SystemTimeDiffInSecs(SYSTEMTIME& t1, SYSTEMTIME& t2)
{
    FILETIME ft1, ft2;
    SystemTimeToFileTime(&t1, &ft1);
    SystemTimeToFileTime(&t2, &ft2);
    return FileTimeDiffInSecs(ft1, ft2);
}

static int SecsSinceSystemTime(SYSTEMTIME& time)
{
    SYSTEMTIME currTime;    
    GetSystemTime(&currTime);
    return SystemTimeDiffInSecs(currTime, time);
}

static TCHAR *FormatTime(int totalSecs)
{
    int secs = totalSecs % 60;
    int totalMins = totalSecs / 60;
    int mins = totalMins % 60;
    int hrs = totalMins / 60;
    if (hrs > 0)
        return str::Format(_T("%d hrs %d mins %d secs"), hrs, mins, secs);
    if (mins > 0)
        return str::Format(_T("%d mins %d secs"), mins, secs);
    return str::Format(_T("%d secs"), secs);
}

static void MakeRandomSelection(DisplayModel *dm, int pageNo)
{
    if (!dm->ValidPageNo(pageNo))
        pageNo = 1;
    if (!dm->ValidPageNo(pageNo))
        return;

    // try a random position in the page
    int x = rand() % 640;
    int y = rand() % 480;
    if (dm->textSelection->IsOverGlyph(pageNo, x, y)) {
        dm->textSelection->StartAt(pageNo, x, y);
        dm->textSelection->SelectUpTo(pageNo, rand() % 640, rand() % 480);
    }
}

/* The idea of StressTest is to render a lot of PDFs sequentially, simulating
a human advancing one page at a time. This is mostly to run through a large number
of PDFs before a release to make sure we're crash proof. */

class StressTest : public CallbackFunc {
    WindowInfo *      win;
    RenderCache *     renderCache;
    MillisecondTimer  currPageRenderTime;
    int               currPage;
    int               pageForSearchStart;
    int               filesCount; // number of files processed so far

    SYSTEMTIME        stressStartTime;
    int               cycles;
    Vec<PageRange>    pageRanges;
    ScopedMem<TCHAR>  basePath;
    ScopedMem<TCHAR>  fileFilter;
    // range of files to render (files get a new index when going through several cycles)
    Vec<PageRange>    fileRanges;
    int               fileIndex;

    // current state of directory traversal
    StrVec            filesToOpen;
    StrVec            dirsToVisit;

    bool OpenDir(const TCHAR *dirPath);
    bool OpenFile(const TCHAR *fileName);

    bool GoToNextPage();
    bool GoToNextFile();

    void OnTimer();
    void TickTimer();
    void Finished(bool success);

public:
    StressTest(WindowInfo *win, RenderCache *renderCache) :
        win(win), renderCache(renderCache), currPage(0), pageForSearchStart(0),
        filesCount(0), cycles(1), fileIndex(0)
        { }

    char *GetLogInfo();
    void Start(const TCHAR *path, const TCHAR *filter, const TCHAR *ranges, int cycles);

    virtual void Callback() { OnTimer(); }
};

void StressTest::Start(const TCHAR *path, const TCHAR *filter, const TCHAR *ranges, int cycles)
{
    srand((unsigned int)time(NULL));
    GetSystemTime(&stressStartTime);

    // forbid entering sleep mode during tests
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);

    basePath.Set(str::Dup(path));
    fileFilter.Set(filter && !str::Eq(filter, _T("*")) ? str::Dup(filter) : NULL);
    if (file::Exists(basePath)) {
        filesToOpen.Append(str::Dup(basePath));
        ParsePageRanges(ranges, pageRanges);
    }
    else if (dir::Exists(basePath)) {
        OpenDir(basePath);
        ParsePageRanges(ranges, fileRanges);
    }
    else {
        // Note: dev only, don't translate
        ScopedMem<TCHAR> s(str::Format(_T("Path '%s' doesn't exist"), path));
        ShowNotification(win, s, false /* autoDismiss */, true, NG_STRESS_TEST_SUMMARY);
        Finished(false);
        return;
    }

    this->cycles = cycles;
    if (pageRanges.Count() == 0)
        pageRanges.Append(PageRange());
    if (fileRanges.Count() == 0)
        fileRanges.Append(PageRange());

    if (GoToNextFile())
        TickTimer();
    else
        Finished(true);
}

void StressTest::Finished(bool success)
{
    win->stressTest = NULL;
    SetThreadExecutionState(ES_CONTINUOUS);

    if (success) {
        int secs = SecsSinceSystemTime(stressStartTime);
        ScopedMem<TCHAR> tm(FormatTime(secs));
        ScopedMem<TCHAR> s(str::Format(_T("Stress test complete, rendered %d files in %s"), filesCount, tm));
        ShowNotification(win, s, false, false, NG_STRESS_TEST_SUMMARY);
    }

    CloseWindow(win, false);
    delete this;
}

bool StressTest::OpenDir(const TCHAR *dirPath)
{
    assert(filesToOpen.Count() == 0);

    bool hasFiles = CollectStressTestSupportedFilesFromDirectory(dirPath, fileFilter, filesToOpen);
    filesToOpen.SortNatural();

    ScopedMem<TCHAR> pattern(str::Format(_T("%s\\*"), dirPath));
    bool hasSubDirs = CollectPathsFromDirectory(pattern, dirsToVisit, true);

    return hasFiles || hasSubDirs;
}

bool StressTest::GoToNextFile()
{
    for (;;) {
        while (filesToOpen.Count() > 0) {
            // test next file
            ScopedMem<TCHAR> path(filesToOpen.At(0));
            filesToOpen.RemoveAt(0);
            if (!IsInRange(fileRanges, ++fileIndex))
                continue;
            if (OpenFile(path))
                return true;
        }

        if (dirsToVisit.Count() > 0) {
            // test next directory
            ScopedMem<TCHAR> path(dirsToVisit.At(0));
            dirsToVisit.RemoveAt(0);
            OpenDir(path);
            continue;
        }

        if (--cycles <= 0)
            return false;
        // start next cycle
        if (file::Exists(basePath))
            filesToOpen.Append(str::Dup(basePath));
        else
            OpenDir(basePath);
    }
}

bool StressTest::OpenFile(const TCHAR *fileName)
{
    bool reuse = rand() % 3 != 1;
    WindowInfo *w = LoadDocument(fileName, NULL, true /* show */, reuse, true /* suppressPwdUI */);
    if (!w)
        return false;

    if (w == win) { // WindowInfo reused
        if (!win->dm)
            return false;
    } else if (!w->dm) { // new WindowInfo
        CloseWindow(w, false, true);
        return false;
    }

    // transfer ownership of stressTest object to a new window and close the
    // current one
    assert(this == win->stressTest);
    if (w != win) {
        WindowInfo *toClose = win;
        w->stressTest = win->stressTest;
        win->stressTest = NULL;
        win = w;
        CloseWindow(toClose, false);
    }
    if (!win->dm)
        return false;

    win->dm->ChangeDisplayMode(DM_CONTINUOUS);
    win->dm->ZoomTo(ZOOM_FIT_PAGE);
    win->dm->GoToFirstPage();
    if (win->tocVisible || gGlobalPrefs.favVisible)
        SetSidebarVisibility(win, win->tocVisible, gGlobalPrefs.favVisible);

    currPage = pageRanges.At(0).start;
    win->dm->GoToPage(currPage, 0);
    currPageRenderTime.Start();
    ++filesCount;

    pageForSearchStart = (rand() % win->dm->PageCount()) + 1;

    int secs = SecsSinceSystemTime(stressStartTime);
    ScopedMem<TCHAR> tm(FormatTime(secs));
    ScopedMem<TCHAR> s(str::Format(_T("File %d: %s, time: %s"), filesCount, fileName, tm));
    ShowNotification(win, s, false, false, NG_STRESS_TEST_SUMMARY);

    return true;
}

bool StressTest::GoToNextPage()
{
    double pageRenderTime = currPageRenderTime.GetCurrTimeInMs();
    ScopedMem<TCHAR> s(str::Format(_T("Page %d rendered in %d milliseconds"), currPage, (int)pageRenderTime));
    ShowNotification(win, s, true, false, NG_STRESS_TEST_BENCHMARK);

    ++currPage;
    while (!IsInRange(pageRanges, currPage) && currPage <= win->dm->PageCount())
        currPage++;

    if (currPage > win->dm->PageCount()) {
        if (GoToNextFile())
            return true;
        Finished(true);
        return false;
    }

    win->dm->GoToPage(currPage, 0);
    currPageRenderTime.Start();

    // start text search when we're in the middle of the document, so that
    // search thread touches both pages that were already rendered and not yet
    // rendered
    // TODO: it would be nice to also randomize search starting page but the
    // current API doesn't make it easy
    if (currPage == pageForSearchStart) {
        // use text that is unlikely to be found, so that we search all pages
        win::SetText(win->hwndFindBox, _T("!z_yt"));
        FindTextOnThread(win);
    }

    if (1 == rand() % 3) {
        ClientRect rect(win->hwndFrame);
        int deltaX = (rand() % 40) - 23;
        rect.dx += deltaX;
        if (rect.dx < 300)
            rect.dx += (abs(deltaX) * 3);
        int deltaY = (rand() % 40) - 23;
        rect.dy += deltaY;
        if (rect.dy < 300)
            rect.dy += (abs(deltaY) * 3);
        SendMessage(win->hwndFrame, WM_SIZE, 0, MAKELONG(rect.dx, rect.dy));
    }
    return true;
}

void StressTest::TickTimer()
{
    SetTimer(win->hwndCanvas, DIR_STRESS_TIMER_ID, USER_TIMER_MINIMUM, NULL);
}

void StressTest::OnTimer()
{
    KillTimer(win->hwndCanvas, DIR_STRESS_TIMER_ID);
    if (!win->dm || !win->dm->engine)
        return;

    // For non-image files, we detect if a page was rendered by checking the cache
    // (but we don't wait more than 3 seconds).
    // Image files are always fully rendered in WM_PAINT, so we know the page
    // has already been rendered.
    bool didRender = renderCache->Exists(win->dm, currPage, win->dm->Rotation());
    if (!didRender && DoCachePageRendering(win, currPage)) {
        double timeInMs = currPageRenderTime.GetCurrTimeInMs();
        if (timeInMs > 3.0 * 1000) {
            if (!GoToNextPage())
                return;
        }
    } else {
        if (!GoToNextPage())
            return;
    }
    MakeRandomSelection(win->dm, currPage);
    TickTimer();
}

char *StressTest::GetLogInfo()
{
    int secs = SecsSinceSystemTime(stressStartTime);
    ScopedMem<TCHAR> st(FormatTime(secs));
    ScopedMem<char> su(str::conv::ToUtf8(st));
    return str::Format(", stress test rendered %d files in %s, currPage: %d", filesCount, su, currPage);
}

char *GetStressTestInfo(CallbackFunc *dst)
{
    return static_cast<StressTest *>(dst)->GetLogInfo();
}

void StartStressTest(WindowInfo *win, const TCHAR *path, const TCHAR *filter,
                     const TCHAR *ranges, int cycles, RenderCache *renderCache)
{
    // gPredictiveRender = false;

    // dst will be deleted when the stress ends
    StressTest *dst = new StressTest(win, renderCache);
    win->stressTest = dst;
    dst->Start(path, filter, ranges, cycles);
}
