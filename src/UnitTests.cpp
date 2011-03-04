#include "base_util.h"
#include "tstr_util.h"
#include "MemChunked.h"
#include "geom_util.h"
#include "benc_util.h"
#include "ParseCommandLine.h"
#include "AppTools.h"
#include "Vec.h"

#ifdef DEBUG
extern DWORD FileTimeDiffInSecs(FILETIME *ft1, FILETIME *ft2);

static void hexstrTest()
{
    unsigned char buf[6] = {1, 2, 33, 255, 0, 18};
    unsigned char buf2[6] = {0};
    char *s = _mem_to_hexstr(&buf);
    assert(str_eq(s, "010221ff0012"));
    BOOL ok = _hexstr_to_mem(s, &buf2);
    assert(ok);
    assert(memcmp(buf, buf2, sizeof(buf)) == 0);
    free(s);

    FILETIME ft1, ft2;
    GetSystemTimeAsFileTime(&ft1);
    s = _mem_to_hexstr(&ft1);
    _hexstr_to_mem(s, &ft2);
    DWORD diff = FileTimeDiffInSecs(&ft1, &ft2);
    assert(0 == diff);
    assert(ft1.dwLowDateTime == ft2.dwLowDateTime);
    assert(ft1.dwHighDateTime == ft2.dwHighDateTime);
    free(s);
}

static void MemChunkedTest()
{
    MemChunked *ms;
    DWORD size;
    char *data;

    char buf[2] = {'a', '\0'};
    ms = new MemChunked();
    for (int i=0; i<7; i++) {
        ms->AddChunk(buf, 1);
        buf[0] = buf[0] + 1;
    }
    data = (char*)ms->GetData(&size);
    delete ms;
    assert(str_eq("abcdefg", data));
    assert(7 == size);
    free(data);

    ms = new MemChunked();
    ms->AddChunk("a", 1);
    data = (char*)ms->GetData(&size);
    delete ms;
    assert(str_eq("a", data));
    assert(1 == size);
    free(data);
}

static void ParseCommandLineTest()
{
    {
        CommandLineInfo i;
        i.ParseCommandLine(_T("SumatraPDF.exe -bench foo.pdf"));
        assert(2 == i.filesToBenchmark.size());
        assert(tstr_eq(_T("foo.pdf"), i.filesToBenchmark.at(0)));
        assert(NULL == i.filesToBenchmark.at(1));
    }

    {
        CommandLineInfo i;
        i.ParseCommandLine(_T("SumatraPDF.exe -bench foo.pdf -fwdsearch-width 5"));
        assert(i.fwdsearchWidth == 5);
        assert(2 == i.filesToBenchmark.size());
        assert(tstr_eq(_T("foo.pdf"), i.filesToBenchmark.at(0)));
        assert(NULL == i.filesToBenchmark.at(1));
    }

    {
        CommandLineInfo i;
        i.ParseCommandLine(_T("SumatraPDF.exe -bench bar.pdf loadonly"));
        assert(2 == i.filesToBenchmark.size());
        assert(tstr_eq(_T("bar.pdf"), i.filesToBenchmark.at(0)));
        assert(tstr_eq(_T("loadonly"), i.filesToBenchmark.at(1)));
    }

    {
        CommandLineInfo i;
        i.ParseCommandLine(_T("SumatraPDF.exe -bench bar.pdf 1 -invert-colors"));
        assert(TRUE == i.invertColors);
        assert(2 == i.filesToBenchmark.size());
        assert(tstr_eq(_T("bar.pdf"), i.filesToBenchmark.at(0)));
        assert(tstr_eq(_T("1"), i.filesToBenchmark.at(1)));
    }

    {
        CommandLineInfo i;
        i.ParseCommandLine(_T("SumatraPDF.exe -bench bar.pdf 1-5,3   -bench some.pdf 1,3,8-34"));
        assert(4 == i.filesToBenchmark.size());
        assert(tstr_eq(_T("bar.pdf"), i.filesToBenchmark.at(0)));
        assert(tstr_eq(_T("1-5,3"), i.filesToBenchmark.at(1)));
        assert(tstr_eq(_T("some.pdf"), i.filesToBenchmark.at(2)));
        assert(tstr_eq(_T("1,3,8-34"), i.filesToBenchmark.at(3)));
    }

    {
        CommandLineInfo i;
        i.ParseCommandLine(_T("SumatraPDF.exe -presentation -bgcolor 0xaa0c13 foo.pdf -invert-colors bar.pdf"));
        assert(true == i.enterPresentation);
        assert(TRUE == i.invertColors);
        assert(1248426 == i.bgColor);
        assert(2 == i.fileNames.size());
        assert(0 == i.fileNames.find(_T("foo.pdf")));
        assert(1 == i.fileNames.find(_T("bar.pdf")));
    }

    {
        CommandLineInfo i;
        i.ParseCommandLine(_T("SumatraPDF.exe -bg-color 0xaa0c13 -invertcolors rosanna.pdf"));
        assert(TRUE == i.invertColors);
        assert(1248426 == i.bgColor);
        assert(1 == i.fileNames.size());
        assert(0 == i.fileNames.find(_T("rosanna.pdf")));
    }

    {
        CommandLineInfo i;
        i.ParseCommandLine(_T("SumatraPDF.exe \"foo \\\" bar \\\\.pdf\" un\\\"quoted.pdf"));
        assert(2 == i.fileNames.size());
        assert(0 == i.fileNames.find(_T("foo \" bar \\\\.pdf")));
        assert(1 == i.fileNames.find(_T("un\\\"quoted.pdf")));
    }
}

