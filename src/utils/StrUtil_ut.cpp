/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"

static void TStrTest()
{
    WCHAR buf[32];
    WCHAR *str = L"a string";
    assert(str::Len(str) == 8);
    assert(str::Eq(str, L"a string") && str::Eq(str, str));
    assert(!str::Eq(str, NULL) && !str::Eq(str, L"A String"));
    assert(str::EqI(str, L"A String") && str::EqI(str, str));
    assert(!str::EqI(str, NULL) && str::EqI((char*)NULL, (char*)NULL));
    assert(str::EqN(L"abcd", L"abce", 3) && !str::EqN(L"abcd", L"Abcd", 3));
    assert(str::EqNI(L"abcd", L"ABCE", 3) && !str::EqNI(L"abcd", L"Ebcd", 3));
    assert(str::StartsWith(str, L"a s") && str::StartsWithI(str, L"A Str"));
    assert(!str::StartsWith(str, L"Astr"));
    assert(str::EndsWith(str, L"ing") && str::EndsWithI(str, L"ING"));
    assert(!str::EndsWith(str, L"ung"));
    assert(str::IsEmpty((char*)NULL) && str::IsEmpty((WCHAR*)NULL)&& str::IsEmpty(L"") && !str::IsEmpty(str));
    assert(str::FindChar(str, _T('s')) && !str::FindChar(str, _T('S')));
    size_t len = str::BufSet(buf, dimof(buf), str);
    assert(len == str::Len(buf) && str::Eq(buf, str));
    len = str::BufSet(buf, 6, str);
    assert(len == 5 && str::Eq(buf, L"a str"));

    str = str::Dup(buf);
    assert(str::Eq(str, buf));
    free(str);
    str = str::DupN(buf, 4);
    assert(str::Eq(str, L"a st"));
    free(str);
    str = str::Format(L"%s", buf);
    assert(str::Eq(str, buf));
    free(str);
    str = str::Join(buf, buf);
    assert(str::Len(str) == 2 * str::Len(buf));
    free(str);
    str = str::Join(NULL, L"ab");
    assert(str::Eq(str, L"ab"));
    free(str);

    str::BufSet(buf, dimof(buf), L"abc\1efg\1");
    size_t count = str::TransChars(buf, L"ace", L"ACE");
    assert(str::Eq(buf, L"AbC\1Efg\1") && count == 3);
    count = str::TransChars(buf, L"\1", L"\0");
    assert(str::Eq(buf, L"AbC") && str::Eq(buf + 4, L"Efg") && count == 2);
    count = str::TransChars(buf, L"", L"X");
    assert(str::Eq(buf, L"AbC") && count == 0);

    str::BufSet(buf, dimof(buf), L"blogarapato");
    count = str::RemoveChars(buf, L"bo");
    assert(3 == count);
    assert(str::Eq(buf, L"lgarapat"));

    str::BufSet(buf, dimof(buf), L"one\r\ntwo\t\v\f\tthree");
    count = str::NormalizeWS(buf);
    assert(4 == count);
    assert(str::Eq(buf, L"one two three"));

    str::BufSet(buf, dimof(buf), L" one    two three ");
    count = str::NormalizeWS(buf);
    assert(5 == count);
    assert(str::Eq(buf, L"one two three"));

    count = str::NormalizeWS(buf);
    assert(0 == count);
    assert(str::Eq(buf, L"one two three"));

    str = _T("[Open(\"filename.pdf\",0,1,0)]");
    {
        UINT u1 = 0;
        WCHAR *str1 = NULL;
        const WCHAR *end = str::Parse(str, _T("[Open(\"%s\",%? 0,%u,0)]"), &str1, &u1);
        assert(end && !*end);
        assert(u1 == 1 && str::Eq(str1, L"filename.pdf"));
        free(str1);
    }

    {
        UINT u1 = 0;
        ScopedMem<WCHAR> str1;
        const WCHAR *end = str::Parse(str, _T("[Open(\"%S\",0%?,%u,0)]"), &str1, &u1);
        assert(end && !*end);
        assert(u1 == 1 && str::Eq(str1, L"filename.pdf"));

        assert(str::Parse(L"0xABCD", L"%x", &u1));
        assert(u1 == 0xABCD);
        assert(str::Parse(L"ABCD", L"%2x%S", &u1, &str1));
        assert(u1 == 0xAB && str::Eq(str1, L"CD"));
    }

    {
        int i1, i2;
        const WCHAR *end = str::Parse(L"1, 2+3", L"%d,%d", &i1, &i2);
        assert(end && str::Eq(end, L"+3"));
        assert(i1 == 1 && i2 == 2);
        end = str::Parse(end, L"+3");
        assert(end && !*end);

        assert(str::Parse(L" -2", L"%d", &i1));
        assert(i1 == -2);
        assert(str::Parse(L" 2", L" %u", &i1));
        assert(i1 == 2);
        assert(str::Parse(L"123-456", L"%3d%3d6", &i1, &i2));
        assert(i1 == 123 && i2 == -45);
        assert(!str::Parse(L"123", L"%4d", &i1));
        assert(str::Parse(L"654", L"%3d", &i1));
        assert(i1 == 654);
    }

    assert(str::Parse(L"abc", L"abc%$"));
    assert(str::Parse(L"abc", L"a%?bc%?d%$"));
    assert(!str::Parse(L"abc", L"ab%$"));
    assert(str::Parse(L"a \r\n\t b", L"a%_b"));
    assert(str::Parse(L"ab", L"a%_b"));
    assert(!str::Parse(L"a,b", L"a%_b"));
    assert(str::Parse(L"a\tb", L"a% b"));
    assert(!str::Parse(L"a\r\nb", L"a% b"));
    assert(str::Parse(L"a\r\nb", L"a% %_b"));
    assert(!str::Parse(L"ab", L"a% b"));
    assert(!str::Parse(L"%+", L"+") && !str::Parse(L"%+", L"%+"));

    assert(str::Parse("abcd", 3, "abc%$"));
    assert(str::Parse("abc", 3, "a%?bc%?d%$"));
    assert(!str::Parse("abcd", 3, "abcd"));

    {
        const char *str = "string";
        assert(str::Parse(str, 4, "str") == str + 3);

        float f1, f2;
        const WCHAR *end = str::Parse(L"%1.23y -2e-3z", L"%%%fy%fz%$", &f1, &f2);
        assert(end && !*end);
        assert(f1 == 1.23f && f2 == -2e-3f);
        f1 = 0; f2 = 0;
        const char *end2 = str::Parse("%1.23y -2e-3zlah", 13, "%%%fy%fz%$", &f1, &f2);
        assert(end2 && str::Eq(end2, "lah"));
        assert(f1 == 1.23f && f2 == -2e-3f);
    }

    {
        WCHAR *str1 = NULL;
        WCHAR c1;
        assert(!str::Parse(L"no exclamation mark?", L"%s!", &str1));
        assert(!str1);
        assert(str::Parse(L"xyz", L"x%cz", &c1));
        assert(c1 == 'y');
        assert(!str::Parse(L"leaks memory!?", L"%s!%$", &str1));
        free(str1);
    }

    {
        ScopedMem<char> str;
        int i, j;
        float f;
        assert(str::Parse("ansi string, -30-20 1.5%", "%S,%d%?-%2u%f%%%$", &str, &i, &j, &f));
        assert(str::Eq(str, "ansi string") && i == -30 && j == 20 && f == 1.5f);
    }
    {
        ScopedMem<WCHAR> str;
        int i, j;
        float f;
        assert(str::Parse(L"wide string, -30-20 1.5%", L"%S,%d%?-%2u%f%%%$", &str, &i, &j, &f));
        assert(str::Eq(str, L"wide string") && i == -30 && j == 20 && f == 1.5f);
    }

    {
        const char *path = "M10 80 C 40 10, 65\r\n10,\t95\t80 S 150 150, 180 80\nA 45 45, 0, 1, 0, 125 125\nA 1 2 3\n0\n1\n20  -20";
        float f[6];
        int b[2];
        const char *s = str::Parse(path, "M%f%_%f", &f[0], &f[1]);
        assert(s && f[0] == 10 && f[1] == 80);
        s = str::Parse(s + 1, "C%f%_%f,%f%_%f,%f%_%f", &f[0], &f[1], &f[2], &f[3], &f[4], &f[5]);
        assert(s && f[0] == 40 && f[1] == 10 && f[2] == 65 && f[3] == 10 && f[4] == 95 && f[5] == 80);
        s = str::Parse(s + 1, "S%f%_%f,%f%_%f", &f[0], &f[1], &f[2], &f[3], &f[4]);
        assert(s && f[0] == 150 && f[1] == 150 && f[2] == 180 && f[3] == 80);
        s = str::Parse(s + 1, "A%f%_%f%?,%f%?,%d%?,%d%?,%f%_%f", &f[0], &f[1], &f[2], &b[0], &b[1], &f[4], &f[5]);
        assert(s && f[0] == 45 && f[1] == 45 && f[2] == 0 && b[0] == 1 && b[1] == 0 && f[4] == 125 && f[5] == 125);
        s = str::Parse(s + 1, "A%f%_%f%?,%f%?,%d%?,%d%?,%f%_%f", &f[0], &f[1], &f[2], &b[0], &b[1], &f[4], &f[5]);
        assert(s && f[0] == 1 && f[1] == 2 && f[2] == 3 && b[0] == 0 && b[1] == 1 && f[4] == 20 && f[5] == -20);
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

    assert(str::IsDigit('0') && str::IsDigit(_T('5')) && str::IsDigit(L'9'));
    assert(iswdigit(L'\xB2') && !str::IsDigit(L'\xB2'));

    assert(str::CmpNatural(L".hg", L"2.pdf") < 0);
    assert(str::CmpNatural(L"100.pdf", L"2.pdf") > 0);
    assert(str::CmpNatural(L"2.pdf", L"zzz") < 0);
    assert(str::CmpNatural(L"abc", L".svn") > 0);
    assert(str::CmpNatural(L"ab0200", L"AB333") < 0);
    assert(str::CmpNatural(L"a b", L"a  c") < 0);

#ifndef LOCALE_INVARIANT
#define LOCALE_INVARIANT (MAKELCID(MAKELANGID(LANG_INVARIANT, SUBLANG_NEUTRAL), SORT_DEFAULT))
#endif

    struct {
        size_t number;
        const WCHAR *result;
    } formatNumData[] = {
        { 1,        L"1" },
        { 12,       L"12" },
        { 123,      L"123" },
        { 1234,     L"1,234" },
        { 12345,    L"12,345" },
        { 123456,   L"123,456" },
        { 1234567,  L"1,234,567" },
        { 12345678, L"12,345,678" },
    };

    for (int i = 0; i < dimof(formatNumData); i++) {
        ScopedMem<WCHAR> tmp(str::FormatNumWithThousandSep(formatNumData[i].number, LOCALE_INVARIANT));
        assert(str::Eq(tmp, formatNumData[i].result));
    }

    struct {
        double number;
        const WCHAR *result;
    } formatFloatData[] = {
        { 1,        L"1.0" },
        { 1.2,      L"1.2" },
        { 1.23,     L"1.23" },
        { 1.234,    L"1.23" },
        { 12.345,   L"12.35" },
        { 123.456,  L"123.46" },
        { 1234.5678,L"1,234.57" },
    };

    for (int i = 0; i < dimof(formatFloatData); i++) {
        ScopedMem<WCHAR> tmp(str::FormatFloatWithThousandSep(formatFloatData[i].number, LOCALE_INVARIANT));
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
        const WCHAR *result;
    } formatRomanData[] = {
        { 1,    L"I" },
        { 3,    L"III" },
        { 6,    L"VI" },
        { 14,   L"XIV" },
        { 49,   L"XLIX" },
        { 176,  L"CLXXVI" },
        { 499,  L"CDXCIX" },
        { 1666, L"MDCLXVI" },
        { 2011, L"MMXI" },
        { 12345,L"MMMMMMMMMMMMCCCXLV" },
        { 0,    NULL },
        { -133, NULL },
    };

    for (int i = 0; i < dimof(formatRomanData); i++) {
        ScopedMem<WCHAR> tmp(str::FormatRomanNumeral(formatRomanData[i].number));
        assert(str::Eq(tmp, formatRomanData[i].result));
    }

    {
        size_t trimmed;
        WCHAR *s = NULL;
        s = str::Dup(L"");
        trimmed = str::TrimWS(s);
        assert(trimmed == 0);
        assert(str::Eq(s, L""));
        trimmed = str::TrimWS(s, str::TrimRight);
        assert(trimmed == 0);
        assert(str::Eq(s, L""));
        trimmed = str::TrimWS(s, str::TrimLeft);
        assert(trimmed == 0);
        assert(str::Eq(s, L""));

        free(s); s = str::Dup(L"  \n\t  ");
        trimmed = str::TrimWS(s);
        assert(trimmed == 6);
        assert(str::Eq(s, L""));

        free(s); s = str::Dup(L"  \n\t  ");
        trimmed = str::TrimWS(s, str::TrimRight);
        assert(trimmed == 6);
        assert(str::Eq(s, L""));

        free(s); s = str::Dup(L"  \n\t  ");
        trimmed = str::TrimWS(s, str::TrimLeft);
        assert(trimmed == 6);
        assert(str::Eq(s, L""));

        free(s); s = str::Dup(L"  lola");
        trimmed = str::TrimWS(s);
        assert(trimmed == 2);
        assert(str::Eq(s, L"lola"));

        free(s); s = str::Dup(L"  lola");
        trimmed = str::TrimWS(s, str::TrimLeft);
        assert(trimmed == 2);
        assert(str::Eq(s, L"lola"));

        free(s); s = str::Dup(L"  lola");
        trimmed = str::TrimWS(s, str::TrimRight);
        assert(trimmed == 0);
        assert(str::Eq(s, L"  lola"));

        free(s); s = str::Dup(L"lola\r\t");
        trimmed = str::TrimWS(s);
        assert(trimmed == 2);
        assert(str::Eq(s, L"lola"));

        free(s); s = str::Dup(L"lola\r\t");
        trimmed = str::TrimWS(s, str::TrimRight);
        assert(trimmed == 2);
        assert(str::Eq(s, L"lola"));

        free(s); s = str::Dup(L"lola\r\t");
        trimmed = str::TrimWS(s, str::TrimLeft);
        assert(trimmed == 0);
        assert(str::Eq(s, L"lola\r\t"));

        free(s);
    }

    assert(!str::ToMultiByte("abc", 9876, 123456));
    assert(!str::ToMultiByte(L"abc", 98765));
    assert(!str::conv::FromCodePage("abc", 12345));
    assert(!str::conv::ToCodePage(L"abc", 987654));

    {
        char buf[6] = { 0 };
        int cnt = str::BufAppend(buf, dimof(buf), "");
        assert(0 == cnt);
        cnt = str::BufAppend(buf, dimof(buf), "1234");
        assert(4 == cnt);
        assert(str::Eq("1234", buf));
        cnt = str::BufAppend(buf, dimof(buf), "56");
        assert(1 == cnt);
        assert(str::Eq("12345", buf));
        cnt = str::BufAppend(buf, dimof(buf), "6");
        assert(0 == cnt);
        assert(str::Eq("12345", buf));
    }

    {
        WCHAR buf[6] = { 0 };
        int cnt = str::BufAppend(buf, dimof(buf), L"");
        assert(0 == cnt);
        cnt = str::BufAppend(buf, dimof(buf), L"1234");
        assert(4 == cnt);
        assert(str::Eq(L"1234", buf));
        cnt = str::BufAppend(buf, dimof(buf), L"56");
        assert(1 == cnt);
        assert(str::Eq(L"12345", buf));
        cnt = str::BufAppend(buf, dimof(buf), L"6");
        assert(0 == cnt);
        assert(str::Eq(L"12345", buf));
    }

}
