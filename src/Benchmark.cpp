#include <windows.h>
#include <tchar.h>
#include "Benchmark.h"
#include "PdfEngine.h"
#include "WinUtil.h"
#include "FileUtil.h"

#define logbench(msg, ...) _ftprintf(stderr, _T(msg), __VA_ARGS__)

static void BenchLoadRender(PdfEngine *engine, int pagenum)
{
    MillisecondTimer t;

    t.Start();
    bool success = engine->_benchLoadPage(pagenum);
    t.Stop();

    if (!success) {
        logbench("Error: failed to load page %d\n", pagenum);
        return;
    }
    double timems = t.GetTimeInMs();
    logbench("pageload   %3d: %.2f ms\n", pagenum, timems);

    t.Start();
    RenderedBitmap *rendered = engine->renderBitmap(pagenum, 1.0, 0);
    t.Stop();

    if (!rendered) {
        logbench("Error: failed to render page %d\n", pagenum);
        return;
    }
    delete rendered;
    timems = t.GetTimeInMs();
    logbench("pagerender %3d: %.2f ms\n", pagenum, timems);
}

// <s> can be in form "1" or "3-58". Error is returned by setting <start>
// and <end> to values that won't be valid to the caller
static void GetRange(TCHAR *s, int& start, int& end)
{
    if (_stscanf(s, _T("%d-%d"), &start, &end) == 2)
        ;
    else if (_stscanf(s, _T("%d"), &start) == 1)
        end = start;
    else
        start = end = -1; // error case
}

// <s> can be:
// * "loadonly"
// * description of page ranges e.g. "1", "1-5", "2-3,6,8-10"
bool IsBenchPagesInfo(TCHAR *s)
{
    if (tstr_ieq(s, _T("loadonly")))
        return true;

    while (s) {
        int start, end;
        GetRange(s, start, end);
        if (start < 0 || end < 0)
            return false;

        s = _tcschr(s, ',');
        if (s)
            s++;
    }
    
    return true;
}

static void BenchFile(TCHAR *filePath, TCHAR *pagesSpec)
{
    if (!file_exists(filePath)) {
        logbench("Error: file %s doesn't exist\n", filePath);
        return;
    }

    MillisecondTimer total;
    total.Start();

    logbench("Starting: %s\n", filePath);

    MillisecondTimer t;
    t.Start();
    PdfEngine *engine = PdfEngine::CreateFromFileName(filePath);
    t.Stop();

    if (!engine) {
        logbench("Error: failed to load %s\n", filePath);
        return;
    }

    double timems = t.GetTimeInMs();
    logbench("load: %.2f ms\n", timems);
    int pages = engine->pageCount();
    logbench("page count: %d\n", pages);

    if (NULL == pagesSpec) {
        for (int i = 1; i <= pages; i++)
            BenchLoadRender(engine, i);
    }

    while (pagesSpec) {
        int start, end;
        GetRange(pagesSpec, start, end);
        for (int j = start; j <= end; j++) {
            if (1 <= j && j <= pages)
                BenchLoadRender(engine, j);
        }

        pagesSpec = _tcschr(pagesSpec, ',');
        if (pagesSpec)
            pagesSpec++;
    }

    delete engine;
    total.Stop();

    logbench("Finished (in %.2f ms): %s\n", total.GetTimeInMs(), filePath);
}

void Bench(VStrList& filesToBench)
{
    size_t n = filesToBench.Count() / 2;
    for (size_t i = 0; i < n; i++)
        BenchFile(filesToBench[2*i], filesToBench[2*i + 1]);
}
