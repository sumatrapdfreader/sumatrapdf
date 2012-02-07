/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// TODO: Merge into src/Tester.cpp

#include "BaseUtil.h"
#include "Scoped.h"
#include "FileUtil.h"
#include "WinUtil.h"
#include "DirIter.h"
#include "CmdLineParser.h"
#include "HtmlPullParser.h"

#include "MobiDoc.h"
#include "EpubDoc.h"
#include "Fb2Doc.h"
#include "PageLayout.h"
#include "GdiPlusUtil.h"

// If set, we'll save the html content of an ebook as well
// as pretty-printed html and all contained image files to
// gExtractDir. The names will be ${file}.html, ${file}_pp.html
// and ${file}_img_${id}.[jpg|png|gif].
const TCHAR *gExtractDir = NULL;

static int Usage()
{
    printf("Tester.exe\n");
    printf("  -parse dirOrFile : run ebook tests in a given directory or for a given file\n");
    printf("  -extractto dir   : extract the contents of all -doc tests to dir\n");
    printf("  -layout file     : load and layout ebook file (for performance testing)\n");
    return 1;
}

inline bool IsSupported(const TCHAR *filePath)
{
    return MobiDoc2::IsSupported(filePath) ||
           EpubDoc::IsSupported(filePath)  ||
           Fb2Doc::IsSupported(filePath);
}

inline BaseEbookDoc *ParseEbook(const TCHAR *filePath)
{
    if (MobiDoc2::IsSupported(filePath))
        return MobiDoc2::ParseFile(filePath);
    if (EpubDoc::IsSupported(filePath))
        return EpubDoc::ParseFile(filePath);
    if (Fb2Doc::IsSupported(filePath))
        return Fb2Doc::ParseFile(filePath);
    return NULL;
}

static void SaveFileContents(const TCHAR *filePathBase, BaseEbookDoc *mb)
{
    size_t htmlLen;
    const char *html = mb->GetBookHtmlData(htmlLen);

    ScopedMem<TCHAR> fileName(str::Format(_T("%s.html"), filePathBase));
    file::WriteAll(fileName.Get(), (char *)html, htmlLen);

    fileName.Set(str::Format(_T("%s_pp.html"), filePathBase));
    ScopedMem<char> pp(PrettyPrintHtml(html, htmlLen, htmlLen));
    file::WriteAll(fileName, pp, htmlLen);

    ImageData2 *img;
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

static void ParseSingleFile(const TCHAR *filePath)
{
    _tprintf(_T("Testing file '%s'\n"), filePath);
    BaseEbookDoc *mb = ParseEbook(filePath);
    if (!mb) {
        printf(" error: failed to parse the file\n");
        return;
    }

    if (!str::IsEmpty(gExtractDir)) {
        dir::CreateAll(gExtractDir);
        ScopedMem<TCHAR> filePathBase(path::Join(gExtractDir, path::GetBaseName(mb->GetFilepath())));
        *(TCHAR *)path::GetExt(filePathBase) = '\0';
        SaveFileContents(filePathBase, mb);
    }

    delete mb;
}

static void ParseFilesInDir(const TCHAR *dir)
{
    _tprintf(_T("Testing files in '%s'\n"), dir);
    DirIter files;
    if (!files.Start(dir, true)) {
        _tprintf(_T("Error: invalid directory '%s'\n"), dir);
        return;
    }

    const TCHAR *p;
    while ((p = files.Next()))
        ParseSingleFile(p);
}

static void ParserTest(const TCHAR *dirOrFile)
{
    if (file::Exists(dirOrFile) && IsSupported(dirOrFile))
        ParseSingleFile(dirOrFile);
    else if (dir::Exists(dirOrFile))
        ParseFilesInDir(dirOrFile);
}

static void LayoutTest(const TCHAR *file)
{
    if (!file::Exists(file) || !IsSupported(file)) {
        _tprintf(_T("LayoutTest: file '%s' doesn't exist or not a supported file"), file);
        return;
    }
    BaseEbookDoc *doc = ParseEbook(file);
    if (!doc) {
        printf("LayoutTest: failed to parse the file\n");
        return;
    }
    _tprintf(_T("Laying out file '%s'\n"), doc->GetFilepath());

    LayoutInfo li;
    li.doc = doc;
    li.htmlStr = doc->GetBookHtmlData(li.htmlStrLen);
    li.pageSize = SizeI(640, 480);
    li.fontName = L"Tahoma";
    li.fontSize = 12;

    InitAllCommonControls();
    ScopedGdiPlus gdi;

    FontCache fontCache;
    LayoutHtml(&li, &fontCache);

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

    if (cmdLine.Count() < 2)
        return Usage();

    for (size_t i = 1; i < cmdLine.Count(); i += 2) {
        if (i + 1 == cmdLine.Count())
            return Usage();
        const TCHAR *arg = cmdLine.At(i);
        const TCHAR *param = cmdLine.At(i + 1);
        if (str::Eq(arg, _T("-parse")))
            ParserTest(param);
        else if (str::Eq(arg, _T("-layout")))
            LayoutTest(param);
        else if (str::Eq(arg, _T("-extractto")))
            gExtractDir = param;
        else
            return Usage();
    }

    return 0;
}
