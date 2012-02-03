/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* A driver for various tests. The idea is that instead of having a separate
   executable and related makefile additions for each test, we have one test
   driver which dispatches desired test based on cmd-line arguments.
   Currently it only does one test: mobi file parsing. */

#include <stdio.h>
#include <stdlib.h>

#include "Scoped.h"
#include "DirIter.h"
#include "MobiDoc.h"
#include "FileUtil.h"
#include "WinUtil.h"
#include "PageLayout.h"
#include "Mui.h"

using namespace Gdiplus;
#include "GdiPlusUtil.h"

// if true, we'll save html content of a mobi ebook as well
// as pretty-printed html to MOBI_SAVE_DIR. The name will be
// ${file}.html and ${file}_pp.html
static bool gMobiSaveHtml = false;
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

#if 0 // TODO: we've changed how pretty-printing works, need to update
    ScopedMem<TCHAR> outFile(str::Join(filePathBase, _T("_pp.html")));
    file::WriteAll(outFile.Get(), (void*)mb->prettyPrintedHtml->LendData(), mb->prettyPrintedHtml->Count());

    outFile.Set(str::Join(filePathBase, _T(".html")));
    file::WriteAll(outFile.Get(), mb->doc->LendData(), mb->doc->Count());
#endif
}

static void SaveMobiImage(const TCHAR *filePathBase, size_t imgNo, ImageData *img)
{
    // it's valid to not have image data at a given index
    if (!img->imgData)
        return;
    const TCHAR *ext = GfxFileExtFromData((char*)img->imgData, img->imgDataLen);
    CrashAlwaysIf(!ext);
    ScopedMem<TCHAR> fileName(str::Format(_T("%s_img_%d%s"), filePathBase, imgNo, ext));
    file::WriteAll(fileName.Get(), img->imgData, img->imgDataLen);
}

static void SaveMobiImages(const TCHAR *filePathBase, MobiDoc *mb)
{
    if (!gMobiSaveImages)
        return;
    for (size_t i = 0; i < mb->imagesCount; i++) {
        SaveMobiImage(filePathBase, i, mb->images + i);
    }
}

static void TestMobiFile(TCHAR *filePath)
{
    _tprintf(_T("Testing file '%s'\n"), filePath);
    MobiDoc *mb = MobiDoc::ParseFile(filePath);
    if (!mb) {
        printf(" error: failed to parse the file\n");
        return;
    }

    if (!gMobiSaveHtml)
        return;

#if 0 // TODO: use PrettyPrintHtml()
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
#endif

    delete mb;
}

static bool IsMobiFile(TCHAR *f)
{
    // TODO: also .prc and .pdb ?
    return str::EndsWithI(f, _T(".mobi")) ||
           str::EndsWithI(f, _T(".azw")) ||
           str::EndsWithI(f, _T(".azw1"));
}

static void MobiTestDir(TCHAR *dir)
{
    _tprintf(_T("Testing mobi files in '%s'\n"), dir);
    DirIter di(true);
    if (!di.Start(dir)) {
        _tprintf(_T("Error: invalid directory '%s'\n"), dir);
        return;
    }

    for (;;) {
        TCHAR *p = di.Next();
        if (NULL == p)
            break;
        if (IsMobiFile(p))
            TestMobiFile(p);
    }
}

static void MobiTest(char *dirOrFile)
{
    ScopedMem<TCHAR> tmp(str::conv::FromAnsi(dirOrFile));

    if (file::Exists(tmp) && IsMobiFile(tmp))
        TestMobiFile(tmp);
    else
        MobiTestDir(tmp);
}

// This loads and layouts a given mobi file. Used for profiling layout process.
static void MobiLayout(char *file)
{
    ScopedMem<TCHAR> tmp(str::conv::FromAnsi(file));
    if (!file::Exists(tmp) || !IsMobiFile(tmp)) {
        printf("MobiLayout: file %s doesn't exist or not a mobi file", file);
        return;
    }
    printf("Laying out file '%s'\n", file);
    MobiDoc *mb = MobiDoc::ParseFile(tmp.Get());
    if (!mb) {
        printf("MobiLayout: failed to parse the file\n");
        return;
    }

    LayoutInfo li;
    li.pageDx = 640;
    li.pageDy = 480;
    li.fontName = L"Tahoma";
    li.fontSize = 12;
    li.htmlStr = mb->GetBookHtmlData(li.htmlStrLen);

    LayoutHtml(&li);

    delete mb;
}

extern "C"
int main(int argc, char **argv)
{
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
        InitAllCommonControls();
        ScopedGdiPlus gdi;
        mui::Initialize();
        MobiLayout(argv[i]);
        mui::Destroy();
        return 0;
    }

    return Usage();
}
