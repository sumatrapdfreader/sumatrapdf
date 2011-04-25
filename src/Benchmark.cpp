/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "WinUtil.h"
#include "FileUtil.h"
#include "SimpleLog.h"

#include "Benchmark.h"
#include "PdfEngine.h"
#include "DjVuEngine.h"

static Log::Logger *gLog;
#define logbench(msg, ...) gLog->LogFmt(_T(msg), __VA_ARGS__)
// #define logbench(msg, ...) Log::LogFmt(_T(msg), __VA_ARGS__)

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

// <s> can be in form "1" or "3-58". If the range is followed
// by a comma, that's skipped. The end of the parsed string
// is returned (or NULL in case of a parsing error).
static const TCHAR *GetRange(const TCHAR *s, int *start, int *end)
{
    const TCHAR *next = Str::Parse(s, _T("%d-%d%?,"), start, end);
    if (!next) {
        next = Str::Parse(s, _T("%d%?,"), start);
        if (next)
            *end = *start;
    }
    return next;
}

// <s> can be:
// * "loadonly"
// * description of page ranges e.g. "1", "1-5", "2-3,6,8-10"
bool IsBenchPagesInfo(const TCHAR *s)
{
    if (Str::IsEmpty(s))
        return false;
    if (Str::EqI(s, _T("loadonly")))
        return true;

    while (!Str::IsEmpty(s)) {
        int start, end;
        s = GetRange(s, &start, &end);
        if (!s || start < 0 || end < 0 || start > end)
            return false;
    }

    return true;
}

static void BenchFile(TCHAR *filePath, const TCHAR *pagesSpec)
{
    if (!File::Exists(filePath)) {
        logbench("Error: file %s doesn't exist", filePath);
        return;
    }

    MillisecondTimer total;
    total.Start();

    logbench("Starting: %s", filePath);

    MillisecondTimer t;
    BaseEngine *engine;
    t.Start();
    if (XpsEngine::IsSupportedFile(filePath))
        engine = XpsEngine::CreateFromFileName(filePath);
    if (DjVuEngine::IsSupportedFile(filePath))
        engine = DjVuEngine::CreateFromFileName(filePath);
    else
        engine = PdfEngine::CreateFromFileName(filePath);
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
    while (!Str::IsEmpty(pagesSpec)) {
        int start, end;
        pagesSpec = GetRange(pagesSpec, &start, &end);
        for (int j = start; j <= end; j++) {
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
    gLog = new Log::StderrLogger();
    // Log::Initialize();
    // Log::StderrLogger logger;
    // Log::AddLogger(&logger);

    size_t n = filesToBench.Count() / 2;
    for (size_t i = 0; i < n; i++)
        BenchFile(filesToBench[2*i], filesToBench[2*i + 1]);

    delete gLog;
    // Log::RemoveLogger(&logger);
    // Log::Destroy();
}
