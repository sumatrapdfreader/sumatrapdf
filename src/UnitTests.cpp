/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "AppUtil.h"
#include "FileUtil.h"
#include "WinUtil.h"
#include "StrFormat.h"

// must be last due to assert() over-write
#include "UtAssert.h"

// TODO: those bring too many dependencies from the rest of Sumatra code
// either have to pull all the code in or make it more independent

#if 0
#include "ParseCommandLine.h"
#include "StressTesting.h"

static void ParseCommandLineTest()
{
    {
        CommandLineInfo i;
        i.ParseCommandLine(L"SumatraPDF.exe -bench foo.pdf");
        utassert(2 == i.pathsToBenchmark.Count());
        utassert(str::Eq(L"foo.pdf", i.pathsToBenchmark.At(0)));
        utassert(NULL == i.pathsToBenchmark.At(1));
    }

    {
        CommandLineInfo i;
        i.ParseCommandLine(L"SumatraPDF.exe -bench foo.pdf -fwdsearch-width 5");
        utassert(5 == i.forwardSearch.highlightWidth);
        utassert(2 == i.pathsToBenchmark.Count());
        utassert(str::Eq(L"foo.pdf", i.pathsToBenchmark.At(0)));
        utassert(NULL == i.pathsToBenchmark.At(1));
    }

    {
        CommandLineInfo i;
        i.ParseCommandLine(L"SumatraPDF.exe -bench bar.pdf loadonly");
        utassert(2 == i.pathsToBenchmark.Count());
        utassert(str::Eq(L"bar.pdf", i.pathsToBenchmark.At(0)));
        utassert(str::Eq(L"loadonly", i.pathsToBenchmark.At(1)));
    }

    {
        CommandLineInfo i;
        utassert(i.textColor == WIN_COL_BLACK && i.backgroundColor == WIN_COL_WHITE);
        i.ParseCommandLine(L"SumatraPDF.exe -bench bar.pdf 1 -set-color-range 0x123456 #abCDef");
        utassert(i.textColor == RGB(0x12, 0x34, 0x56));
        utassert(i.backgroundColor == RGB(0xAB, 0xCD, 0xEF));
        utassert(2 == i.pathsToBenchmark.Count());
        utassert(str::Eq(L"bar.pdf", i.pathsToBenchmark.At(0)));
        utassert(str::Eq(L"1", i.pathsToBenchmark.At(1)));
    }

    {
        CommandLineInfo i;
        i.ParseCommandLine(L"SumatraPDF.exe -bench bar.pdf 1-5,3   -bench some.pdf 1,3,8-34");
        utassert(4 == i.pathsToBenchmark.Count());
        utassert(str::Eq(L"bar.pdf", i.pathsToBenchmark.At(0)));
        utassert(str::Eq(L"1-5,3", i.pathsToBenchmark.At(1)));
        utassert(str::Eq(L"some.pdf", i.pathsToBenchmark.At(2)));
        utassert(str::Eq(L"1,3,8-34", i.pathsToBenchmark.At(3)));
    }

    {
        CommandLineInfo i;
        utassert(i.textColor == WIN_COL_BLACK && i.backgroundColor == WIN_COL_WHITE);
        i.ParseCommandLine(L"SumatraPDF.exe -presentation -bgcolor 0xaa0c13 foo.pdf -invert-colors bar.pdf");
        utassert(true == i.enterPresentation);
        utassert(i.textColor == WIN_COL_WHITE && i.backgroundColor == WIN_COL_BLACK);
        utassert(1248426 == i.bgColor);
        utassert(2 == i.fileNames.Count());
        utassert(0 == i.fileNames.Find(L"foo.pdf"));
        utassert(1 == i.fileNames.Find(L"bar.pdf"));
    }

    {
        CommandLineInfo i;
        utassert(i.textColor == WIN_COL_BLACK && i.backgroundColor == WIN_COL_WHITE);
        i.ParseCommandLine(L"SumatraPDF.exe -bg-color 0xaa0c13 -invertcolors rosanna.pdf");
        utassert(i.textColor == WIN_COL_WHITE && i.backgroundColor == WIN_COL_BLACK);
        utassert(1248426 == i.bgColor);
        utassert(1 == i.fileNames.Count());
        utassert(0 == i.fileNames.Find(L"rosanna.pdf"));
    }

    {
        CommandLineInfo i;
        i.ParseCommandLine(L"SumatraPDF.exe \"foo \\\" bar \\\\.pdf\" un\\\"quoted.pdf");
        utassert(2 == i.fileNames.Count());
        utassert(0 == i.fileNames.Find(L"foo \" bar \\\\.pdf"));
        utassert(1 == i.fileNames.Find(L"un\\\"quoted.pdf"));
    }

    {
        CommandLineInfo i;
        i.ParseCommandLine(L"SumatraPDF.exe -page 37 -view continuousfacing -zoom fitcontent -scroll 45,1234 -reuse-instance");
        utassert(0 == i.fileNames.Count());
        utassert(i.pageNumber == 37);
        utassert(i.startView == DM_CONTINUOUS_FACING);
        utassert(i.startZoom == ZOOM_FIT_CONTENT);
        utassert(i.startScroll.x == 45 && i.startScroll.y == 1234);
    }

    {
        CommandLineInfo i;
        i.ParseCommandLine(L"SumatraPDF.exe -view \"single page\" -zoom 237.45 -scroll -21,-1");
        utassert(0 == i.fileNames.Count());
        utassert(i.startView == DM_SINGLE_PAGE);
        utassert(i.startZoom == 237.45f);
        utassert(i.startScroll.x == -21 && i.startScroll.y == -1);
    }
}

