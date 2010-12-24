#include "SumatraPDF.h"
#include "str_util.h"
#include "MemSegment.h"
#include "geom_util.h"
#include "ParseCommandLine.h"

#ifdef DEBUG
extern void StrToFileTime(char *s, FILETIME* ft);
extern char *FileTimeToStr(FILETIME* ft);
extern DWORD FileTimeDiffInSecs(FILETIME *ft1, FILETIME *ft2);

static void hexstrTest()
{
    unsigned char buf[6] = {1, 2, 33, 255, 0, 18};
    unsigned char buf2[6] = {0};
    char *s = mem_to_hexstr(buf, sizeof(buf));
    BOOL ok = hexstr_to_mem(s, buf2, sizeof(buf2));
    assert(ok);
    for (int i=0; i<sizeof(buf); i++) {
        assert(buf[i] == buf2[i]);
    }
    free(s);
    FILETIME ft1, ft2;
    GetSystemTimeAsFileTime(&ft1);
    s = FileTimeToStr(&ft1);
    StrToFileTime(s, &ft2);
    DWORD diff = FileTimeDiffInSecs(&ft1, &ft2);
    assert(0 == diff);
    assert(ft1.dwLowDateTime == ft2.dwLowDateTime);
    assert(ft1.dwHighDateTime == ft2.dwHighDateTime);
    free(s);
}

static void MemSegmentTest()
{
    MemSegment *ms;
    DWORD size;
    char *data;

    char buf[2] = {'a', '\0'};
    ms = new MemSegment();
    for (int i=0; i<7; i++) {
        ms->add(buf, 1);
        buf[0] = buf[0] + 1;
    }
    data = (char*)ms->getData(&size);
    delete ms;
    assert(str_eq("abcdefg", data));
    assert(7 == size);
    free(data);

    ms = new MemSegment();
    ms->add("a", 1);
    data = (char*)ms->getData(&size);
    ms->clearFree();
    delete ms;
    assert(str_eq("a", data));
    assert(1 == size);
    free(data);
}

static void ParseCommandLineTest()
{
    {
        CommandLineInfo i;
        ParseCommandLine(i, _T("SumatraPDF.exe -presentation -bgcolor 0xaa0c13 foo.pdf -invert-colors bar.pdf"));
        assert(true == i.enterPresentation);
        assert(TRUE == i.invertColors);
        assert(1248426 == i.bgColor);
        assert(2 == i.fileNames.size());
        assert(-1 != i.fileNames.find(_T("foo.pdf")));
        assert(-1 != i.fileNames.find(_T("bar.pdf")));
    }

    {
        CommandLineInfo i;
        ParseCommandLine(i, _T("SumatraPDF.exe -bg-color 0xaa0c13 -invertcolors rosanna.pdf"));
        assert(TRUE == i.invertColors);
        assert(1248426 == i.bgColor);
        assert(1 == i.fileNames.size());
        assert(-1 != i.fileNames.find(_T("rosanna.pdf")));
    }
}

void u_DoAllTests(void)
{
    DBG_OUT("Running tests\n");
    u_RectI_Intersect();
    MemSegmentTest();
    hexstrTest();
    ParseCommandLineTest();
}
#endif