static void tstr_test()
{
    TCHAR buf[32];
    TCHAR *str = _T("a string");
    assert(tstr_len(str) == 8);
    assert(tstr_eq(str, _T("a string")) && tstr_eq(str, str));
    assert(!tstr_eq(str, NULL) && !tstr_eq(str, _T("A String")));
    assert(tstr_ieq(str, _T("A String")) && tstr_ieq(str, str));
    assert(!tstr_ieq(str, NULL) && tstr_ieq(NULL, NULL));
    assert(tstr_startswith(str, _T("a s")) && tstr_startswithi(str, _T("A Str")));
    assert(!tstr_startswith(str, _T("Astr")));
    assert(tstr_endswith(str, _T("ing")) && tstr_endswithi(str, _T("ING")));
    assert(!tstr_endswith(str, _T("ung")));
    assert(tstr_empty(NULL) && tstr_empty(_T("")) && !tstr_empty(str));
    assert(tstr_contains(str, _T('s')) && !tstr_contains(str, _T('S')));
    int res = tstr_copyn(buf, dimof(buf), str, 4);
    assert(res && tstr_eq(buf, _T("a st")));
    res = tstr_copyn(buf, 4, str, 4);
    assert(!res && tstr_eq(buf, _T("a s")));
    res = tstr_printf_s(buf, 4, _T("%s"), str);
    assert(tstr_eq(buf, _T("a s")) && res < 0);
    res = tstr_printf_s(buf, dimof(buf), _T("%s!!"), str);
    assert(tstr_startswith(buf, str) && tstr_endswith(buf, _T("!!")) && res == 10);
    tstr_copy(buf, dimof(buf), str);
    assert(tstr_eq(buf, str));

    str = tstr_dup(buf);
    assert(tstr_eq(str, buf));
    free(str);
    str = tstr_dupn(buf, 4);
    assert(tstr_eq(str, _T("a st")));
    free(str);
    str = tstr_printf(_T("%s"), buf);
    assert(tstr_eq(str, buf));
    free(str);
    str = tstr_cat(buf, buf);
    assert(tstr_len(str) == 2 * tstr_len(buf));
    free(str);
    str = tstr_cat3(_T("ab"), NULL, _T("ef"));
    assert(tstr_eq(str, _T("abef")));
    free(str);

    tstr_copy(buf, 6, _T("abc"));
    str = tstr_cat_s(buf, 6, _T("def"));
    assert(tstr_eq(buf, _T("abcde")) && !str);
    str = tstr_cat_s(buf, 6, _T("ghi"));
    assert(tstr_eq(buf, _T("abcde")) && !str);
    str = tstr_cat_s(buf, dimof(buf), _T("jkl"));
    assert(buf == str && tstr_eq(buf, _T("abcdejkl")));
    str = tstr_catn_s(buf, dimof(buf), _T("mno"), 2);
    assert(buf == str && tstr_eq(buf, _T("abcdejklmn")));

    tstr_copy(buf, dimof(buf), _T("abc\1efg\1"));
    tstr_trans_chars(buf, _T("ace"), _T("ACE"));
    assert(tstr_eq(buf, _T("AbC\1Efg\1")));
    tstr_trans_chars(buf, _T("\1"), _T("\0"));
    assert(tstr_eq(buf, _T("AbC")) && tstr_eq(buf + 4, _T("Efg")));

    TCHAR *url = tstr_url_encode(_T("key=value&key2=more data! (even \"\xFCmlauts\")'\b"));
    assert(tstr_eq(url, _T("key%3dvalue%26key2%3dmore+data!+(even+%22%fcmlauts%22)'%08")));
    free(url);

    const TCHAR *pos = _T("[Open(\"filename.pdf\",0,1,0)]");
    assert(tstr_skip(&pos, _T("[Open(\"")));
    assert(tstr_copy_skip_until(&pos, buf, dimof(buf), '"'));
    assert(tstr_eq(buf, _T("filename.pdf")));
    assert(!tstr_skip(&pos, _T("0,1")));
    assert(tstr_eq(pos, _T(",0,1,0)]")));
    *buf = _T('\0');
    assert(!tstr_copy_skip_until(&pos, buf, dimof(buf), '"'));
    assert(!*pos && !*buf);

    // the test string should only contain ASCII characters,
    // as all others might not be available in all code pages
#define TEST_STRING "aBc"
    char *strA = tstr_to_ansi(_T(TEST_STRING));
    assert(str_eq(strA, TEST_STRING));
    str = ansi_to_tstr(strA);
    free(strA);
    assert(tstr_eq(str, _T(TEST_STRING)));
    free(str);
#undef TEST_STRING
}

static void versioncheck_test()
{
    assert(ValidProgramVersion("1"));
    assert(ValidProgramVersion("1.1"));
    assert(ValidProgramVersion("1.1.1\r\n"));
    assert(ValidProgramVersion("2662"));

    assert(!ValidProgramVersion("1.1b"));
    assert(!ValidProgramVersion("1..1"));
    assert(!ValidProgramVersion("1.1\r\n.1"));

    assert(CompareVersion(_T("0.9.3.900"), _T("0.9.3")) > 0);
    assert(CompareVersion(_T("1.09.300"), _T("1.09.3")) > 0);
    assert(CompareVersion(_T("1.9.1"), _T("1.09.3")) < 0);
    assert(CompareVersion(_T("1.2.0"), _T("1.2")) == 0);
    assert(CompareVersion(_T("1.3.0"), _T("2662")) < 0);
}

static void VecTest()
{
    // TODO: write me

}

void u_DoAllTests(void)
{
    DBG_OUT("Running tests\n");
    u_RectI_Intersect();
    u_benc_all();
    MemChunkedTest();
    hexstrTest();
    ParseCommandLineTest();
    tstr_test();
    versioncheck_test();
    VecTest();
}
#endif