static void BenchRangeTest()
{
    utassert(IsBenchPagesInfo(L"1"));
    utassert(IsBenchPagesInfo(L"2-4"));
    utassert(IsBenchPagesInfo(L"5,7"));
    utassert(IsBenchPagesInfo(L"6,8,"));
    utassert(IsBenchPagesInfo(L"1-3,4,6-9,13"));
    utassert(IsBenchPagesInfo(L"2-"));
    utassert(IsBenchPagesInfo(L"loadonly"));

    utassert(!IsBenchPagesInfo(L""));
    utassert(!IsBenchPagesInfo(L"-2"));
    utassert(!IsBenchPagesInfo(L"2--4"));
    utassert(!IsBenchPagesInfo(L"4-2"));
    utassert(!IsBenchPagesInfo(L"1-3,loadonly"));
    utassert(!IsBenchPagesInfo(NULL));
}
#endif

static void versioncheck_test()
{
    utassert(IsValidProgramVersion("1"));
    utassert(IsValidProgramVersion("1.1"));
    utassert(IsValidProgramVersion("1.1.1\r\n"));
    utassert(IsValidProgramVersion("2662"));

    utassert(!IsValidProgramVersion("1.1b"));
    utassert(!IsValidProgramVersion("1..1"));
    utassert(!IsValidProgramVersion("1.1\r\n.1"));

    utassert(CompareVersion(L"0.9.3.900", L"0.9.3") > 0);
    utassert(CompareVersion(L"1.09.300", L"1.09.3") > 0);
    utassert(CompareVersion(L"1.9.1", L"1.09.3") < 0);
    utassert(CompareVersion(L"1.2.0", L"1.2") == 0);
    utassert(CompareVersion(L"1.3.0", L"2662") < 0);
}

static void UrlExtractTest()
{
    // TODO: move to StrUtil_ut.cpp
    utassert(!url::GetFileName(L""));
    utassert(!url::GetFileName(L"#hash_only"));
    utassert(!url::GetFileName(L"?query=only"));
    ScopedMem<WCHAR> fileName(url::GetFileName(L"http://example.net/filename.ext"));
    utassert(str::Eq(fileName, L"filename.ext"));
    fileName.Set(url::GetFileName(L"http://example.net/filename.ext#with_hash"));
    utassert(str::Eq(fileName, L"filename.ext"));
    fileName.Set(url::GetFileName(L"http://example.net/path/to/filename.ext?more=data"));
    utassert(str::Eq(fileName, L"filename.ext"));
    fileName.Set(url::GetFileName(L"http://example.net/pa%74h/na%2f%6d%65%2ee%78t"));
    utassert(str::Eq(fileName, L"na/me.ext"));
    fileName.Set(url::GetFileName(L"http://example.net/%E2%82%AC"));
    utassert(str::Eq((char *)fileName.Get(), "\xAC\x20"));
}

static void hexstrTest()
{
    unsigned char buf[6] = { 1, 2, 33, 255, 0, 18 };
    unsigned char buf2[6] = { 0 };
    ScopedMem<char> s(_MemToHex(&buf));
    utassert(str::Eq(s, "010221ff0012"));
    bool ok = _HexToMem(s, &buf2);
    utassert(ok);
    utassert(memeq(buf, buf2, sizeof(buf)));

    FILETIME ft1, ft2;
    GetSystemTimeAsFileTime(&ft1);
    s.Set(_MemToHex(&ft1));
    _HexToMem(s, &ft2);
    DWORD diff = FileTimeDiffInSecs(ft1, ft2);
    utassert(0 == diff);
    utassert(FileTimeEq(ft1, ft2));

    s.Set(str::MemToHex(NULL, 0));
    utassert(str::Eq(s, ""));
    ok = str::HexToMem(s, NULL, 0);
    utassert(ok);
}

void SumatraPDF_UnitTests()
{
#if 0
    ParseCommandLineTest();
    BenchRangeTest();
#endif
    versioncheck_test();
    UrlExtractTest();
    hexstrTest();
}
