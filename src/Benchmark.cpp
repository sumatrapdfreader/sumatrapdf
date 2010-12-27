#include <windows.h>
#include <tchar.h>
#include "Benchmark.h"
#include "WinUtil.hpp"

extern "C" {
#include <fitz.h>
#include <mupdf.h>
}
#include "file_util.h"

static void logbenchhelp(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

#define logbench(...) logbenchhelp(__VA_ARGS__)

static pdf_xref *xref;
static int pagecount = 0;
static fz_glyphcache *drawcache;
static pdf_page *drawpage;

static void BenchCloseXref(void)
{
    if (!xref)
        return;

    if (xref->store)
    {
        pdf_freestore(xref->store);
        xref->store = 0;
    }
    pdf_freexref(xref);
    xref = 0;
}

static fz_error BenchOpenXref(char *filename, char *password)
{
    fz_error error;

    error = pdf_openxref(&xref, filename, password);
    if (error)
        return fz_rethrow(error, "pdf_openxref() failed");

    if (pdf_needspassword(xref))
    {
        int okay = pdf_authenticatepassword(xref, password);
        if (!okay)
        {
            logbench("Warning: pdf_setpassword() failed, incorrect password\n");
            return fz_throw("invalid password");
        }
    }

    error = pdf_loadpagetree(xref);
    if (error)
        return fz_rethrow(error, "cannot load page tree: %s", filename);

    pagecount = pdf_getpagecount(xref);

    return fz_okay;
}

static fz_error BenchLoadPage(int pagenum)
{
    fz_error error;
    fz_obj *pageobj;
    double timems;

    MillisecondTimer t;
    t.Start();
    pageobj = pdf_getpageobject(xref, pagenum);

    drawpage = 0;
    error = pdf_loadpage(&drawpage, xref, pageobj);
    t.Stop();
    if (error)
    {
        logbench("Error: failed to load page %d\n", pagenum);
        return error;
    }
    timems = t.GetTimeInMs();
    logbench("pageload   %3d: %.2f ms\n", pagenum, timems);
    return fz_okay;
}

static fz_error BenchRenderPage(int pagenum)
{
    fz_error error;
    fz_matrix ctm;
    fz_bbox bbox;
    fz_pixmap *pix;
    int w, h;
    double timems;
    fz_device *dev;

    MillisecondTimer t;
    t.Start();
    ctm = fz_identity;
    ctm = fz_concat(ctm, fz_translate(0, -drawpage->mediabox.y1));

    bbox = fz_roundrect(fz_transformrect(ctm, drawpage->mediabox));
    w = bbox.x1 - bbox.x0;
    h = bbox.y1 - bbox.y0;

    pix = fz_newpixmapwithrect(fz_devicergb, bbox);
    fz_clearpixmapwithcolor(pix, 0xFF);
    dev = fz_newdrawdevice(drawcache, pix);
    error = pdf_runpage(xref, drawpage, dev, ctm);
    fz_freedevice(dev);
    if (error)
    {
        logbench("Error: pdf_runcontentstream() failed\n");
        goto Exit;
    }

    t.Stop();
    timems = t.GetTimeInMs();

    logbench("pagerender %3d: %.2f ms\n", pagenum, timems);
Exit:
    fz_droppixmap(pix);
    return fz_okay;
}

static void BenchFreePage(void)
{
    pdf_freepage(drawpage);
    drawpage = 0;
}

static bool IsDigit(TCHAR c)
{
    return (c >= '0') && (c <= '9');
}

static TCHAR *GetNumber(TCHAR *s, int& n)
{
    n = 0;
    while (*s) {
        if (IsDigit(*s)) {
            int d = *s - '0';
            n = n * 10 + d;
        } else {
            if (*s == '-')
                return s+1;
            else
                return NULL;
        }
        s++;
    }
    return NULL;
}

// <s> can be in form "1" or "3-58". Error is returned by setting <start>
// and <end> to values that won't be valid to the caller
static TCHAR *GetRange(TCHAR *s, int& start, int& end)
{
    start = end = 0; // error case
    TCHAR *s2 = GetNumber(s, start);
    if (s2)
        GetNumber(s2, end);
    else
        end = start;
    return s + tstr_len(s) + 1;
}

static int RangesCount(TCHAR *s)
{
    int n = 1;
    while (*s) {
        if (*s == ',') {
            *s = 0;
            ++n;
        }
        s++;
    }
    return n;
}

static void BenchLoadRender(int page)
{
    fz_error error = BenchLoadPage(page);
    if (!error)
        BenchRenderPage(page);
    if (drawpage != NULL)
        BenchFreePage();
}

static void BenchFile(TCHAR *filePath, TCHAR *pagesSpec)
{
    fz_error error;
    double timems;
    int pages;

    BOOL loadOnly = tstr_ieq(_T("loadonly"), pagesSpec);
    char *pdffilename = tstr_to_utf8(filePath);

    if (!file_exists(filePath)) {
        logbench("Error: file %s doesn't exist\n", pdffilename);
        return;
    }

    drawcache = fz_newglyphcache();
    if (!drawcache)
    {
        logbench("Error: fz_newglyphcache() failed\n");
        goto Exit;
    }

    logbench("Starting: %s\n", pdffilename);

    MillisecondTimer t;
    t.Start();
    error = BenchOpenXref(pdffilename, "");
    t.Stop();
    if (error)
        goto Exit;
    timems = t.GetTimeInMs();
    logbench("load: %.2f ms\n", timems);

    pages = pagecount;
    logbench("page count: %d\n", pages);

    if (loadOnly)
        goto Exit;

    if (NULL == pagesSpec) {
        for (int i = 1; i <= pages; i++) {
            BenchLoadRender(i);
        }
    }

    int n = RangesCount(pagesSpec);
    for (int i = 0; i < n; i++) {
        int start, end;
        pagesSpec = GetRange(pagesSpec, start, end);
        for (int j = start; j <= end; j++) {
            if ((j >= 1) && (j <= pages)) {
                BenchLoadRender(j);
            }
        }
    }

Exit:
    logbench("Finished: %s\n", pdffilename);
    if (drawcache)
        fz_freeglyphcache(drawcache);
    BenchCloseXref();
    free(pdffilename);
}

void Bench(VStrList& filesToBench)
{
    int n = filesToBench.size() / 2;
    for (int i=0; i<n; i++)
    {
        BenchFile(filesToBench.at(2*i), filesToBench.at((2*i)+1));
    }
}

