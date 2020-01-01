/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

static void StrReplaceTestOne(const char* s, const char* toReplace, const char* replaceWith, const char* expected) {
    char* res = str::Replace(s, toReplace, replaceWith);
    utassert(str::Eq(res, expected));
    free(res);
}

static void StrReplaceTest() {
    const char* d[] = {
        "golagon", "gon",   "rabato", "golarabato", "a",   "a",      "bor", "bor", "abora", "a",
        "",        "bor",   "aaaaaa", "a",          "b",   "bbbbbb", "aba", "a",   "ccc",   "cccbccc",
        "Aba",     "a",     "c",      "Abc",        "abc", "abc",    "",    "",    nullptr, "a",
        "b",       nullptr, "a",      "",           "b",   nullptr,  "a",   "b",   nullptr, nullptr,
    };
    size_t n = dimof(d) / 4;
    for (size_t i = 0; i < n; i++) {
        StrReplaceTestOne(d[i * 4], d[i * 4 + 1], d[i * 4 + 2], d[i * 4 + 3]);
    }

    struct {
        const WCHAR *string, *find, *replace, *result;
    } data[] = {
        {L"golagon", L"gon", L"rabato", L"golarabato"},
        {L"a", L"a", L"bor", L"bor"},
        {L"abora", L"a", L"", L"bor"},
        {L"aaaaaa", L"a", L"b", L"bbbbbb"},
        {L"aba", L"a", L"ccc", L"cccbccc"},
        {L"Aba", L"a", L"c", L"Abc"},
        {L"abc", L"abc", L"", L""},
        {nullptr, L"a", L"b", nullptr},
        {L"a", L"", L"b", nullptr},
        {L"a", L"b", nullptr, nullptr},
    };
    for (size_t i = 0; i < dimof(data); i++) {
        AutoFreeWstr result(str::Replace(data[i].string, data[i].find, data[i].replace));
        utassert(str::Eq(result, data[i].result));
    }
}

static void StrSeqTest() {
    const char* s = "foo\0a\0bar\0";
    utassert(0 == seqstrings::StrToIdx(s, "foo"));
    utassert(1 == seqstrings::StrToIdx(s, "a"));
    utassert(2 == seqstrings::StrToIdx(s, "bar"));

    utassert(0 == seqstrings::StrToIdx(s, L"foo"));
    utassert(1 == seqstrings::StrToIdx(s, L"a"));
    utassert(2 == seqstrings::StrToIdx(s, L"bar"));

    utassert(str::Eq("foo", seqstrings::IdxToStr(s, 0)));
    utassert(str::Eq("a", seqstrings::IdxToStr(s, 1)));
    utassert(str::Eq("bar", seqstrings::IdxToStr(s, 2)));

    utassert(0 == seqstrings::StrToIdx(s, "foo"));
    utassert(1 == seqstrings::StrToIdx(s, "a"));
    utassert(2 == seqstrings::StrToIdx(s, "bar"));
    utassert(-1 == seqstrings::StrToIdx(s, "fo"));
    utassert(-1 == seqstrings::StrToIdx(s, ""));
    utassert(-1 == seqstrings::StrToIdx(s, "ab"));
    utassert(-1 == seqstrings::StrToIdx(s, "baro"));
    utassert(-1 == seqstrings::StrToIdx(s, "ba"));

    utassert(0 == seqstrings::StrToIdx(s, L"foo"));
    utassert(1 == seqstrings::StrToIdx(s, L"a"));
    utassert(2 == seqstrings::StrToIdx(s, L"bar"));
    utassert(-1 == seqstrings::StrToIdx(s, L"fo"));
    utassert(-1 == seqstrings::StrToIdx(s, L""));
    utassert(-1 == seqstrings::StrToIdx(s, L"ab"));
    utassert(-1 == seqstrings::StrToIdx(s, L"baro"));
    utassert(-1 == seqstrings::StrToIdx(s, L"ba"));
}

