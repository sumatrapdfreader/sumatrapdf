/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifdef DEBUG

#include "BaseUtil.h"
#include "FileUtil.h"
#include "SimpleLog.h"
#include "WinUtil.h"

#include "DebugLog.h"

static void BaseUtilTest()
{
    assert(roundToPowerOf2(0) == 1);
    assert(roundToPowerOf2(1) == 1);
    assert(roundToPowerOf2(2) == 2);
    assert(roundToPowerOf2(3) == 4);
    assert(roundToPowerOf2(15) == 16);
    assert(roundToPowerOf2((1 << 13) + 1) == (1 << 14));
    assert(roundToPowerOf2(MAX_SIZE_T) == MAX_SIZE_T);

    assert(murmur_hash2(NULL, 0) == 0x342CE6C);
    assert(murmur_hash2("test", 4) != murmur_hash2("Test", 4));
}

static void GeomTest()
{
    PointD ptD(12.4, -13.6);
    assert(ptD.x == 12.4 && ptD.y == -13.6);
    PointI ptI = ptD.Convert<int>();
    assert(ptI.x == 12 && ptI.y == -14);
    ptD = ptI.Convert<double>();
    assert(PointD(12, -14) == ptD);
    assert(PointD(12.4, -13.6) != ptD);

    SizeD szD(7.7, -3.3);
    assert(szD.dx == 7.7 && szD.dy == -3.3);
    SizeI szI = szD.Convert<int>();
    assert(szI.dx == 8 && szI.dy == -3);
    szD = szI.Convert<double>();
    assert(SizeD(8, -3) == szD);

    struct SRIData {
        int     x1s, x1e, y1s, y1e;
        int     x2s, x2e, y2s, y2e;
        bool    intersect;
        int     i_xs, i_xe, i_ys, i_ye;
        int     u_xs, u_xe, u_ys, u_ye;
    } testData[] = {
        { 0,10, 0,10,   0,10, 0,10,  true,  0,10, 0,10,  0,10, 0,10 }, /* complete intersect */
        { 0,10, 0,10,  20,30,20,30,  false, 0, 0, 0, 0,  0,30, 0,30 }, /* no intersect */
        { 0,10, 0,10,   5,15, 0,10,  true,  5,10, 0,10,  0,15, 0,10 }, /* { | } | */
        { 0,10, 0,10,   5, 7, 0,10,  true,  5, 7, 0,10,  0,10, 0,10 }, /* { | | } */
        { 0,10, 0,10,   5, 7, 5, 7,  true,  5, 7, 5, 7,  0,10, 0,10 },
        { 0,10, 0,10,   5, 15,5,15,  true,  5,10, 5,10,  0,15, 0,15 },
    };

    for (size_t i = 0; i < dimof(testData); i++) {
        struct SRIData *curr = &testData[i];

        RectI rx1(curr->x1s, curr->y1s, curr->x1e - curr->x1s, curr->y1e - curr->y1s);
        RectI rx2 = RectI::FromXY(curr->x2s, curr->y2s, curr->x2e, curr->y2e);
        RectI isect = rx1.Intersect(rx2);
        if (curr->intersect) {
            assert(!isect.IsEmpty());
            assert(isect.x == curr->i_xs && isect.y == curr->i_ys);
            assert(isect.x + isect.dx == curr->i_xe && isect.y + isect.dy == curr->i_ye);
        }
        else {
            assert(isect.IsEmpty());
        }
        RectI urect = rx1.Union(rx2);
        assert(urect.x == curr->u_xs && urect.y == curr->u_ys);
        assert(urect.x + urect.dx == curr->u_xe && urect.y + urect.dy == curr->u_ye);

        /* if we swap rectangles, the results should be the same */
        swap(rx1, rx2);
        isect = rx1.Intersect(rx2);
        if (curr->intersect) {
            assert(!isect.IsEmpty());
            assert(isect.x == curr->i_xs && isect.y == curr->i_ys);
            assert(isect.x + isect.dx == curr->i_xe && isect.y + isect.dy == curr->i_ye);
        }
        else {
            assert(isect.IsEmpty());
        }
        urect = rx1.Union(rx2);
        assert(RectI::FromXY(curr->u_xs, curr->u_ys, curr->u_xe, curr->u_ye) == urect);

        assert(!rx1.Contains(PointI(-2, -2)));
        assert(rx1.Contains(rx1.TL()));
        assert(!rx1.Contains(PointI(rx1.x, INT_MAX)));
        assert(!rx1.Contains(PointI(INT_MIN, rx1.y)));
    }
}


