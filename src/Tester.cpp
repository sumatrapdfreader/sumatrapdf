/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* A driver for various tests. The idea is that instead of having a separate
   executable and related makefile additions for each test, we have one test
   driver which dispatches desired test based on cmd-line arguments.
   Currently it only does one test: mobi file parsing. */

#include <stdio.h>
#include <stdlib.h>

#include "DirIter.h"
#include "DebugLog.h"
#include "FileUtil.h"
#include "HtmlPrettyPrint.h"
#include "MobiDoc.h"
#include "Mui.h"
#include "NoFreeAllocator.h"
#include "PageLayout.h"
#include "Scoped.h"
#include "Timer.h"
#include "WinUtil.h"

using namespace Gdiplus;
#include "GdiPlusUtil.h"

// if true, we'll save html content of a mobi ebook as well
// as pretty-printed html to MOBI_SAVE_DIR. The name will be
// ${file}.html and ${file}_pp.html
static bool gMobiSaveHtml = true;
// if true, we'll also save images in mobi files. The name
// will be ${file}_img_${imgNo}.[jpg|png]
// gMobiSaveHtml must be true as well
static bool gMobiSaveImages = true;
// directory to which we'll save mobi html and images
#define MOBI_SAVE_DIR _T("..\\ebooks-converted")

static int Usage()
{
    printf("Tester.exe\n");
    printf("  -mobi dirOrFile : run mobi tests in a given directory or for a given file\n");
    printf("  -mobilayout file : load and layout mobi file\n");
    return 1;
}

static void SaveMobiHtml(const TCHAR *filePathBase, MobiDoc *mb)
{
    CrashAlwaysIf(!gMobiSaveHtml);

    ScopedMem<TCHAR> outFile(str::Join(filePathBase, _T("_pp.html")));

    size_t htmlLen;
    const char *html = mb->GetBookHtmlData(htmlLen);
    size_t ppHtmlLen;
    char *ppHtml = PrettyPrintHtml(html, htmlLen, ppHtmlLen);
    file::WriteAll(outFile.Get(), ppHtml, ppHtmlLen);

    outFile.Set(str::Join(filePathBase, _T(".html")));
    file::WriteAll(outFile.Get(), html, htmlLen);
}

static void SaveMobiImage(const TCHAR *filePathBase, size_t imgNo, ImageData *img)
{
    // it's valid to not have image data at a given index
    if (!img || !img->data)
        return;
    const TCHAR *ext = GfxFileExtFromData((char*)img->data, img->len);
    CrashAlwaysIf(!ext);
    ScopedMem<TCHAR> fileName(str::Format(_T("%s_img_%d%s"), filePathBase, imgNo, ext));
    file::WriteAll(fileName.Get(), img->data, img->len);
}

static void SaveMobiImages(const TCHAR *filePathBase, MobiDoc *mb)
{
    if (!gMobiSaveImages)
        return;
    for (size_t i = 0; i < mb->imagesCount; i++) {
        SaveMobiImage(filePathBase, i, mb->GetImage(i+1));
    }
}

static void TestMobiFile(const TCHAR *filePath)
{
    _tprintf(_T("Testing file '%s'\n"), filePath);
    MobiDoc *mb = MobiDoc::CreateFromFile(filePath);
    if (!mb) {
        printf(" error: failed to parse the file\n");
        return;
    }

    if (!gMobiSaveHtml)
        return;

    // Given the name of the name of source mobi file "${srcdir}/${file}.mobi"
    // construct a base name for extracted html/image files in the form
    // "${MOBI_SAVE_DIR}/${file}" i.e. change dir to MOBI_SAVE_DIR and
    // remove the file extension
    TCHAR *dir = MOBI_SAVE_DIR;
    dir::CreateAll(dir);
    ScopedMem<TCHAR> fileName(str::Dup(path::GetBaseName(filePath)));
    ScopedMem<TCHAR> filePathBase(path::Join(dir, fileName));
    TCHAR *ext = (TCHAR*)str::FindCharLast(filePathBase.Get(), '.');
    *ext = 0;

    SaveMobiHtml(filePathBase, mb);
    SaveMobiImages(filePathBase, mb);

    delete mb;
}

static bool IsMobiFile(const TCHAR *f)
{
    // TODO: also .prc and .pdb ?
    return str::EndsWithI(f, _T(".mobi")) ||
           str::EndsWithI(f, _T(".azw")) ||
           str::EndsWithI(f, _T(".azw1"));
}

static void MobiTestDir(TCHAR *dir)
{
    _tprintf(_T("Testing mobi files in '%s'\n"), dir);
    DirIter di;
    if (!di.Start(dir, true)) {
        _tprintf(_T("Error: invalid directory '%s'\n"), dir);
        return;
    }

    for (;;) {
        const TCHAR *p = di.Next();
        if (NULL == p)
            break;
        if (IsMobiFile(p))
            TestMobiFile(p);
    }
}

static void MobiTest(char *dirOrFile)
{
    TCHAR *tmp = nf::str::conv::FromAnsi(dirOrFile);

    if (file::Exists(tmp) && IsMobiFile(tmp))
        TestMobiFile(tmp);
    else
        MobiTestDir(tmp);
}

// This loads and layouts a given mobi file. Used for profiling layout process.
static void MobiLayout(char *file)
{
    nf::AllocatorMark mark;
    TCHAR *tmp = nf::str::conv::FromAnsi(file);
    if (!file::Exists(tmp) || !IsMobiFile(tmp)) {
        printf("MobiLayout: file %s doesn't exist or not a mobi file", file);
        return;
    }
    printf("Laying out file '%s'\n", file);
    MobiDoc *mb = MobiDoc::CreateFromFile(tmp);
    if (!mb) {
        printf("MobiLayout: failed to parse the file\n");
        return;
    }

    PoolAllocator textAllocator;

    LayoutInfo li;
    li.pageDx = 640;
    li.pageDy = 480;
    li.fontName = L"Tahoma";
    li.fontSize = 12;
    li.htmlStr = mb->GetBookHtmlData(li.htmlStrLen);
    li.textAllocator = &textAllocator;

    PageLayoutMobi pl(&li, mb);
    Vec<PageData*> *pages = pl.Layout();
    DeleteVecMembers<PageData*>(*pages);
    delete pages;
    delete mb;
}

extern "C"
int main(int argc, char **argv)
{
    nf::AllocatorMark allocatorMark;

    int i = 1;
    int left = argc - 1;

    if (left < 2)
        return Usage();

    if (str::Eq(argv[i], "-mobi")) {
        ++i; --left;
        if (1 != left)
            return Usage();
        MobiTest(argv[i]);
        return 0;
    }

    if (str::Eq(argv[i], "-mobilayout")) {
        ++i; --left;
        if (1 != left)
            return Usage();
        void *test = nf::alloc(50);
        InitAllCommonControls();
        ScopedGdiPlus gdi;
        mui::Initialize();
        Timer t(true);
        MobiLayout(argv[i]);
        printf("Spent %.2f ms laying out %s", t.GetTimeInMs(), argv[i]);
        mui::Destroy();
        return 0;
    }

    return Usage();
}