static void StrIsDigitTest() {
    const char* nonDigits = "/:.bz{}";
    const char* digits = "0123456789";
    for (size_t i = 0; i < str::Len(nonDigits); i++) {
#if 0
        if (str::IsDigit(nonDigits[i])) {
            char c = nonDigits[i];
            printf("%c is incorrectly determined as a digit\n", c);
        }
#endif
        utassert(!str::IsDigit(nonDigits[i]));
    }
    for (size_t i = 0; i < str::Len(digits); i++) {
        utassert(str::IsDigit(digits[i]));
    }

    const WCHAR* nonDigitsW = L"/:.bz{}";
    const WCHAR* digitsW = L"0123456789";
    for (size_t i = 0; i < str::Len(nonDigitsW); i++) {
        utassert(!str::IsDigit(nonDigitsW[i]));
    }
    for (size_t i = 0; i < str::Len(digitsW); i++) {
        utassert(str::IsDigit(digitsW[i]));
    }
}

static void StrConvTest() {
    WCHAR wbuf[4];
    char cbuf[4];
    size_t conv = strconv::Utf8ToWcharBuf("testing", 4, wbuf, dimof(wbuf));
    utassert(conv == 3 && str::Eq(wbuf, L"tes"));
    conv = strconv::WcharToUtf8Buf(L"abc", cbuf, dimof(cbuf));
    utassert(conv == 3 && str::Eq(cbuf, "abc"));
    conv = strconv::Utf8ToWcharBuf("ab\xF0\x90\x82\x80", 6, wbuf, dimof(wbuf));
    utassert(conv == 3 && str::StartsWith(wbuf, L"ab") && wbuf[2] == 0xD800);
    conv = strconv::Utf8ToWcharBuf("ab\xF0\x90\x82\x80", 6, wbuf, dimof(wbuf) - 1);
    utassert(conv == 1 && str::Eq(wbuf, L"a"));
    conv = strconv::WcharToUtf8Buf(L"ab\u20AC", cbuf, dimof(cbuf));
    utassert(conv == 0 && str::Eq(cbuf, ""));
    conv = strconv::WcharToUtf8Buf(L"abcd", cbuf, dimof(cbuf));
    utassert(conv == 0 && str::Eq(cbuf, ""));
}

static void StrUrlExtractTest() {
    utassert(!url::GetFileName(L""));
    utassert(!url::GetFileName(L"#hash_only"));
    utassert(!url::GetFileName(L"?query=only"));
    AutoFreeWstr fileName(url::GetFileName(L"http://example.net/filename.ext"));
    utassert(str::Eq(fileName, L"filename.ext"));
    fileName.Set(url::GetFileName(L"http://example.net/filename.ext#with_hash"));
    utassert(str::Eq(fileName, L"filename.ext"));
    fileName.Set(url::GetFileName(L"http://example.net/path/to/filename.ext?more=data"));
    utassert(str::Eq(fileName, L"filename.ext"));
    fileName.Set(url::GetFileName(L"http://example.net/pa%74h/na%2f%6d%65%2ee%78t"));
    utassert(str::Eq(fileName, L"na/me.ext"));
    fileName.Set(url::GetFileName(L"http://example.net/%E2%82%AC"));
    utassert(str::Eq((char*)fileName.Get(), "\xAC\x20"));
}

static void ParseUntilTest() {
    const char* txt = "foo\nbar\n\nla\n";
    const char* a[] = {
        "foo",
        "bar",
        "",
        "la",
    };
    size_t nEls = dimof(a);
    {
        std::string_view sv(txt);
        size_t i = 0;
        while (true) {
            auto el = sv::ParseUntil(sv, '\n');
            const char* got = el.data();
            if (got == nullptr) {
                utassert(i == dimof(a));
                break;
            }
            const char* s = a[i];
            size_t len = str::Len(s);
            utassert(len == el.size());
            utassert(str::EqN(s, got, len));
            i++;
            utassert(i <= nEls);
        }
    }
    {
        std::string_view sv(txt, str::Len(txt) - 1);
        size_t i = 0;
        while (true) {
            auto el = sv::ParseUntilBack(sv, '\n');
            const char* got = el.data();
            if (got == nullptr) {
                utassert(i == dimof(a));
                break;
            }
            const char* s = a[nEls - 1 - i];
            size_t len = str::Len(s);
            utassert(len == el.size());
            utassert(str::EqN(s, got, len));
            i++;
            utassert(i <= nEls);
        }
    }
}

