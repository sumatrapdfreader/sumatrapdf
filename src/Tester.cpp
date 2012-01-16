/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* A driver for various tests. The idea is that instead of having a separate
   executable and related makefile additions for each test, we have one test
   driver which dispatches desired test based on cmd-line arguments.
   Currently it only does one test: mobi file parsing. */

#include <stdio.h>
#include <stdlib.h>

#include "DirIter.h"
#include "MobiParse.h"
#include "FileUtil.h"
#include "MobiHtmlParse.h"
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
    return 1;
}

static void SaveMobiHtml(const TCHAR *filePathBase, MobiParse *mb)
{
    CrashAlwaysIf(!gMobiSaveHtml);

    ScopedMem<TCHAR> outFile(str::Join(filePathBase, _T("_pp.html")));
    file::WriteAll(outFile.Get(), (void*)mb->prettyPrintedHtml->LendData(), mb->prettyPrintedHtml->Count());

    outFile.Set(str::Join(filePathBase, _T(".html")));
    file::WriteAll(outFile.Get(), mb->doc->LendData(), mb->doc->Count());
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

static void SaveMobiImages(const TCHAR *filePathBase, MobiParse *mb)
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
    MobiParse *mb = MobiParse::ParseFile(filePath);
    if (!mb) {
        printf(" error: failed to parse the file\n");
        return;
    }

    mb->ConvertToDisplayFormat();
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

extern "C"
int main(int argc, char **argv)
{
    int i = 1;
    int left = argc - 1;

    if (left < 2)
        return Usage();

    if (str::Eq(argv[i], "-mobi")) {
        ++i; --left;
        if (left != 1)
            return Usage();
        MobiTest(argv[i]);
        return 0;
    }

    return Usage();
}
