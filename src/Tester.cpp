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

// if set, we'll save html content of a mobi ebook as well
// as pretty-printed html and all contained image files to
// gExtractDir. The names will be ${file}.html, ${file}_pp.html
// and ${file}_img_${id}.[jpg|png|gif]
const TCHAR *gExtractDir = NULL;
// value to use for gExtractDir for -mobi (which used to be the default)
// TODO: remove when no longer needed
#define MOBI_SAVE_DIR _T("..\\ebooks-converted")

static int Usage()
{
    printf("Tester.exe\n");
    printf("  -doc dirOrFile   : run ebook tests in a given directory or for a given file\n");
    printf("  -mobi dirOrFile  : run ebook tests only for mobi files\n");
    printf("  -extractto dir   : extract the contents of all -doc/-mobi tests to dir\n");
    printf("  -layout file     : load and layout ebook file\n");
    printf("  -mobilayout file : same as -layout\n");
    return 1;
}

static void SaveMobiHtml(const TCHAR *filePathBase, BaseEbookDoc *mb)
{
    size_t htmlLen;
    const char *html = mb->GetBookHtmlData(htmlLen);

    ScopedMem<TCHAR> fileName(str::Format(_T("%s.html"), filePathBase));
    file::WriteAll(fileName.Get(), (char *)html, htmlLen);

    fileName.Set(str::Format(_T("%s_pp.html"), filePathBase));
    ScopedMem<char> pp(PrettyPrintHtml(html, htmlLen, htmlLen));
    file::WriteAll(fileName, pp, htmlLen);
}

static void SaveMobiImages(const TCHAR *filePathBase, BaseEbookDoc *mb)
{
    ImageData *img;
    for (size_t i = 0; (img = mb->GetImageData(i)); i++) {
        // it's valid to not have image data at a given index
        if (!img->data)
            continue;

        ScopedMem<TCHAR> id(img->id ? str::conv::FromUtf8(img->id) : str::Format(_T("%d"), i));
        str::TransChars(id, _T("\\/:*?\"<>|"), _T("_________"));
        const TCHAR *ext = GfxFileExtFromData(img->data, img->len);
        CrashAlwaysIf(!ext);
        ScopedMem<TCHAR> fileName(str::Format(_T("%s_img_%s%s"), filePathBase, id, ext));
        file::WriteAll(fileName.Get(), img->data, img->len);
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

    if (!str::IsEmpty(gExtractDir) || !allEbooks) {
        // Given the name of the name of source mobi file "${srcdir}/${file}.mobi"
        // construct a base name for extracted html/image files in the form
        // "${gExtractDir}/${file}"
        const TCHAR *saveDir = !str::IsEmpty(gExtractDir) ? gExtractDir : MOBI_SAVE_DIR;
        ScopedMem<TCHAR> filePathBase(path::Join(saveDir, path::GetBaseName(mb->GetFilepath())));
        *(TCHAR *)path::GetExt(filePathBase) = '\0';

        dir::CreateAll(gExtractDir);
        SaveMobiHtml(filePathBase, mb);
        SaveMobiImages(filePathBase, mb);
    }

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
        else if (str::Eq(arg, _T("-extractto"))) {
            gExtractDir = param;
        }
        else {
            return Usage();
        }
    }

    return 0;
}