static void FileUtilTest()
{
    TCHAR *path1 = _T("C:\\Program Files\\SumatraPDF\\SumatraPDF.exe");

    const TCHAR *baseName = path::GetBaseName(path1);
    assert(str::Eq(baseName, _T("SumatraPDF.exe")));

    ScopedMem<TCHAR> dirName(path::GetDir(path1));
    assert(str::Eq(dirName, _T("C:\\Program Files\\SumatraPDF")));
    baseName = path::GetBaseName(dirName);
    assert(str::Eq(baseName, _T("SumatraPDF")));

    dirName.Set(path::GetDir(_T("C:\\Program Files")));
    assert(str::Eq(dirName, _T("C:\\")));
    dirName.Set(path::GetDir(dirName));
    assert(str::Eq(dirName, _T("C:\\")));
    dirName.Set(path::GetDir(_T("\\\\server")));
    assert(str::Eq(dirName, _T("\\\\server")));
    dirName.Set(path::GetDir(_T("file.exe")));
    assert(str::Eq(dirName, _T(".")));
    dirName.Set(path::GetDir(_T("/etc")));
    assert(str::Eq(dirName, _T("/")));

    path1 = _T("C:\\Program Files");
    TCHAR *path2 = path::Join(_T("C:\\"), _T("Program Files"));
    assert(str::Eq(path1, path2));
    free(path2);
    path2 = path::Join(path1, _T("SumatraPDF"));
    assert(str::Eq(path2, _T("C:\\Program Files\\SumatraPDF")));
    free(path2);
    path2 = path::Join(_T("C:\\"), _T("\\Windows"));
    assert(str::Eq(path2, _T("C:\\Windows")));
    free(path2);

    assert(path::Match(_T("C:\\file.pdf"), _T("*.pdf")));
    assert(path::Match(_T("C:\\file.pdf"), _T("file.*")));
    assert(path::Match(_T("C:\\file.pdf"), _T("*.xps;*.pdf")));
    assert(path::Match(_T("C:\\file.pdf"), _T("*.xps;*.pdf;*.djvu")));
    assert(path::Match(_T("C:\\file.pdf"), _T("f??e.p?f")));
    assert(!path::Match(_T("C:\\file.pdf"), _T("*.xps;*.djvu")));
    assert(!path::Match(_T("C:\\dir.xps\\file.pdf"), _T("*.xps;*.djvu")));
    assert(!path::Match(_T("C:\\file.pdf"), _T("f??f.p?f")));
    assert(!path::Match(_T("C:\\.pdf"), _T("?.pdf")));
}

static void WinUtilTest()
{
    ScopedCom comScope;

    {
        char *string = "abcde";
        size_t stringSize = 5, len;
        ScopedComPtr<IStream> stream(CreateStreamFromData(string, stringSize));
        assert(stream);
        char *data = (char *)GetDataFromStream(stream, &len);
        assert(data && stringSize == len && str::Eq(data, string));
        free(data);
    }

    {
        WCHAR *string = L"abcde";
        size_t stringSize = 10, len;
        ScopedComPtr<IStream> stream(CreateStreamFromData(string, stringSize));
        assert(stream);
        WCHAR *data = (WCHAR *)GetDataFromStream(stream, &len);
        assert(data && stringSize == len && str::Eq(data, string));
        free(data);
    }

    {
        RectI oneScreen = GetFullscreenRect(NULL);
        RectI allScreens = GetVirtualScreenRect();
        assert(allScreens.Intersect(oneScreen) == oneScreen);
    }
}

static void LogTest()
{
    slog::MultiLogger log;
    log.LogAndFree(str::Dup(_T("Don't leak me!")));

    slog::MemoryLogger logAll;
    log.AddLogger(&logAll);

    {
        slog::MemoryLogger ml;
        log.AddLogger(&ml);
        log.Log(_T("Test1"));
        ml.Log(_T("ML"));
        ml.LogFmt(_T("%s : %d"), _T("filen\xE4me.pdf"), 25);
        log.RemoveLogger(&ml);

        assert(str::Eq(ml.GetData(), _T("Test1\r\nML\r\nfilen\xE4me.pdf : 25\r\n")));
    }

    {
        HANDLE hRead, hWrite;
        CreatePipe(&hRead, &hWrite, NULL, 0);
        slog::FileLogger fl(hWrite);
        log.AddLogger(&fl);
        log.Log(_T("Test2"));
        fl.Log(_T("FL"));
        log.LogFmt(_T("%s : %d"), _T("filen\xE4me.pdf"), 25);
        log.RemoveLogger(&fl);

        char pipeData[32];
        char *expected = "Test2\r\nFL\r\nfilen\xC3\xA4me.pdf : 25\r\n";
        DWORD len;
        BOOL ok = ReadFile(hRead, pipeData, sizeof(pipeData), &len, NULL);
        assert(ok && len == str::Len(expected));
        pipeData[len] = '\0';
        assert(str::Eq(pipeData, expected));
        CloseHandle(hRead);
    }

    assert(str::Eq(logAll.GetData(), _T("Test1\r\nTest2\r\nfilen\xE4me.pdf : 25\r\n")));
    log.RemoveLogger(&logAll);

    // don't leak the logger, don't crash on logging NULL
    log.AddLogger(new slog::DebugLogger());
    log.Log(NULL);
}

#include "BencUtil_ut.cpp"
#include "JsonParser_ut.cpp"
#include "Sigslot_ut.cpp"
#include "StrUtil_ut.cpp"
#include "Vec_ut.cpp"
#include "ByteOrderDecoder_ut.cpp"
#include "StrFormat_ut.cpp"
#include "Dict_ut.cpp"

void BaseUtils_UnitTests()
{
    plogf("Running BaseUtils unit tests");
    BaseUtilTest();
    ByteOrderTests();
    GeomTest();
    TStrTest();
    FileUtilTest();
    VecTest();
    StrVecTest();
    StrListTest();
    WinUtilTest();
    LogTest();
    BencTest();
    SigSlotTest();
    JsonTest();
    StrFormatTest();
    DictTest();
}

#endif
