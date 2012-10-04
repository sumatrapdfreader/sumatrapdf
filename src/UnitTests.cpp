/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifdef DEBUG

#include "BaseUtil.h"
#include "AppTools.h"
#include "ParseCommandLine.h"
#include "StressTesting.h"
#include "WinUtil.h"

static void hexstrTest()
{
    unsigned char buf[6] = { 1, 2, 33, 255, 0, 18 };
    unsigned char buf2[6] = { 0 };
    char *s = _MemToHex(&buf);
    assert(str::Eq(s, "010221ff0012"));
    bool ok = _HexToMem(s, &buf2);
    assert(ok);
    assert(memeq(buf, buf2, sizeof(buf)));
    free(s);

    FILETIME ft1, ft2;
    GetSystemTimeAsFileTime(&ft1);
    s = _MemToHex(&ft1);
    _HexToMem(s, &ft2);
    DWORD diff = FileTimeDiffInSecs(ft1, ft2);
    assert(0 == diff);
    assert(ft1.dwLowDateTime == ft2.dwLowDateTime);
    assert(ft1.dwHighDateTime == ft2.dwHighDateTime);
    free(s);
}

static void ParseCommandLineTest()
{
    {
        CommandLineInfo i;
        i.ParseCommandLine(_T("SumatraPDF.exe -bench foo.pdf"));
        assert(2 == i.pathsToBenchmark.Count());
        assert(str::Eq(_T("foo.pdf"), i.pathsToBenchmark.At(0)));
        assert(NULL == i.pathsToBenchmark.At(1));
    }

    {
        CommandLineInfo i;
        i.ParseCommandLine(_T("SumatraPDF.exe -bench foo.pdf -fwdsearch-width 5"));
        assert(i.fwdSearch.width == 5);
        assert(2 == i.pathsToBenchmark.Count());
        assert(str::Eq(_T("foo.pdf"), i.pathsToBenchmark.At(0)));
        assert(NULL == i.pathsToBenchmark.At(1));
    }

    {
        CommandLineInfo i;
        i.ParseCommandLine(_T("SumatraPDF.exe -bench bar.pdf loadonly"));
        assert(2 == i.pathsToBenchmark.Count());
        assert(str::Eq(_T("bar.pdf"), i.pathsToBenchmark.At(0)));
        assert(str::Eq(_T("loadonly"), i.pathsToBenchmark.At(1)));
    }

    {
        CommandLineInfo i;
        assert(i.colorRange[0] == WIN_COL_BLACK && i.colorRange[1] == WIN_COL_WHITE);
        i.ParseCommandLine(_T("SumatraPDF.exe -bench bar.pdf 1 -set-color-range 0x123456 #abCDef"));
        assert(i.colorRange[0] == RGB(0x12, 0x34, 0x56));
        assert(i.colorRange[1] == RGB(0xAB, 0xCD, 0xEF));
        assert(2 == i.pathsToBenchmark.Count());
        assert(str::Eq(_T("bar.pdf"), i.pathsToBenchmark.At(0)));
        assert(str::Eq(_T("1"), i.pathsToBenchmark.At(1)));
    }

    {
        CommandLineInfo i;
        i.ParseCommandLine(_T("SumatraPDF.exe -bench bar.pdf 1-5,3   -bench some.pdf 1,3,8-34"));
        assert(4 == i.pathsToBenchmark.Count());
        assert(str::Eq(_T("bar.pdf"), i.pathsToBenchmark.At(0)));
        assert(str::Eq(_T("1-5,3"), i.pathsToBenchmark.At(1)));
        assert(str::Eq(_T("some.pdf"), i.pathsToBenchmark.At(2)));
        assert(str::Eq(_T("1,3,8-34"), i.pathsToBenchmark.At(3)));
    }

    {
        CommandLineInfo i;
        assert(i.colorRange[0] == WIN_COL_BLACK && i.colorRange[1] == WIN_COL_WHITE);
        i.ParseCommandLine(_T("SumatraPDF.exe -presentation -bgcolor 0xaa0c13 foo.pdf -invert-colors bar.pdf"));
        assert(true == i.enterPresentation);
        assert(i.colorRange[0] == WIN_COL_WHITE && i.colorRange[1] == WIN_COL_BLACK);
        assert(1248426 == i.bgColor);
        assert(2 == i.fileNames.Count());
        assert(0 == i.fileNames.Find(_T("foo.pdf")));
        assert(1 == i.fileNames.Find(_T("bar.pdf")));
    }

    {
        CommandLineInfo i;
        assert(i.colorRange[0] == WIN_COL_BLACK && i.colorRange[1] == WIN_COL_WHITE);
        i.ParseCommandLine(_T("SumatraPDF.exe -bg-color 0xaa0c13 -invertcolors rosanna.pdf"));
        assert(i.colorRange[0] == WIN_COL_WHITE && i.colorRange[1] == WIN_COL_BLACK);
        assert(1248426 == i.bgColor);
        assert(1 == i.fileNames.Count());
        assert(0 == i.fileNames.Find(_T("rosanna.pdf")));
    }

    {
        CommandLineInfo i;
        i.ParseCommandLine(_T("SumatraPDF.exe \"foo \\\" bar \\\\.pdf\" un\\\"quoted.pdf"));
        assert(2 == i.fileNames.Count());
        assert(0 == i.fileNames.Find(_T("foo \" bar \\\\.pdf")));
        assert(1 == i.fileNames.Find(_T("un\\\"quoted.pdf")));
    }

    {
        CommandLineInfo i;
        i.ParseCommandLine(_T("SumatraPDF.exe -page 37 -view continuousfacing -zoom fitcontent -scroll 45,1234 -reuse-instance"));
        assert(0 == i.fileNames.Count());
        assert(i.pageNumber == 37);
        assert(i.startView == DM_CONTINUOUS_FACING);
        assert(i.startZoom == ZOOM_FIT_CONTENT);
        assert(i.startScroll.x == 45 && i.startScroll.y == 1234);
    }

    {
        CommandLineInfo i;
        i.ParseCommandLine(_T("SumatraPDF.exe -view \"single page\" -zoom 237.45 -scroll -21,-1"));
        assert(0 == i.fileNames.Count());
        assert(i.startView == DM_SINGLE_PAGE);
        assert(i.startZoom == 237.45f);
        assert(i.startScroll.x == -21 && i.startScroll.y == -1);
    }
}

