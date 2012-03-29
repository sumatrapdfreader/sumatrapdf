#include "BaseUtil.h"
#include "StrUtil.h"
#include "Scoped.h"

static void TStrTest()
{
    TCHAR buf[32];
    TCHAR *str = _T("a string");
    assert(str::Len(str) == 8);
    assert(str::Eq(str, _T("a string")) && str::Eq(str, str));
    assert(!str::Eq(str, NULL) && !str::Eq(str, _T("A String")));
    assert(str::EqI(str, _T("A String")) && str::EqI(str, str));
    assert(!str::EqI(str, NULL) && str::EqI((char*)NULL, (char*)NULL));
    assert(str::EqN(_T("abcd"), _T("abce"), 3) && !str::EqN(_T("abcd"), _T("Abcd"), 3));
    assert(str::EqNI(_T("abcd"), _T("ABCE"), 3) && !str::EqNI(_T("abcd"), _T("Ebcd"), 3));
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
    assert(str::Parse(_T("a \r\n\t b"), _T("a%_b")));
    assert(str::Parse(_T("ab"), _T("a%_b")));
    assert(!str::Parse(_T("a,b"), _T("a%_b")));
    assert(str::Parse(_T("a\tb"), _T("a% b")));
    assert(!str::Parse(_T("a\r\nb"), _T("a% b")));
    assert(str::Parse(_T("a\r\nb"), _T("a% %_b")));
    assert(!str::Parse(_T("ab"), _T("a% b")));
    assert(!str::Parse(_T("%+"), _T("+")) && !str::Parse(_T("%+"), _T("%+")));

    assert(str::Parse("abcd", 3, "abc%$"));
    assert(str::Parse("abc", 3, "a%?bc%?d%$"));
    assert(!str::Parse("abcd", 3, "abcd"));

    {
        const char *str = "string";
        assert(str::Parse(str, 4, "str") == str + 3);

        float f1, f2;
        const TCHAR *end = str::Parse(_T("%1.23y -2e-3z"), _T("%%%fy%fz%$"), &f1, &f2);
        assert(end && !*end);
        assert(f1 == 1.23f && f2 == -2e-3f);
        f1 = 0; f2 = 0;
        const char *end2 = str::Parse("%1.23y -2e-3zlah", 13, "%%%fy%fz%$", &f1, &f2);
        assert(end2 && str::Eq(end2, "lah"));
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

    assert(ChrIsDigit('0') && ChrIsDigit(_T('5')) && ChrIsDigit(L'9'));
    assert(iswdigit(L'\xB2') && !ChrIsDigit(L'\xB2'));

    assert(str::CmpNatural(_T(".hg"), _T("2.pdf")) < 0);
    assert(str::CmpNatural(_T("100.pdf"), _T("2.pdf")) > 0);
    assert(str::CmpNatural(_T("2.pdf"), _T("zzz")) < 0);
    assert(str::CmpNatural(_T("abc"), _T(".svn")) > 0);
    assert(str::CmpNatural(_T("ab0200"), _T("AB333")) < 0);
    assert(str::CmpNatural(_T("a b"), _T("a  c")) < 0);

#ifndef LOCALE_INVARIANT
#define LOCALE_INVARIANT (MAKELCID(MAKELANGID(LANG_INVARIANT, SUBLANG_NEUTRAL), SORT_DEFAULT))
#endif

    struct {
        size_t number;
        const TCHAR *result;
    } formatNumData[] = {
        { 1,        _T("1") },
        { 12,       _T("12") },
        { 123,      _T("123") },
        { 1234,     _T("1,234") },
        { 12345,    _T("12,345") },
        { 123456,   _T("123,456") },
        { 1234567,  _T("1,234,567") },
        { 12345678, _T("12,345,678") },
    };

    for (int i = 0; i < dimof(formatNumData); i++) {
        ScopedMem<TCHAR> tmp(str::FormatNumWithThousandSep(formatNumData[i].number, LOCALE_INVARIANT));
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
        { 1234.5678,_T("1,234.57") },
    };

    for (int i = 0; i < dimof(formatFloatData); i++) {
        ScopedMem<TCHAR> tmp(str::FormatFloatWithThousandSep(formatFloatData[i].number, LOCALE_INVARIANT));
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

    assert(!str::ToMultiByte("abc", 9876, 123456));
    assert(!str::ToMultiByte(L"abc", 98765));
    assert(!str::conv::FromCodePage("abc", 12345));
    assert(!str::conv::ToCodePage(_T("abc"), 987654));

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
