/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifdef DEBUG

#include "BaseUtil.h"
#include "FileUtil.h"
#include "SimpleLog.h"
#include "WinUtil.h"

#include "DebugLog.h"

// if set to 1, dumps on to the debugger code that can be copied
// to util.py (test_gob()), to verify C and python generate
// the same encoded data
#define GEN_PYTHON_TESTS 0

static void GenPythonIntTest(int64_t val, uint8_t *d, int dLen)
{
#if GEN_PYTHON_TESTS == 1
    str::Str<char> s;
    s.AppendFmt("  assert gob_varint_encode(%I64d) == ", val);
    int n;
    for (int i = 0; i < dLen; i++) {
        n = (int)d[i];
        s.AppendFmt("chr(%d)", n);
        if (i < dLen - 1)
            s.Append(" + ");
    }
    plogf("%s", s.Get());
#endif
}

static void GenPythonUIntTest(uint64_t val, uint8_t *d, int dLen)
{
#if GEN_PYTHON_TESTS == 1
    str::Str<char> s;
    s.AppendFmt("  assert gob_uvarint_encode(%I64u) == ", val);
    int n;
    for (int i = 0; i < dLen; i++) {
        n = (int)d[i];
        s.AppendFmt("chr(%d)", n);
        if (i < dLen - 1)
            s.Append(" + ");
    }
    plogf("%s", s.Get());
#endif
}

static void BaseUtilGobEncodingTest()
{
    uint8_t buf[2048];
    int64_t intVals[] = {
        0, 1, 0x7f, 0x80, 0x81, 0xfe, 0xff, 0x100, 0x1234, 0x12345, 0x123456, 
        0x1234567, 0x12345678, 0x7fffffff, -1, -2, -255, -256, -257, -0x1234,
        -0x12345, -0x123456, -0x124567, -0x1245678
    };
    uint64_t uintVals[] = {
        0, 1, 0x7f, 0x80, 0x81, 0xfe, 0xff, 0x100, 0x1234, 0x12345, 0x123456, 
        0x1234567, 0x12345678, 0x7fffffff, 0x80000000, 0x80000001, 0xfffffffe,
        0xffffffff
    };
    int n, dLen, n2;
    uint8_t *d;
    int64_t val, expVal;
    uint64_t uval, expUval;

    d = buf; dLen = dimof(buf);
    for (int i = 0; i < dimof(intVals); i++) {
        val = intVals[i];
        n = GobVarintEncode(val, d, dLen);
        assert(n >= 1);
        GenPythonIntTest(val, d, n);
        n2 = GobVarintDecode(d, n, &expVal);
        assert(n == n2);
        assert(val == expVal);
        d += n;
        dLen -= n;
        assert(dLen > 0);
    }
    dLen = (d - buf);
    d = buf;
    for (int i = 0; i < dimof(intVals); i++) {
        expVal = intVals[i];
        n = GobVarintDecode(d, dLen, &val);
        assert(0 != n);
        assert(val == expVal);
        d += n;
        dLen -= n;
    }
    assert(0 == dLen);

    d = buf; dLen = dimof(buf);
    for (int i = 0; i < dimof(uintVals); i++) {
        uval = uintVals[i];
        n = GobUVarintEncode(uval, d, dLen);
        assert(n >= 1);
        GenPythonUIntTest(uval, d, n);
        n2 = GobUVarintDecode(d, n, &expUval);
        assert(n == n2);
        assert(uval == expUval);
        d += n;
        dLen -= n;
        assert(dLen > 0);
    }
    dLen = (d - buf);
    d = buf;
    for (int i = 0; i < dimof(uintVals); i++) {
        expUval = uintVals[i];
        n = GobUVarintDecode(d, dLen, &uval);
        assert(0 != n);
        assert(uval == expUval);
        d += n;
        dLen -= n;
    }
    assert(0 == dLen);
}
    