void StrTest() {
    WCHAR buf[32];
    WCHAR* str = L"a string";
    utassert(str::Len(str) == 8);
    utassert(str::Eq(str, L"a string") && str::Eq(str, str));
    utassert(!str::Eq(str, nullptr) && !str::Eq(str, L"A String"));
    utassert(str::EqI(str, L"A String") && str::EqI(str, str));
    utassert(!str::EqI(str, nullptr) && str::EqI((char*)nullptr, (char*)nullptr));
    utassert(str::EqN(L"abcd", L"abce", 3) && !str::EqN(L"abcd", L"Abcd", 3));
    utassert(str::EqNI(L"abcd", L"ABCE", 3) && !str::EqNI(L"abcd", L"Ebcd", 3));
    utassert(str::StartsWith(str, L"a s") && str::StartsWithI(str, L"A Str"));
    utassert(!str::StartsWith(str, L"Astr"));
    utassert(str::EndsWith(str, L"ing") && str::EndsWithI(str, L"ING"));
    utassert(!str::EndsWith(str, L"ung"));
    utassert(str::IsEmpty((char*)nullptr) && str::IsEmpty((WCHAR*)nullptr) && str::IsEmpty(L"") && !str::IsEmpty(str));
    utassert(str::FindChar(str, 's') && !str::FindChar(str, 'S'));
    size_t len = str::BufSet(buf, dimof(buf), str);
    utassert(len == str::Len(buf) && str::Eq(buf, str));
    len = str::BufSet(buf, 6, str);
    utassert(len == 5 && str::Eq(buf, L"a str"));

    str = str::Dup(buf);
    utassert(str::Eq(str, buf));
    free(str);
    str = str::DupN(buf, 4);
    utassert(str::Eq(str, L"a st"));
    free(str);
    str = str::Format(L"%s", buf);
    utassert(str::Eq(str, buf));
    free(str);
    {
        AutoFreeWstr large(AllocArray<WCHAR>(2000));
        memset(large, 0x11, 1998);
        str = str::Format(L"%s", large.get());
        utassert(str::Eq(str, large));
        free(str);
    }
#if 0
    // TODO: this test slows down DEBUG builds significantly
    str = str::Format(L"%s", L"\uFFFF");
    // TODO: in VS2015, str matches L"\uFFFF" instead of nullptr
    utassert(str::Eq(str, nullptr));
    free(str);
#endif
    str = str::Join(buf, buf);
    utassert(str::Len(str) == 2 * str::Len(buf));
    free(str);
    str = str::Join(nullptr, L"ab");
    utassert(str::Eq(str, L"ab"));
    free(str);
    str = str::Join(L"\uFDEF", L"\uFFFF");
    utassert(str::Eq(str, L"\uFDEF\uFFFF"));
    free(str);

    str::BufSet(buf, dimof(buf), L"abc\1efg\1");
    size_t count = str::TransChars(buf, L"ace", L"ACE");
    utassert(str::Eq(buf, L"AbC\1Efg\1") && count == 3);
    count = str::TransChars(buf, L"\1", L"\0");
    utassert(str::Eq(buf, L"AbC") && str::Eq(buf + 4, L"Efg") && count == 2);
    count = str::TransChars(buf, L"", L"X");
    utassert(str::Eq(buf, L"AbC") && count == 0);

    str::BufSet(buf, dimof(buf), L"blogarapato");
    count = str::RemoveChars(buf, L"bo");
    utassert(3 == count);
    utassert(str::Eq(buf, L"lgarapat"));

    str::BufSet(buf, dimof(buf), L"one\r\ntwo\t\v\f\tthree");
    count = str::NormalizeWS(buf);
    utassert(4 == count);
    utassert(str::Eq(buf, L"one two three"));

    str::BufSet(buf, dimof(buf), L" one    two three ");
    count = str::NormalizeWS(buf);
    utassert(5 == count);
    utassert(str::Eq(buf, L"one two three"));

    count = str::NormalizeWS(buf);
    utassert(0 == count);
    utassert(str::Eq(buf, L"one two three"));

    str = L"[Open(\"filename.pdf\",0,1,0)]";
    {
        UINT u1 = 0;
        WCHAR* str1 = nullptr;
        const WCHAR* end = str::Parse(str, L"[Open(\"%s\",%? 0,%u,0)]", &str1, &u1);
        utassert(end && !*end);
        utassert(u1 == 1 && str::Eq(str1, L"filename.pdf"));
        free(str1);
    }

    {
        UINT u1 = 0;
        AutoFreeWstr str1;
        const WCHAR* end = str::Parse(str, L"[Open(\"%S\",0%?,%u,0)]", &str1, &u1);
        utassert(end && !*end);
        utassert(u1 == 1 && str::Eq(str1, L"filename.pdf"));

        utassert(str::Parse(L"0xABCD", L"%x", &u1));
        utassert(u1 == 0xABCD);
        utassert(str::Parse(L"ABCD", L"%2x%S", &u1, &str1));
        utassert(u1 == 0xAB && str::Eq(str1, L"CD"));
    }

    {
        int i1, i2;
        const WCHAR* end = str::Parse(L"1, 2+3", L"%d,%d", &i1, &i2);
        utassert(end && str::Eq(end, L"+3"));
        utassert(i1 == 1 && i2 == 2);
        end = str::Parse(end, L"+3");
        utassert(end && !*end);

        utassert(str::Parse(L" -2", L"%d", &i1));
        utassert(i1 == -2);
        utassert(str::Parse(L" 2", L" %u", &i1));
        utassert(i1 == 2);
        utassert(str::Parse(L"123-456", L"%3d%3d6", &i1, &i2));
        utassert(i1 == 123 && i2 == -45);
        utassert(!str::Parse(L"123", L"%4d", &i1));
        utassert(str::Parse(L"654", L"%3d", &i1));
        utassert(i1 == 654);
    }

    utassert(str::Parse(L"abc", L"abc%$"));
    utassert(str::Parse(L"abc", L"a%?bc%?d%$"));
    utassert(!str::Parse(L"abc", L"ab%$"));
    utassert(str::Parse(L"a \r\n\t b", L"a%_b"));
    utassert(str::Parse(L"ab", L"a%_b"));
    utassert(!str::Parse(L"a,b", L"a%_b"));
    utassert(str::Parse(L"a\tb", L"a% b"));
    utassert(!str::Parse(L"a\r\nb", L"a% b"));
    utassert(str::Parse(L"a\r\nb", L"a% %_b"));
    utassert(!str::Parse(L"ab", L"a% b"));
    utassert(!str::Parse(L"%+", L"+") && !str::Parse(L"%+", L"%+"));

    utassert(str::Parse("abcd", 3, "abc%$"));
    utassert(str::Parse("abc", 3, "a%?bc%?d%$"));
    utassert(!str::Parse("abcd", 3, "abcd"));

    {
        const char* str1 = "string";
        utassert(str::Parse(str1, 4, "str") == str1 + 3);

        float f1, f2;
        const WCHAR* end = str::Parse(L"%1.23y -2e-3z", L"%%%fy%fz%$", &f1, &f2);
        utassert(end && !*end);
        utassert(f1 == 1.23f && f2 == -2e-3f);
        f1 = 0;
        f2 = 0;
        const char* end2 = str::Parse("%1.23y -2e-3zlah", 13, "%%%fy%fz%$", &f1, &f2);
        utassert(end2 && str::Eq(end2, "lah"));
        utassert(f1 == 1.23f && f2 == -2e-3f);
    }

    {
        WCHAR* str1 = nullptr;
        WCHAR c1;
        utassert(!str::Parse(L"no exclamation mark?", L"%s!", &str1));
        utassert(!str1);
        utassert(str::Parse(L"xyz", L"x%cz", &c1));
        utassert(c1 == 'y');
        utassert(!str::Parse(L"leaks memory!?", L"%s!%$", &str1));
        free(str1);
    }

    {
        AutoFree str1;
        int i, j;
        float f;
        utassert(str::Parse("ansi string, -30-20 1.5%", "%S,%d%?-%2u%f%%%$", &str1, &i, &j, &f));
        utassert(str::Eq(str1, "ansi string") && i == -30 && j == 20 && f == 1.5f);
    }
    {
        AutoFreeWstr str1;
        int i, j;
        float f;
        utassert(str::Parse(L"wide string, -30-20 1.5%", L"%S,%d%?-%2u%f%%%$", &str1, &i, &j, &f));
        utassert(str::Eq(str1, L"wide string") && i == -30 && j == 20 && f == 1.5f);
    }

    {
        const char* path =
            "M10 80 C 40 10, 65\r\n10,\t95\t80 S 150 150, 180 80\nA 45 45, 0, 1, 0, 125 125\nA 1 2 3\n0\n1\n20  -20";
        float f[6];
        int b[2];
        const char* s = str::Parse(path, "M%f%_%f", &f[0], &f[1]);
        utassert(s && f[0] == 10 && f[1] == 80);
        s = str::Parse(s + 1, "C%f%_%f,%f%_%f,%f%_%f", &f[0], &f[1], &f[2], &f[3], &f[4], &f[5]);
        utassert(s && f[0] == 40 && f[1] == 10 && f[2] == 65 && f[3] == 10 && f[4] == 95 && f[5] == 80);
        s = str::Parse(s + 1, "S%f%_%f,%f%_%f", &f[0], &f[1], &f[2], &f[3], &f[4]);
        utassert(s && f[0] == 150 && f[1] == 150 && f[2] == 180 && f[3] == 80);
        s = str::Parse(s + 1, "A%f%_%f%?,%f%?,%d%?,%d%?,%f%_%f", &f[0], &f[1], &f[2], &b[0], &b[1], &f[4], &f[5]);
        utassert(s && f[0] == 45 && f[1] == 45 && f[2] == 0 && b[0] == 1 && b[1] == 0 && f[4] == 125 && f[5] == 125);
        s = str::Parse(s + 1, "A%f%_%f%?,%f%?,%d%?,%d%?,%f%_%f", &f[0], &f[1], &f[2], &b[0], &b[1], &f[4], &f[5]);
        utassert(s && f[0] == 1 && f[1] == 2 && f[2] == 3 && b[0] == 0 && b[1] == 1 && f[4] == 20 && f[5] == -20);
    }

    // the test string should only contain ASCII characters,
    // as all others might not be available in all code pages
#define TEST_STRING "aBc"
    AutoFree strA = strconv::WstrToAnsi(TEXT(TEST_STRING));
    utassert(str::Eq(strA.Get(), TEST_STRING));
    str = strconv::FromAnsi(strA.Get());
    utassert(str::Eq(str, TEXT(TEST_STRING)));
    free(str);
#undef TEST_STRING

    utassert(str::IsDigit('0') && str::IsDigit(TEXT('5')) && str::IsDigit(L'9'));
    utassert(iswdigit(L'\u0660') && !str::IsDigit(L'\xB2'));

    utassert(str::CmpNatural(L".hg", L"2.pdf") < 0);
    utassert(str::CmpNatural(L"100.pdf", L"2.pdf") > 0);
    utassert(str::CmpNatural(L"2.pdf", L"zzz") < 0);
    utassert(str::CmpNatural(L"abc", L".svn") > 0);
    utassert(str::CmpNatural(L"ab0200", L"AB333") < 0);
    utassert(str::CmpNatural(L"a b", L"a  c") < 0);

#ifndef LOCALE_INVARIANT
#define LOCALE_INVARIANT (MAKELCID(MAKELANGID(LANG_INVARIANT, SUBLANG_NEUTRAL), SORT_DEFAULT))
#endif

    struct {
        size_t number;
        const WCHAR* result;
    } formatNumData[] = {
        {1, L"1"},          {12, L"12"},          {123, L"123"},           {1234, L"1,234"},
        {12345, L"12,345"}, {123456, L"123,456"}, {1234567, L"1,234,567"}, {12345678, L"12,345,678"},
    };

    for (int i = 0; i < dimof(formatNumData); i++) {
        AutoFreeWstr tmp(str::FormatNumWithThousandSep(formatNumData[i].number, LOCALE_INVARIANT));
        utassert(str::Eq(tmp, formatNumData[i].result));
    }

    struct {
        double number;
        const WCHAR* result;
    } formatFloatData[] = {
        {1, L"1.0"},        {1.2, L"1.2"},        {1.23, L"1.23"},          {1.234, L"1.23"},
        {12.345, L"12.35"}, {123.456, L"123.46"}, {1234.5678, L"1,234.57"},
    };

    for (int i = 0; i < dimof(formatFloatData); i++) {
        AutoFreeWstr tmp(str::FormatFloatWithThousandSep(formatFloatData[i].number, LOCALE_INVARIANT));
        utassert(str::Eq(tmp, formatFloatData[i].result));
    }

    {
        char str1[] = "aAbBcC... 1-9";
        str::ToLowerInPlace(str1);
        utassert(str::Eq(str1, "aabbcc... 1-9"));

        WCHAR wstr[] = L"aAbBcC... 1-9";
        str::ToLowerInPlace(wstr);
        utassert(str::Eq(wstr, L"aabbcc... 1-9"));
    }

    struct {
        int number;
        const WCHAR* result;
    } formatRomanData[] = {
        {1, L"I"},        {3, L"III"},      {6, L"VI"},         {14, L"XIV"},    {49, L"XLIX"},
        {176, L"CLXXVI"}, {499, L"CDXCIX"}, {1666, L"MDCLXVI"}, {2011, L"MMXI"}, {12345, L"MMMMMMMMMMMMCCCXLV"},
        {0, nullptr},     {-133, nullptr},
    };

    for (int i = 0; i < dimof(formatRomanData); i++) {
        AutoFreeWstr tmp(str::FormatRomanNumeral(formatRomanData[i].number));
        utassert(str::Eq(tmp, formatRomanData[i].result));
    }

    {
        size_t trimmed;
        WCHAR* s = str::Dup(L"");
        trimmed = str::TrimWS(s, str::TrimOpt::Both);
        utassert(trimmed == 0);
        utassert(str::Eq(s, L""));
        trimmed = str::TrimWS(s, str::TrimOpt::Right);
        utassert(trimmed == 0);
        utassert(str::Eq(s, L""));
        trimmed = str::TrimWS(s, str::TrimOpt::Left);
        utassert(trimmed == 0);
        utassert(str::Eq(s, L""));

        free(s);
        s = str::Dup(L"  \n\t  ");
        trimmed = str::TrimWS(s, str::TrimOpt::Both);
        utassert(trimmed == 6);
        utassert(str::Eq(s, L""));

        free(s);
        s = str::Dup(L"  \n\t  ");
        trimmed = str::TrimWS(s, str::TrimOpt::Right);
        utassert(trimmed == 6);
        utassert(str::Eq(s, L""));

        free(s);
        s = str::Dup(L"  \n\t  ");
        trimmed = str::TrimWS(s, str::TrimOpt::Left);
        utassert(trimmed == 6);
        utassert(str::Eq(s, L""));

        free(s);
        s = str::Dup(L"  lola");
        trimmed = str::TrimWS(s, str::TrimOpt::Both);
        utassert(trimmed == 2);
        utassert(str::Eq(s, L"lola"));

        free(s);
        s = str::Dup(L"  lola");
        trimmed = str::TrimWS(s, str::TrimOpt::Left);
        utassert(trimmed == 2);
        utassert(str::Eq(s, L"lola"));

        free(s);
        s = str::Dup(L"  lola");
        trimmed = str::TrimWS(s, str::TrimOpt::Right);
        utassert(trimmed == 0);
        utassert(str::Eq(s, L"  lola"));

        free(s);
        s = str::Dup(L"lola\r\t");
        trimmed = str::TrimWS(s, str::TrimOpt::Both);
        utassert(trimmed == 2);
        utassert(str::Eq(s, L"lola"));

        free(s);
        s = str::Dup(L"lola\r\t");
        trimmed = str::TrimWS(s, str::TrimOpt::Right);
        utassert(trimmed == 2);
        utassert(str::Eq(s, L"lola"));

        free(s);
        s = str::Dup(L"lola\r\t");
        trimmed = str::TrimWS(s, str::TrimOpt::Left);
        utassert(trimmed == 0);
        utassert(str::Eq(s, L"lola\r\t"));

        free(s);
    }

    {
        AutoFree tmp = strconv::ToMultiByte("abc", 9876, 123456);
        utassert(!tmp.get());
    }
    {
        AutoFree tmp = strconv::WstrToCodePage(L"abc", 98765);
        utassert(!tmp.Get());
    }
    {
        AutoFreeWstr tmp(strconv::FromCodePage("abc", 12345));
        utassert(str::IsEmpty(tmp.Get()));
    }
    {
        AutoFree tmp = strconv::WstrToCodePage(L"abc", 987654);
        utassert(str::IsEmpty(tmp.Get()));
    }

    {
        char buf1[6] = {0};
        size_t cnt = str::BufAppend(buf1, dimof(buf1), "");
        utassert(0 == cnt);
        cnt = str::BufAppend(buf1, dimof(buf1), "1234");
        utassert(4 == cnt);
        utassert(str::Eq("1234", buf1));
        cnt = str::BufAppend(buf1, dimof(buf1), "56");
        utassert(1 == cnt);
        utassert(str::Eq("12345", buf1));
        cnt = str::BufAppend(buf1, dimof(buf1), "6");
        utassert(0 == cnt);
        utassert(str::Eq("12345", buf1));
    }

    {
        WCHAR buf1[6] = {0};
        size_t cnt = str::BufAppend(buf1, dimof(buf1), L"");
        utassert(0 == cnt);
        cnt = str::BufAppend(buf1, dimof(buf1), L"1234");
        utassert(4 == cnt);
        utassert(str::Eq(L"1234", buf1));
        cnt = str::BufAppend(buf1, dimof(buf1), L"56");
        utassert(1 == cnt);
        utassert(str::Eq(L"12345", buf1));
        cnt = str::BufAppend(buf1, dimof(buf1), L"6");
        utassert(0 == cnt);
        utassert(str::Eq(L"12345", buf1));
    }

    {
        for (int c = 0x00; c < 0x100; c++) {
            utassert(!!isspace((unsigned char)c) == str::IsWs((char)c));
        }
        for (int c = 0x00; c < 0x10000; c++) {
            utassert(!!iswspace((WCHAR)c) == str::IsWs((WCHAR)c));
        }
    }

    {
        utassert(str::Eq(str::FindI(L"test", nullptr), nullptr));
        utassert(str::Eq(str::FindI(nullptr, L"test"), nullptr));
        utassert(str::Eq(str::FindI(L"test", L""), L"test"));
        utassert(str::Eq(str::FindI(L"test", L"ES"), L"est"));
        utassert(str::Eq(str::FindI(L"test", L"Te"), L"test"));
        utassert(str::Eq(str::FindI(L"testx", L"X"), L"x"));
        utassert(str::Eq(str::FindI(L"test", L"st"), L"st"));
        utassert(str::Eq(str::FindI(L"t\xE4st", L"\xC4s"), nullptr));
        utassert(str::Eq(str::FindI(L"t\xE4st", L"T\xC5"), nullptr));

        utassert(str::Eq(str::FindI("test", nullptr), nullptr));
        utassert(str::Eq(str::FindI(nullptr, "test"), nullptr));
        utassert(str::Eq(str::FindI("test", ""), "test"));
        utassert(str::Eq(str::FindI("test", "ES"), "est"));
        utassert(str::Eq(str::FindI("test", "Te"), "test"));
        utassert(str::Eq(str::FindI("testx", "X"), "x"));
        utassert(str::Eq(str::FindI("test", "st"), "st"));
    }

    StrIsDigitTest();
    StrReplaceTest();
    StrSeqTest();
    StrConvTest();
    StrUrlExtractTest();
    ParseUntilTest();
}
