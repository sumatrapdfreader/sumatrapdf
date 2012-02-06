/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* A driver for various tests. The idea is that instead of having a separate
   executable and related makefile additions for each test, we have one test
   driver which dispatches desired test based on cmd-line arguments.
   Currently it only does one test: mobi file parsing. */

#include "BaseUtil.h"
#include "NoFreeAllocator.h"
#include "Scoped.h"
#include "DirIter.h"
#include "MobiDoc.h"
#include "EpubDoc.h"
#include "Fb2Doc.h"
#include "FileUtil.h"
#include "WinUtil.h"
#include "PageLayout.h"
#include "CmdLineParser.h"
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
    printf("  -doc dirOrFile   : run ebook tests in a given directory or for a given file\n");
    printf("  -mobi dirOrFile  : run ebook tests only for mobi files\n");
    printf("  -layout file     : load and layout ebook file\n");
    printf("  -mobilayout file : same as -layout\n");
    return 1;
}

static void SaveMobiHtml(const TCHAR *filePathBase, BaseEbookDoc *mb)
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
    if (!img->data)
        return;
    const TCHAR *ext = GfxFileExtFromData(img->data, img->len);
    CrashAlwaysIf(!ext);
    ScopedMem<TCHAR> fileName(str::Format(_T("%s_img_%d%s"), filePathBase, imgNo, ext));
    file::WriteAll(fileName.Get(), img->data, img->len);
}

static void SaveMobiImages(const TCHAR *filePathBase, BaseEbookDoc *mb)
{
    if (!gMobiSaveImages)
        return;
    ImageData *image;
    for (size_t i = 0; (image = mb->GetImageData(i)); i++) {
        SaveMobiImage(filePathBase, i, image);
    }
}

static bool IsSupported(const TCHAR *filePath, bool allEbooks=true)
{
    return MobiDoc::IsSupported(filePath) ||
           allEbooks && (EpubDoc::IsSupported(filePath) || Fb2Doc::IsSupported(filePath));
}

static BaseEbookDoc *ParseEbook(const TCHAR *filePath, bool allEbooks=true)
{
    if (allEbooks) {
        if (EpubDoc::IsSupported(filePath))
            return EpubDoc::ParseFile(filePath);
        if (Fb2Doc::IsSupported(filePath))
            return Fb2Doc::ParseFile(filePath);
    }
    return MobiDoc::ParseFile(filePath);
}

static void TestMobiFile(const TCHAR *filePath, bool allEbooks)
{
    _tprintf(_T("Testing file '%s'\n"), filePath);
    BaseEbookDoc *mb = ParseEbook(filePath, allEbooks);
    if (!mb) {
        printf(" error: failed to parse the file\n");
        return;
    }

    if (!gMobiSaveHtml) {
        delete mb;
        return;
    }

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

static void MobiTestDir(const TCHAR *dir, bool allEbooks)
{
    _tprintf(_T("Testing mobi files in '%s'\n"), dir);
    DirIter di;
    if (!di.Start(dir, true)) {
        _tprintf(_T("Error: invalid directory '%s'\n"), dir);
        return;
    }

    const TCHAR *p;
    while ((p = di.Next())) {
        if (IsSupported(p, allEbooks))
            TestMobiFile(p, allEbooks);
    }
}

static void MobiTest(const TCHAR *dirOrFile, bool allEbooks=false)
{
    if (file::Exists(dirOrFile) && IsSupported(dirOrFile, allEbooks))
        TestMobiFile(dirOrFile, allEbooks);
    else if (dir::Exists(dirOrFile))
        MobiTestDir(dirOrFile, allEbooks);
}

// This loads and layouts a given mobi file. Used for profiling layout process.
static void MobiLayout(const TCHAR *file)
{
    nf::AllocatorMark mark;
    if (!file::Exists(file) || !IsSupported(file)) {
        _tprintf(_T("MobiLayout: file %s doesn't exist or not a mobi file"), file);
        return;
    }
    BaseEbookDoc *doc = ParseEbook(file);
    if (!doc) {
        printf("MobiLayout: failed to parse the file\n");
        return;
    }
    _tprintf(_T("Laying out file '%s'\n"), doc->GetFilepath());

    LayoutInfo li;
    li.pageDx = 640;
    li.pageDy = 480;
    li.fontName = L"Tahoma";
    li.fontSize = 12;
    li.htmlStr = doc->GetBookHtmlData(li.htmlStrLen);

    Vec<PageData *> *data = LayoutHtml(&li);
    DeleteVecMembers(*data);
    delete data;

    delete doc;
}

int main(int argc, char **argv)
{
#ifdef DEBUG
    // report memory leaks on stderr
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    CmdLineParser cmdLine(GetCommandLine());
    nf::AllocatorMark allocatorMark;

    if (cmdLine.Count() < 2)
        return Usage();

    for (size_t i = 1; i < cmdLine.Count(); i += 2) {
        if (i + 1 == cmdLine.Count())
            return Usage();
        const TCHAR *arg = cmdLine.At(i);
        const TCHAR *param = cmdLine.At(i + 1);
        if (str::Eq(arg, _T("-mobi")) || str::Eq(arg, _T("-doc"))) {
            bool onlyMobi = str::Eq(arg, _T("-mobi"));
            MobiTest(param, !onlyMobi);
        }
        else if (str::Eq(arg, _T("-layout")) || str::Eq(arg, _T("-mobilayout"))) {
            void *test = nf::alloc(50);
            InitAllCommonControls();
            ScopedGdiPlus gdi;
            mui::Initialize();
            MobiLayout(param);
            mui::Destroy();
        }
        else {
            return Usage();
        }
    }

    return 0;
}