static void BaseUtilTest()
{
    BaseUtilGobEncodingTest();
    assert(RoundToPowerOf2(0) == 1);
    assert(RoundToPowerOf2(1) == 1);
    assert(RoundToPowerOf2(2) == 2);
    assert(RoundToPowerOf2(3) == 4);
    assert(RoundToPowerOf2(15) == 16);
    assert(RoundToPowerOf2((1 << 13) + 1) == (1 << 14));
    assert(RoundToPowerOf2(MAX_SIZE_T) == MAX_SIZE_T);

    assert(MurmurHash2(NULL, 0) == 0x342CE6C);
    assert(MurmurHash2("test", 4) != MurmurHash2("Test", 4));
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

    assert(!szD.IsEmpty() && !szI.IsEmpty());
    assert(SizeI().IsEmpty() && SizeD().IsEmpty());

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
        Swap(rx1, rx2);
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
    WCHAR *path1 = L"C:\\Program Files\\SumatraPDF\\SumatraPDF.exe";

    const WCHAR *baseName = path::GetBaseName(path1);
    assert(str::Eq(baseName, L"SumatraPDF.exe"));

    ScopedMem<WCHAR> dirName(path::GetDir(path1));
    assert(str::Eq(dirName, L"C:\\Program Files\\SumatraPDF"));
    baseName = path::GetBaseName(dirName);
    assert(str::Eq(baseName, L"SumatraPDF"));

    dirName.Set(path::GetDir(L"C:\\Program Files"));
    assert(str::Eq(dirName, L"C:\\"));
    dirName.Set(path::GetDir(dirName));
    assert(str::Eq(dirName, L"C:\\"));
    dirName.Set(path::GetDir(L"\\\\server"));
    assert(str::Eq(dirName, L"\\\\server"));
    dirName.Set(path::GetDir(L"file.exe"));
    assert(str::Eq(dirName, L"."));
    dirName.Set(path::GetDir(L"/etc"));
    assert(str::Eq(dirName, L"/"));

    path1 = L"C:\\Program Files";
    WCHAR *path2 = path::Join(L"C:\\", L"Program Files");
    assert(str::Eq(path1, path2));
    free(path2);
    path2 = path::Join(path1, L"SumatraPDF");
    assert(str::Eq(path2, L"C:\\Program Files\\SumatraPDF"));
    free(path2);
    path2 = path::Join(L"C:\\", L"\\Windows");
    assert(str::Eq(path2, L"C:\\Windows"));
    free(path2);

    assert(path::Match(L"C:\\file.pdf", L"*.pdf"));
    assert(path::Match(L"C:\\file.pdf", L"file.*"));
    assert(path::Match(L"C:\\file.pdf", L"*.xps;*.pdf"));
    assert(path::Match(L"C:\\file.pdf", L"*.xps;*.pdf;*.djvu"));
    assert(path::Match(L"C:\\file.pdf", L"f??e.p?f"));
    assert(!path::Match(L"C:\\file.pdf", L"*.xps;*.djvu"));
    assert(!path::Match(L"C:\\dir.xps\\file.pdf", L"*.xps;*.djvu"));
    assert(!path::Match(L"C:\\file.pdf", L"f??f.p?f"));
    assert(!path::Match(L"C:\\.pdf", L"?.pdf"));
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

    {
        COLORREF c = AdjustLightness(RGB(255, 0, 0), 1.0f);
        assert(c == RGB(255, 0, 0));
        c = AdjustLightness(RGB(255, 0, 0), 2.0f);
        assert(c == RGB(255, 255, 255));
        c = AdjustLightness(RGB(255, 0, 0), 0.25f);
        assert(c == RGB(64, 0, 0));
        c = AdjustLightness(RGB(226, 196, 226), 95 / 255.0f);
        assert(c == RGB(105, 52, 105));
        c = AdjustLightness(RGB(255, 255, 255), 0.5f);
        assert(c == RGB(128, 128, 128));
    }
}

static void LogTest()
{
    slog::MultiLogger log;
    log.LogAndFree(str::Dup(L"Don't leak me!"));

    slog::MemoryLogger logAll;
    log.AddLogger(&logAll);

    {
        slog::MemoryLogger ml;
        log.AddLogger(&ml);
        log.Log(L"Test1");
        ml.Log(L"ML");
        ml.LogFmt(L"%s : %d", L"filen\xE4me.pdf", 25);
        log.RemoveLogger(&ml);

        assert(str::Eq(ml.GetData(), L"Test1\r\nML\r\nfilen\xE4me.pdf : 25\r\n"));
    }

    {
        HANDLE hRead, hWrite;
        CreatePipe(&hRead, &hWrite, NULL, 0);
        slog::FileLogger fl(hWrite);
        log.AddLogger(&fl);
        log.Log(L"Test2");
        fl.Log(L"FL");
        log.LogFmt(L"%s : %d", L"filen\xE4me.pdf", 25);
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

    assert(str::Eq(logAll.GetData(), L"Test1\r\nTest2\r\nfilen\xE4me.pdf : 25\r\n"));
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
    StrTest();
    FileUtilTest();
    VecTest();
    WStrVecTest();
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