static void versioncheck_test()
{
    assert(IsValidProgramVersion("1"));
    assert(IsValidProgramVersion("1.1"));
    assert(IsValidProgramVersion("1.1.1\r\n"));
    assert(IsValidProgramVersion("2662"));

    assert(!IsValidProgramVersion("1.1b"));
    assert(!IsValidProgramVersion("1..1"));
    assert(!IsValidProgramVersion("1.1\r\n.1"));

    assert(CompareVersion(_T("0.9.3.900"), _T("0.9.3")) > 0);
    assert(CompareVersion(_T("1.09.300"), _T("1.09.3")) > 0);
    assert(CompareVersion(_T("1.9.1"), _T("1.09.3")) < 0);
    assert(CompareVersion(_T("1.2.0"), _T("1.2")) == 0);
    assert(CompareVersion(_T("1.3.0"), _T("2662")) < 0);
}

static void BenchRangeTest()
{
    assert(IsBenchPagesInfo(_T("1")));
    assert(IsBenchPagesInfo(_T("2-4")));
    assert(IsBenchPagesInfo(_T("5,7")));
    assert(IsBenchPagesInfo(_T("6,8,")));
    assert(IsBenchPagesInfo(_T("1-3,4,6-9,13")));
    assert(IsBenchPagesInfo(_T("2-")));
    assert(IsBenchPagesInfo(_T("loadonly")));

    assert(!IsBenchPagesInfo(_T("")));
    assert(!IsBenchPagesInfo(_T("-2")));
    assert(!IsBenchPagesInfo(_T("2--4")));
    assert(!IsBenchPagesInfo(_T("4-2")));
    assert(!IsBenchPagesInfo(_T("1-3,loadonly")));
    assert(!IsBenchPagesInfo(NULL));
}

static void UrlExtractTest()
{
    assert(!ExtractFilenameFromURL(_T("")));
    assert(!ExtractFilenameFromURL(_T("#hash_only")));
    assert(!ExtractFilenameFromURL(_T("?query=only")));
    ScopedMem<TCHAR> filename(ExtractFilenameFromURL(_T("http://example.net/filename.ext")));
    assert(str::Eq(filename, _T("filename.ext")));
    filename.Set(ExtractFilenameFromURL(_T("http://example.net/filename.ext#with_hash")));
    assert(str::Eq(filename, _T("filename.ext")));
    filename.Set(ExtractFilenameFromURL(_T("http://example.net/path/to/filename.ext?more=data")));
    assert(str::Eq(filename, _T("filename.ext")));
    filename.Set(ExtractFilenameFromURL(_T("http://example.net/pa%74h/na%2f%6d%65%2ee%78t")));
    assert(str::Eq(filename, _T("na/me.ext")));
#ifdef UNICODE
    filename.Set(ExtractFilenameFromURL(_T("http://example.net/%E2%82%AC")));
    assert(str::Eq((char *)filename.Get(), "\xAC\x20"));
#endif
}

void SumatraPDF_UnitTests()
{
    hexstrTest();
    ParseCommandLineTest();
    versioncheck_test();
    BenchRangeTest();
    UrlExtractTest();
}

#endif
