/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

#ifdef DEBUG

#include "BaseUtil.h"
#include "BencUtil.h"
#include "FileUtil.h"
#include "GeomUtil.h"
#include "StrUtil.h"
#include "WinUtil.h"
#include "Vec.h"
#include "SimpleLog.h"
#include <time.h>

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

        assert(!rx1.Inside(PointI(-2, -2)));
        assert(rx1.Inside(rx1.TL()));
        assert(!rx1.Inside(PointI(rx1.x, INT_MAX)));
        assert(!rx1.Inside(PointI(INT_MIN, rx1.y)));
    }
}

static void TStrTest()
{
    TCHAR buf[32];
    TCHAR *str = _T("a string");
    assert(str::Len(str) == 8);
    assert(str::Eq(str, _T("a string")) && str::Eq(str, str));
    assert(!str::Eq(str, NULL) && !str::Eq(str, _T("A String")));
    assert(str::EqI(str, _T("A String")) && str::EqI(str, str));
    assert(!str::EqI(str, NULL) && str::EqI((char*)NULL, (char*)NULL));
    assert(str::StartsWith(str, _T("a s")) && str::StartsWithI(str, _T("A Str")));
    assert(!str::StartsWith(str, _T("Astr")));
    assert(str::EndsWith(str, _T("ing")) && str::EndsWithI(str, _T("ING")));
    assert(!str::EndsWith(str, _T("ung")));
    assert(str::IsEmpty((char*)NULL) && str::IsEmpty((WCHAR*)NULL)&& str::IsEmpty(_T("")) && !str::IsEmpty(str));
    assert(str::FindChar(str, _T('s')) && !str::FindChar(str, _T('S')));
    size_t len = str::BufSet(buf, dimof(buf), str);
    assert(len == str::Len(buf) && str::Eq(buf, str));
    len = str::BufSet(buf, 6, str);
    assert(len == 5 && str::Eq(buf, _T("a str")));

    str = str::Dup(buf);
    assert(str::Eq(str, buf));
    free(str);
    str = str::DupN(buf, 4);
    assert(str::Eq(str, _T("a st")));
    free(str);
    str = str::Format(_T("%s"), buf);
    assert(str::Eq(str, buf));
    free(str);
    str = str::Join(buf, buf);
    assert(str::Len(str) == 2 * str::Len(buf));
    free(str);
    str = str::Join(NULL, _T("ab"));
    assert(str::Eq(str, _T("ab")));
    free(str);

    str::BufSet(buf, dimof(buf), _T("abc\1efg\1"));
    size_t count = str::TransChars(buf, _T("ace"), _T("ACE"));
    assert(str::Eq(buf, _T("AbC\1Efg\1")) && count == 3);
    count = str::TransChars(buf, _T("\1"), _T("\0"));
    assert(str::Eq(buf, _T("AbC")) && str::Eq(buf + 4, _T("Efg")) && count == 2);
    count = str::TransChars(buf, _T(""), _T("X"));
    assert(str::Eq(buf, _T("AbC")) && count == 0);

    str::BufSet(buf, dimof(buf), _T("blogarapato"));
    count = str::RemoveChars(buf, _T("bo"));
    assert(3 == count);
    assert(str::Eq(buf, _T("lgarapat")));

    str::BufSet(buf, dimof(buf), _T("one\r\ntwo\t\v\f\tthree"));
    count = str::NormalizeWS(buf);
    assert(4 == count);
    assert(str::Eq(buf, _T("one two three")));

    str::BufSet(buf, dimof(buf), _T(" one    two three "));
    count = str::NormalizeWS(buf);
    assert(5 == count);
    assert(str::Eq(buf, _T("one two three")));

    count = str::NormalizeWS(buf);
    assert(0 == count);
    assert(str::Eq(buf, _T("one two three")));

    str = _T("[Open(\"filename.pdf\",0,1,0)]");
    {
        UINT u1 = 0;
        TCHAR *str1 = NULL;
        const TCHAR *end = str::Parse(str, _T("[Open(\"%s\",%? 0,%u,0)]"), &str1, &u1);
        assert(end && !*end);
        assert(u1 == 1 && str::Eq(str1, _T("filename.pdf")));
        free(str1);
    }

    {
        UINT u1 = 0;
        ScopedMem<TCHAR> str1;
        const TCHAR *end = str::Parse(str, _T("[Open(\"%S\",0%?,%u,0)]"), &str1, &u1);
        assert(end && !*end);
        assert(u1 == 1 && str::Eq(str1, _T("filename.pdf")));

        assert(str::Parse(_T("0xABCD"), _T("%x"), &u1));
        assert(u1 == 0xABCD);
        assert(str::Parse(_T("ABCD"), _T("%2x%S"), &u1, &str1));
        assert(u1 == 0xAB && str::Eq(str1, _T("CD")));
    }

    {
        int i1, i2;
        const TCHAR *end = str::Parse(_T("1, 2+3"), _T("%d,%d"), &i1, &i2);
        assert(end && str::Eq(end, _T("+3")));
        assert(i1 == 1 && i2 == 2);
        end = str::Parse(end, _T("+3"));
        assert(end && !*end);

        assert(str::Parse(_T(" -2"), _T("%d"), &i1));
        assert(i1 == -2);
        assert(str::Parse(_T(" 2"), _T(" %u"), &i1));
        assert(i1 == 2);
        assert(str::Parse(_T("123-456"), _T("%3d%3d6"), &i1, &i2));
        assert(i1 == 123 && i2 == -45);
        assert(!str::Parse(_T("123"), _T("%4d"), &i1));
        assert(str::Parse(_T("654"), _T("%3d"), &i1));
        assert(i1 == 654);
    }

    assert(str::Parse(_T("abc"), _T("abc%$")));
    assert(str::Parse(_T("abc"), _T("a%?bc%?d%$")));
    assert(!str::Parse(_T("abc"), _T("ab%$")));

    {
        float f1, f2;
        const TCHAR *end = str::Parse(_T("%1.23y -2e-3z"), _T("%%%fy%fz%$"), &f1, &f2);
        assert(end && !*end);
        assert(f1 == 1.23f && f2 == -2e-3f);
    }

    {
        TCHAR *str1 = NULL;
        TCHAR c1;
        assert(!str::Parse(_T("no exclamation mark?"), _T("%s!"), &str1));
        assert(!str1);
        assert(str::Parse(_T("xyz"), _T("x%cz"), &c1));
        assert(c1 == 'y');
        assert(!str::Parse(_T("leaks memory!?"), _T("%s!%$"), &str1));
        free(str1);
    }

    // the test string should only contain ASCII characters,
    // as all others might not be available in all code pages
#define TEST_STRING "aBc"
    char *strA = str::conv::ToAnsi(_T(TEST_STRING));
    assert(str::Eq(strA, TEST_STRING));
    str = str::conv::FromAnsi(strA);
    free(strA);
    assert(str::Eq(str, _T(TEST_STRING)));
    free(str);
#undef TEST_STRING

    assert(ChrIsDigit('0') && ChrIsDigit(_T('5')) && ChrIsDigit(L'9'));
    assert(iswdigit(L'\xB2') && !ChrIsDigit(L'\xB2'));

    assert(str::CmpNatural(_T(".hg"), _T("2.pdf")) < 0);
    assert(str::CmpNatural(_T("100.pdf"), _T("2.pdf")) > 0);
    assert(str::CmpNatural(_T("2.pdf"), _T("zzz")) < 0);
    assert(str::CmpNatural(_T("abc"), _T(".svn")) > 0);
    assert(str::CmpNatural(_T("ab0200"), _T("AB333")) < 0);
    assert(str::CmpNatural(_T("a b"), _T("a  c")) < 0);

    struct {
        size_t number;
        const TCHAR *result;
    } formatNumData[] = {
        { 1,        _T("1") },
        { 12,       _T("12") },
        { 123,      _T("123") },
        { 1234,     _T("1'234") },
        { 12345,    _T("12'345") },
        { 123456,   _T("123'456") },
        { 1234567,  _T("1'234'567") },
        { 12345678, _T("12'345'678") },
    };

    for (int i = 0; i < dimof(formatNumData); i++) {
        ScopedMem<TCHAR> tmp(str::FormatNumWithThousandSep(formatNumData[i].number, _T("'")));
        assert(str::Eq(tmp, formatNumData[i].result));
    }

    struct {
        double number;
        const TCHAR *result;
    } formatFloatData[] = {
        { 1,        _T("1.0") },
        { 1.2,      _T("1.2") },
        { 1.23,     _T("1.23") },
        { 1.234,    _T("1.23") },
        { 12.345,   _T("12.35") },
        { 123.456,  _T("123.46") },
    };

    for (int i = 0; i < dimof(formatFloatData); i++) {
        ScopedMem<TCHAR> tmp(str::FormatFloatWithThousandSep(formatFloatData[i].number));
        assert(str::Eq(tmp, formatFloatData[i].result));
    }

    {
        char str[] = "aAbBcC... 1-9";
        str::ToLower(str);
        assert(str::Eq(str, "aabbcc... 1-9"));

        WCHAR wstr[] = L"aAbBcC... 1-9";
        str::ToLower(wstr);
        assert(str::Eq(wstr, L"aabbcc... 1-9"));
    }

    struct {
        int number;
        const TCHAR *result;
    } formatRomanData[] = {
        { 1,    _T("I") },
        { 3,    _T("III") },
        { 6,    _T("VI") },
        { 14,   _T("XIV") },
        { 49,   _T("XLIX") },
        { 176,  _T("CLXXVI") },
        { 499,  _T("CDXCIX") },
        { 1666, _T("MDCLXVI") },
        { 2011, _T("MMXI") },
        { 12345,_T("MMMMMMMMMMMMCCCXLV") },
        { 0,    NULL },
        { -133, NULL },
    };

    for (int i = 0; i < dimof(formatRomanData); i++) {
        ScopedMem<TCHAR> tmp(str::FormatRomanNumeral(formatRomanData[i].number));
        assert(str::Eq(tmp, formatRomanData[i].result));
    }

    {
        size_t trimmed;
        TCHAR *s = NULL;
        s = str::Dup(_T(""));
        trimmed = str::TrimWS(s);
        assert(trimmed == 0);
        assert(str::Eq(s, _T("")));
        trimmed = str::TrimWS(s, str::TrimRight);
        assert(trimmed == 0);
        assert(str::Eq(s, _T("")));
        trimmed = str::TrimWS(s, str::TrimLeft);
        assert(trimmed == 0);
        assert(str::Eq(s, _T("")));

        free(s); s = str::Dup(_T("  \n\t  "));
        trimmed = str::TrimWS(s);
        assert(trimmed == 6);
        assert(str::Eq(s, _T("")));

        free(s); s = str::Dup(_T("  \n\t  "));
        trimmed = str::TrimWS(s, str::TrimRight);
        assert(trimmed == 6);
        assert(str::Eq(s, _T("")));

        free(s); s = str::Dup(_T("  \n\t  "));
        trimmed = str::TrimWS(s, str::TrimLeft);
        assert(trimmed == 6);
        assert(str::Eq(s, _T("")));

        free(s); s = str::Dup(_T("  lola"));
        trimmed = str::TrimWS(s);
        assert(trimmed == 2);
        assert(str::Eq(s, _T("lola")));

        free(s); s = str::Dup(_T("  lola"));
        trimmed = str::TrimWS(s, str::TrimLeft);
        assert(trimmed == 2);
        assert(str::Eq(s, _T("lola")));

        free(s); s = str::Dup(_T("  lola"));
        trimmed = str::TrimWS(s, str::TrimRight);
        assert(trimmed == 0);
        assert(str::Eq(s, _T("  lola")));

        free(s); s = str::Dup(_T("lola\r\t"));
        trimmed = str::TrimWS(s);
        assert(trimmed == 2);
        assert(str::Eq(s, _T("lola")));

        free(s); s = str::Dup(_T("lola\r\t"));
        trimmed = str::TrimWS(s, str::TrimRight);
        assert(trimmed == 2);
        assert(str::Eq(s, _T("lola")));

        free(s); s = str::Dup(_T("lola\r\t"));
        trimmed = str::TrimWS(s, str::TrimLeft);
        assert(trimmed == 0);
        assert(str::Eq(s, _T("lola\r\t")));

        free(s);
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

static void StrVecTest()
{
    StrVec v;
    v.Append(str::Dup(_T("foo")));
    v.Append(str::Dup(_T("bar")));
    TCHAR *s = v.Join();
    assert(v.Count() == 2);
    assert(str::Eq(_T("foobar"), s));
    free(s);

    s = v.Join(_T(";"));
    assert(v.Count() == 2);
    assert(str::Eq(_T("foo;bar"), s));
    free(s);

    v.Append(str::Dup(_T("glee")));
    s = v.Join(_T("_ _"));
    assert(v.Count() == 3);
    assert(str::Eq(_T("foo_ _bar_ _glee"), s));
    free(s);

    v.Sort();
    s = v.Join();
    assert(str::Eq(_T("barfooglee"), s));
    free(s);

    {
        StrVec v2(v);
        assert(str::Eq(v2.At(1), _T("foo")));
        v2.Append(str::Dup(_T("nobar")));
        assert(str::Eq(v2.At(3), _T("nobar")));
        v2 = v;
        assert(v2.Count() == 3 && v2.At(0) != v.At(0));
        assert(str::Eq(v2.At(1), _T("foo")));
    }

    {
        StrVec v2;
        size_t count = v2.Split(_T("a,b,,c,"), _T(","));
        assert(count == 5 && v2.Find(_T("c")) == 3);
        assert(v2.Find(_T("")) == 2 && v2.Find(_T(""), 3) == 4 && v2.Find(_T(""), 5) == -1);
        ScopedMem<TCHAR> joined(v2.Join(_T(";")));
        assert(str::Eq(joined, _T("a;b;;c;")));
    }

    {
        StrVec v2;
        size_t count = v2.Split(_T("a,b,,c,"), _T(","), true);
        assert(count == 3 && v2.Find(_T("c")) == 2);
        ScopedMem<TCHAR> joined(v2.Join(_T(";")));
        assert(str::Eq(joined, _T("a;b;c")));
    }
}

static size_t VecTestAppendFmt()
{
    str::Str<char> v(256);
    int64_t val = 1;
    for (int i = 0; i < 10000; i++) {
        v.AppendFmt("i%" PRId64 "e", val);
        val = (val * 3) / 2; // somewhat exponential growth
        val += 15;
    }
    size_t l = v.Count();
    return l;
}

static void VecTest()
{
    Vec<int> ints;
    assert(ints.Count() == 0);
    ints.Append(1);
    ints.Push(2);
    ints.InsertAt(0, -1);
    assert(ints.Count() == 3);
    assert(ints.At(0) == -1 && ints.At(1) == 1 && ints.At(2) == 2);
    assert(ints.At(0) == -1 && ints.Last() == 2);
    int last = ints.Pop();
    assert(last == 2);
    assert(ints.Count() == 2);
    ints.Push(3);
    ints.RemoveAt(0);
    assert(ints.Count() == 2);
    assert(ints.At(0) == 1 && ints.At(1) == 3);
    ints.Reset();
    assert(ints.Count() == 0);

    for (int i = 0; i < 1000; i++)
        ints.Push(i);
    assert(ints.Count() == 1000 && ints.At(500) == 500);
    ints.Remove(500);
    assert(ints.Count() == 999 && ints.At(500) == 501);

    {
        Vec<int> ints2(ints);
        assert(ints2.Count() == 999);
        assert(ints.LendData() != ints2.LendData());
        ints.Remove(600);
        assert(ints.Count() < ints2.Count());
        ints2 = ints;
        assert(ints2.Count() == 998);
    }

    {
        char buf[2] = {'a', '\0'};
        str::Str<char> v(0);
        for (int i = 0; i < 7; i++) {
            v.Append(buf, 1);
            buf[0] = buf[0] + 1;
        }
        char *s = v.LendData();
        assert(str::Eq("abcdefg", s));
        assert(7 == v.Count());
        v.Set("helo");
        assert(4 == v.Count());
        assert(str::Eq("helo", v.LendData()));
    }

    {
        str::Str<char> v(128);
        v.Append("boo", 3);
        assert(str::Eq("boo", v.LendData()));
        assert(v.Count() == 3);
        v.Append("fop");
        assert(str::Eq("boofop", v.LendData()));
        assert(v.Count() == 6);
        v.RemoveAt(2, 3);
        assert(v.Count() == 3);
        assert(str::Eq("bop", v.LendData()));
        v.Append('a');
        assert(v.Count() == 4);
        assert(str::Eq("bopa", v.LendData()));
        char *s = v.StealData();
        assert(str::Eq("bopa", s));
        free(s);
        assert(v.Count() == 0);
    }

    {
        str::Str<char> v(0);
        for (int i = 0; i < 32; i++) {
            assert(v.Count() == i * 6);
            v.Append("lambd", 5);
            if (i % 2 == 0)
                v.Append('a');
            else
                v.Push('a');
        }

        for (int i=1; i<=16; i++) {
            v.RemoveAt((16 - i) * 6, 6);
            assert(v.Count() == (32 - i) * 6);
        }

        v.RemoveAt(0, 6 * 15);
        assert(v.Count() == 6);
        char *s = v.LendData();
        assert(str::Eq(s, "lambda"));
        s = v.StealData();
        assert(str::Eq(s, "lambda"));
        free(s);
        assert(v.Count() == 0);
    }

    VecTestAppendFmt();

    {
        Vec<PointI *> v;
        srand((unsigned int)time(NULL));
        for (int i = 0; i < 128; i++) {
            v.Append(new PointI(i, i));
            size_t pos = rand() % v.Count();
            v.InsertAt(pos, new PointI(i, i));
        }
        while (v.Count() > 64) {
            size_t pos = rand() % v.Count();
            PointI *f = v.At(pos);
            v.Remove(f);
            delete f;
        }
        DeleteVecMembers(v);
    }

    {
        Vec<int> v;
        v.Append(2);
        for (int i = 0; i < 500; i++)
            v.Append(4);
        v.At(250) = 5;
        v.Reverse();
        assert(v.Count() == 501 && v.At(0) == 4 && v.At(249) == v.At(251) && v.At(250) == 5 && v.At(500) == 2);
        v.Remove(4);
        v.Reverse();
        assert(v.Count() == 500 && v.At(0) == 2 && v.At(249) == v.At(251) && v.At(250) == 5 && v.At(499) == 4);
    }
}

static void WinUtilTest()
{
    ScopedCom comScope;

    {
        char *string = "abcde", *data;
        size_t stringSize = 5, len;
        ScopedComPtr<IStream> stream(CreateStreamFromData(string, stringSize));
        assert(stream);
        HRESULT res = GetDataFromStream(stream, (void **)&data, &len);
        assert(SUCCEEDED(res));
        assert(stringSize == len && str::Eq(data, string));
        free(data);
    }

    {
        WCHAR *string = L"abcde", *data;
        size_t stringSize = 10, len;
        ScopedComPtr<IStream> stream(CreateStreamFromData(string, stringSize));
        assert(stream);
        HRESULT res = GetDataFromStream(stream, (void **)&data, &len);
        assert(SUCCEEDED(res));
        assert(stringSize == len && str::Eq(data, string));
        free(data);
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

static void BencTestSerialization(BencObj *obj, const char *dataOrig)
{
    ScopedMem<char> data(obj->Encode());
    assert(data);
    assert(str::Eq(data, dataOrig));
}

static void BencTestRoundtrip(BencObj *obj)
{
    ScopedMem<char> encoded(obj->Encode());
    assert(encoded);
    size_t len;
    BencObj *obj2 = BencObj::Decode(encoded, &len);
    assert(obj2 && len == str::Len(encoded));
    ScopedMem<char> roundtrip(obj2->Encode());
    assert(str::Eq(encoded, roundtrip));
    delete obj2;
}

static void BencTestParseInt()
{
    struct {
        const char *    benc;
        bool            valid;
        int64_t         value;
    } testData[] = {
        { NULL, false },
        { "", false },
        { "a", false },
        { "0", false },
        { "i", false },
        { "ie", false },
        { "i0", false },
        { "i1", false },
        { "i23", false },
        { "i-", false },
        { "i-e", false },
        { "i-0e", false },
        { "i23f", false },
        { "i2-3e", false },
        { "i23-e", false },
        { "i041e", false },
        { "i9223372036854775808e", false },
        { "i-9223372036854775809e", false },

        { "i0e", true, 0 },
        { "i1e", true, 1 },
        { "i9823e", true, 9823 },
        { "i-1e", true, -1 },
        { "i-53e", true, -53 },
        { "i123e", true, 123 },
        { "i2147483647e", true, INT_MAX },
        { "i2147483648e", true, (int64_t)INT_MAX + 1 },
        { "i-2147483648e", true, INT_MIN },
        { "i-2147483649e", true, (int64_t)INT_MIN - 1 },
        { "i9223372036854775807e", true, _I64_MAX },
        { "i-9223372036854775808e", true, _I64_MIN },
    };

    for (int i = 0; i < dimof(testData); i++) {
        BencObj *obj = BencObj::Decode(testData[i].benc);
        if (testData[i].valid) {
            assert(obj);
            assert(obj->Type() == BT_INT);
            assert(static_cast<BencInt *>(obj)->Value() == testData[i].value);
            BencTestSerialization(obj, testData[i].benc);
            delete obj;
        } else {
            assert(!obj);
        }
    }
}

static void BencTestParseString()
{
    struct {
        const char *    benc;
        TCHAR *         value;
    } testData[] = {
        { NULL, NULL },
        { "", NULL },
        { "0", NULL },
        { "1234", NULL },
        { "a", NULL },
        { ":", NULL },
        { ":z", NULL },
        { "1:ab", NULL },
        { "3:ab", NULL },
        { "-2:ab", NULL },
        { "2e:ab", NULL },

        { "0:", _T("") },
        { "1:a", _T("a") },
        { "2::a", _T(":a") },
        { "4:spam", _T("spam") },
        { "4:i23e", _T("i23e") },
#ifdef UNICODE
        { "5:\xC3\xA4\xE2\x82\xAC", L"\u00E4\u20AC" },
#endif
    };

    for (int i = 0; i < dimof(testData); i++) {
        BencObj *obj = BencObj::Decode(testData[i].benc);
        if (testData[i].value) {
            assert(obj);
            assert(obj->Type() == BT_STRING);
            ScopedMem<TCHAR> value(static_cast<BencString *>(obj)->Value());
            assert(str::Eq(value, testData[i].value));
            BencTestSerialization(obj, testData[i].benc);
            delete obj;
        } else {
            assert(!obj);
        }
    }
}

static void BencTestParseRawStrings()
{
    BencArray array;
    array.AddRaw("a\x82");
    array.AddRaw("a\x82", 1);
    BencString *raw = array.GetString(0);
    assert(raw && str::Eq(raw->RawValue(), "a\x82"));
    BencTestSerialization(raw, "2:a\x82");
    raw = array.GetString(1);
    assert(raw && str::Eq(raw->RawValue(), "a"));
    BencTestSerialization(raw, "1:a");

    BencDict dict;
    dict.AddRaw("1", "a\x82");
    dict.AddRaw("2", "a\x82", 1);
    raw = dict.GetString("1");
    assert(raw && str::Eq(raw->RawValue(), "a\x82"));
    BencTestSerialization(raw, "2:a\x82");
    raw = dict.GetString("2");
    assert(raw && str::Eq(raw->RawValue(), "a"));
    BencTestSerialization(raw, "1:a");
}

static void BencTestParseArray(const char *benc, size_t expectedLen)
{
    BencObj *obj = BencObj::Decode(benc);
    assert(obj);
    assert(obj->Type() == BT_ARRAY);
    assert(static_cast<BencArray *>(obj)->Length() == expectedLen);
    BencTestSerialization(obj, benc);
    delete obj;
}

static void BencTestParseArrays()
{   
    BencObj *obj;

    obj = BencObj::Decode("l");
    assert(!obj);
    obj = BencObj::Decode("l123");
    assert(!obj);
    obj = BencObj::Decode("li12e");
    assert(!obj);
    obj = BencObj::Decode("l2:ie");
    assert(!obj);

    BencTestParseArray("le", 0);
    BencTestParseArray("li35ee", 1);
    BencTestParseArray("llleee", 1);
    BencTestParseArray("li35ei-23e2:abe", 3);
    BencTestParseArray("li42e2:teldeedee", 4);
}

static void BencTestParseDict(const char *benc, size_t expectedLen)
{
    BencObj *obj = BencObj::Decode(benc);
    assert(obj);
    assert(obj->Type() == BT_DICT);
    assert(static_cast<BencDict *>(obj)->Length() == expectedLen);
    BencTestSerialization(obj, benc);
    delete obj;
}

static void BencTestParseDicts()
{   
    BencObj *obj;

    obj = BencObj::Decode("d");
    assert(!obj);
    obj = BencObj::Decode("d123");
    assert(!obj);
    obj = BencObj::Decode("di12e");
    assert(!obj);
    obj = BencObj::Decode("di12e2:ale");
    assert(!obj);

    BencTestParseDict("de", 0);
    BencTestParseDict("d2:hai35ee", 1);
    BencTestParseDict("d4:borg1:a3:rum3:leee", 2);
    BencTestParseDict("d1:Zi-23e2:able3:keyi35ee", 3);
}

#define ITERATION_COUNT 128

static void BencTestArrayAppend()
{
    BencArray *array = new BencArray();
    for (size_t i = 1; i <= ITERATION_COUNT; i++) {
        array->Add(i);
        assert(array->Length() == i);
    }
    array->Add(new BencDict());
    for (size_t i = 1; i <= ITERATION_COUNT; i++) {
        BencInt *obj = array->GetInt(i - 1);
        assert(obj && obj->Type() == BT_INT);
        assert(obj->Value() == i);
        assert(!array->GetString(i - 1));
        assert(!array->GetArray(i - 1));
        assert(!array->GetDict(i - 1));
    }
    assert(!array->GetInt(ITERATION_COUNT));
    assert(array->GetDict(ITERATION_COUNT));
    BencTestRoundtrip(array);
    delete array;
}

static void BencTestDictAppend()
{
    /* test insertion in ascending order */
    BencDict *dict = new BencDict();
    for (size_t i = 1; i <= ITERATION_COUNT; i++) {
        ScopedMem<char> key(str::Format("%04u", i));
        assert(str::Len(key) == 4);
        dict->Add(key, i);
        assert(dict->Length() == i);
        assert(dict->GetInt(key));
        assert(!dict->GetString(key));
        assert(!dict->GetArray(key));
        assert(!dict->GetDict(key));
    }
    BencInt *intObj = dict->GetInt("0123");
    assert(intObj && intObj->Value() == 123);
    BencTestRoundtrip(dict);
    delete dict;

    /* test insertion in descending order */
    dict = new BencDict();
    for (size_t i = ITERATION_COUNT; i > 0; i--) {
        ScopedMem<char> key(str::Format("%04u", i));
        assert(str::Len(key) == 4);
        BencObj *obj = new BencInt(i);
        dict->Add(key, obj);
        assert(dict->Length() == ITERATION_COUNT + 1 - i);
        assert(dict->GetInt(key));
    }
    intObj = dict->GetInt("0123");
    assert(intObj && intObj->Value() == 123);
    BencTestRoundtrip(dict);
    delete dict;

    dict = new BencDict();
    dict->Add("ab", 1);
    dict->Add("KL", 2);
    dict->Add("gh", 3);
    dict->Add("YZ", 4);
    dict->Add("ab", 5);
    BencTestSerialization(dict, "d2:KLi2e2:YZi4e2:abi5e2:ghi3ee");
    delete dict;
}

static void GenRandStr(char *buf, int bufLen)
{
    int l = rand() % (bufLen - 1);
    for (int i = 0; i < l; i++) {
        char c = (char)(33 + (rand() % (174 - 33)));
        buf[i] = c;
    }
    buf[l] = 0;
}

static void GenRandTStr(TCHAR *buf, int bufLen)
{
    int l = rand() % (bufLen - 1);
    for (int i = 0; i < l; i++) {
        TCHAR c = (TCHAR)(33 + (rand() % (174 - 33)));
        buf[i] = c;
    }
    buf[l] = 0;
}

static void BencTestStress()
{
    char key[64];
    char val[64];
    TCHAR tval[64];
    Vec<BencObj*> stack(29);
    BencDict *startDict = new BencDict();
    BencDict *d = startDict;
    BencArray *a = NULL;
    srand((unsigned int)time(NULL));
    // generate new dict or array with 5% probability each, close an array or
    // dict with 8% probability (less than 10% probability of opening one, to
    // encourage nesting), generate int, string or raw strings uniformly
    // across the remaining 72% probability
    for (int i = 0; i < 10000; i++)
    {
        int n = rand() % 100;
        if (n < 5) {
            BencDict *nd = new BencDict();
            if (a) {
                a->Add(nd);
            } else {
                GenRandStr(key, dimof(key));
                d->Add(key, nd);
            }
            stack.Push(nd);
            d = nd;
            a = NULL;
        } else if (n < 10) {
            BencArray *na = new BencArray();
            if (a) {
                a->Add(na);
            } else {
                GenRandStr(key, dimof(key));
                d->Add(key, na);
            }
            stack.Push(na);
            d = NULL;
            a = na;
        } else if (n < 18) {
            if (stack.Count() > 0) {
                n = rand() % 100;
                BencObj *o = stack.Pop();
                o = startDict;
                if (stack.Count() > 0) {
                    o = stack.Last();
                }
                a = NULL; d = NULL;
                if (BT_ARRAY == o->Type()) {
                    a = (BencArray*)o;
                } else {
                    d = (BencDict*)o;
                }
            }
        } else if (n < (18 + 24)) {
            int64_t v = rand();
            if (a) {
                a->Add(v);
            } else {
                GenRandStr(key, dimof(key));
                d->Add(key, v);
            }
        } else if (n < (18 + 24 + 24)) {
            GenRandStr(val, dimof(val));
            if (a) {
                a->AddRaw((const char*)val);
            } else {
                GenRandStr(key, dimof(key));
                d->AddRaw((const char*)key, val);
            }
        } else {
            GenRandTStr(tval, dimof(tval));
            if (a) {
                a->Add(tval);
            } else {
                GenRandStr(key, dimof(key));
                d->Add((const char*)key, (const TCHAR *)val);
            }
        }
    }

    char *s = startDict->Encode();
    free(s);
    delete startDict;
}

void BaseUtils_UnitTests()
{
    DBG_OUT("Running BaseUtils unit tests\n");
    GeomTest();
    TStrTest();
    FileUtilTest();
    VecTest();
    StrVecTest();
    WinUtilTest();
    LogTest();
    BencTestParseInt();
    BencTestParseString();
    BencTestParseRawStrings();
    BencTestParseArrays();
    BencTestParseDicts();
    BencTestArrayAppend();
    BencTestDictAppend();
    BencTestStress();
}

#endif
