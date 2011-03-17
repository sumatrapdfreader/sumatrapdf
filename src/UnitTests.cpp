/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifdef DEBUG

#include "BaseUtil.h"
#include "TStrUtil.h"
#include "ParseCommandLine.h"
#include "AppTools.h"

extern DWORD FileTimeDiffInSecs(FILETIME *ft1, FILETIME *ft2);

static void hexstrTest()
{
    unsigned char buf[6] = {1, 2, 33, 255, 0, 18};
    unsigned char buf2[6] = {0};
    char *s = _MemToHex(&buf);
    assert(Str::Eq(s, "010221ff0012"));
    bool ok = _HexToMem(s, &buf2);
    assert(ok);
    assert(memcmp(buf, buf2, sizeof(buf)) == 0);
    free(s);

    FILETIME ft1, ft2;
    GetSystemTimeAsFileTime(&ft1);
    s = _MemToHex(&ft1);
    _HexToMem(s, &ft2);
    DWORD diff = FileTimeDiffInSecs(&ft1, &ft2);
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
        assert(2 == i.filesToBenchmark.Count());
        assert(Str::Eq(_T("foo.pdf"), i.filesToBenchmark.At(0)));
        assert(NULL == i.filesToBenchmark.At(1));
    }

    {
        CommandLineInfo i;
        i.ParseCommandLine(_T("SumatraPDF.exe -bench foo.pdf -fwdsearch-width 5"));
        assert(i.fwdsearchWidth == 5);
        assert(2 == i.filesToBenchmark.Count());
        assert(Str::Eq(_T("foo.pdf"), i.filesToBenchmark.At(0)));
        assert(NULL == i.filesToBenchmark.At(1));
    }

    {
        CommandLineInfo i;
        i.ParseCommandLine(_T("SumatraPDF.exe -bench bar.pdf loadonly"));
        assert(2 == i.filesToBenchmark.Count());
        assert(Str::Eq(_T("bar.pdf"), i.filesToBenchmark.At(0)));
        assert(Str::Eq(_T("loadonly"), i.filesToBenchmark.At(1)));
    }

    {
        CommandLineInfo i;
        i.ParseCommandLine(_T("SumatraPDF.exe -bench bar.pdf 1 -invert-colors"));
        assert(TRUE == i.invertColors);
        assert(2 == i.filesToBenchmark.Count());
        assert(Str::Eq(_T("bar.pdf"), i.filesToBenchmark.At(0)));
        assert(Str::Eq(_T("1"), i.filesToBenchmark.At(1)));
    }

    {
        CommandLineInfo i;
        i.ParseCommandLine(_T("SumatraPDF.exe -bench bar.pdf 1-5,3   -bench some.pdf 1,3,8-34"));
        assert(4 == i.filesToBenchmark.Count());
        assert(Str::Eq(_T("bar.pdf"), i.filesToBenchmark.At(0)));
        assert(Str::Eq(_T("1-5,3"), i.filesToBenchmark.At(1)));
        assert(Str::Eq(_T("some.pdf"), i.filesToBenchmark.At(2)));
        assert(Str::Eq(_T("1,3,8-34"), i.filesToBenchmark.At(3)));
    }

    {
        CommandLineInfo i;
        i.ParseCommandLine(_T("SumatraPDF.exe -presentation -bgcolor 0xaa0c13 foo.pdf -invert-colors bar.pdf"));
        assert(true == i.enterPresentation);
        assert(TRUE == i.invertColors);
        assert(1248426 == i.bgColor);
        assert(2 == i.fileNames.Count());
        assert(0 == i.fileNames.Find(_T("foo.pdf")));
        assert(1 == i.fileNames.Find(_T("bar.pdf")));
    }

    {
        CommandLineInfo i;
        i.ParseCommandLine(_T("SumatraPDF.exe -bg-color 0xaa0c13 -invertcolors rosanna.pdf"));
        assert(TRUE == i.invertColors);
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

void SumatraPDF_UnitTests(void)
{
    DBG_OUT("Running SumatraPDF unit tests\n");
    hexstrTest();
    ParseCommandLineTest();
    versioncheck_test();
}

#endif
