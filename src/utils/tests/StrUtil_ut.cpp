/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

static void StrReplaceTestOne(const char* s, const char* toReplace, const char* replaceWith, const char* expected) {
    TempStr res = str::ReplaceTemp(s, toReplace, replaceWith);
    utassert(str::Eq(res, expected));
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
        const char *string, *find, *replace, *result;
    } data[] = {
        {"golagon", "gon", "rabato", "golarabato"},
        {"a", "a", "bor", "bor"},
        {"abora", "a", "", "bor"},
        {"aaaaaa", "a", "b", "bbbbbb"},
        {"aba", "a", "ccc", "cccbccc"},
        {"Aba", "a", "c", "Abc"},
        {"abc", "abc", "", ""},
        {nullptr, "a", "b", nullptr},
        {"a", "", "b", nullptr},
        {"a", "b", nullptr, nullptr},
    };
    for (size_t i = 0; i < dimof(data); i++) {
        TempStr result = str::ReplaceTemp(data[i].string, data[i].find, data[i].replace);
        utassert(str::Eq(result, data[i].result));
    }
}

static void StrSeqTest() {
    const char* s = "foo\0a\0bar\0";
    utassert(0 == seqstrings::StrToIdx(s, "foo"));
    utassert(1 == seqstrings::StrToIdx(s, "a"));
    utassert(2 == seqstrings::StrToIdx(s, "bar"));

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
#if 0
    WCHAR wbuf[4];
    char cbuf[4];
    size_t conv = strconv::Utf8ToWcharBuf("testing", 4, wbuf, dimof(wbuf));
    utassert(conv == 3 && str::Eq(wbuf, L"tes"));
    conv = strconv::WStrToUtf8Buf(L"abc", cbuf, dimof(cbuf));
    utassert(conv == 3 && str::Eq(cbuf, "abc"));
    conv = strconv::Utf8ToWcharBuf("ab\xF0\x90\x82\x80", 6, wbuf, dimof(wbuf));
    utassert(conv == 3 && str::StartsWith(wbuf, L"ab") && wbuf[2] == 0xD800);
    conv = strconv::Utf8ToWcharBuf("ab\xF0\x90\x82\x80", 6, wbuf, dimof(wbuf) - 1);
    utassert(conv == 1 && str::Eq(wbuf, L"a"));
    conv = strconv::WStrToUtf8Buf(L"ab\u20AC", cbuf, dimof(cbuf));
    utassert(conv == 0 && str::Eq(cbuf, ""));
    conv = strconv::WStrToUtf8Buf(L"abcd", cbuf, dimof(cbuf));
    utassert(conv == 0 && str::Eq(cbuf, ""));
#endif
}

static void StrUrlExtractTest() {
    utassert(!url::GetFileNameTemp(""));
    utassert(!url::GetFileNameTemp("#hash_only"));
    utassert(!url::GetFileNameTemp("?query=only"));
    TempStr fileName = url::GetFileNameTemp("http://example.net/filename.ext");
    utassert(str::Eq(fileName, "filename.ext"));
    fileName = url::GetFileNameTemp("http://example.net/filename.ext#with_hash");
    utassert(str::Eq(fileName, "filename.ext"));
    fileName = url::GetFileNameTemp("http://example.net/path/to/filename.ext?more=data");
    utassert(str::Eq(fileName, "filename.ext"));
    fileName = url::GetFileNameTemp("http://example.net/pa%74h/na%2f%6d%65%2ee%78t");
    utassert(str::Eq(fileName, "na/me.ext"));
    fileName = url::GetFileNameTemp("http://example.net/%E2%82%AC");
    utassert(str::Eq(fileName, "\xE2\x82\xaC"));
}

void strStrTest() {
    {
        // verify that we use buf for initial allocations
        str::Str str;
        char* buf = str.Get();
        str.Append("blah");
        utassert(str.Contains("blah"));
        utassert(str.Contains("ah"));
        utassert(str.Contains("h"));
        utassert(!str.Contains("lahd"));
        utassert(!str.Contains("blahd"));
        utassert(!str.Contains("blas"));

        char* buf2 = str.Get();
        utassert(buf == buf2);
        utassert(str::Eq(buf2, "blah"));
        str.Append("lost");
        buf2 = str.Get();
        utassert(str::Eq(buf2, "blahlost"));
        utassert(str.Contains("blahlost"));
        utassert(str.Contains("ahlo"));
        utassert(buf == buf2);
        str.Reset();
        for (int i = 0; i < str::Str::kBufChars + 4; i++) {
            str.AppendChar((char)i);
        }
        buf2 = str.Get();
        // we should have allocated buf on the heap
        utassert(buf != buf2);
        for (int i = 0; i < str::Str::kBufChars + 4; i++) {
            char c = str.at(i);
            utassert(c == (char)i);
        }
    }

    {
        // verify that initialCapacity hint works
        str::Str str(1024);
        char* buf = nullptr;

        for (int i = 0; i < 50; i++) {
            str.Append("01234567890123456789");
            if (i == 2) {
                // we filled Str::buf (32 bytes) by putting 20 bytes
                // and allocated heap for 1024 bytes. Remember the
                buf = str.Get();
            }
        }
        // we've appended 100*10 = 1000 chars, which is less than 1024
        // so Str::buf should be the same as buf
        char* buf2 = str.Get();
        utassert(buf == buf2);
    }
}

void StrTest() {
    char buf[32];
    const char* str = "a string";
    utassert(str::Len(str) == 8);
    utassert(str::Eq(str, "a string") && str::Eq(str, str));
    utassert(!str::Eq(str, nullptr) && !str::Eq(str, "A String"));
    utassert(str::EqI(str, "A String") && str::EqI(str, str));
    utassert(!str::EqI(str, nullptr) && str::EqI((char*)nullptr, (char*)nullptr));
    utassert(str::EqN("abcd", "abce", 3) && !str::EqN("abcd", "Abcd", 3));
    utassert(str::StartsWith(str, "a s") && str::StartsWithI(str, "A Str"));
    utassert(!str::StartsWith(str, "Astr"));
    utassert(str::EndsWith(str, "ing") && str::EndsWithI(str, "ING"));
    utassert(!str::EndsWith(str, "ung"));
    utassert(str::IsEmpty((char*)nullptr) && str::IsEmpty((char*)nullptr) && str::IsEmpty("") && !str::IsEmpty(str));
    utassert(str::FindChar(str, 's') && !str::FindChar(str, 'S'));
    size_t len = str::BufSet(buf, dimof(buf), str);
    utassert(len == str::Len(buf) && str::Eq(buf, str));
    len = str::BufSet(buf, 6, str);
    utassert(len == 5 && str::Eq(buf, "a str"));

    str = str::Dup(buf);
    utassert(str::Eq(str, buf));
    str::Free(str);
    str = str::Dup(buf, 4);
    utassert(str::Eq(str, "a st"));
    str::Free(str);
    str = str::Format("%s", buf);
    utassert(str::Eq(str, buf));
    str::Free(str);
    {
        char* str2;
        AutoFreeStr large(AllocArray<char>(2000));
        memset(large, 0x11, 1998);
        str2 = str::Format("%s", large.Get());
        utassert(str::Eq(str2, large));
        str::Free(str2);
    }
#if 0
    // TODO: this test slows down DEBUG builds significantly
    str = str::Format("%s", "\uFFFF");
    // TODO: in VS2015, str matches "\uFFFF" instead of nullptr
    utassert(str::Eq(str, nullptr));
    free(str);
#endif
    str = str::Join(buf, buf);
    utassert(str::Len(str) == 2 * str::Len(buf));
    str::Free(str);
    str = str::Join(nullptr, "ab");
    utassert(str::Eq(str, "ab"));
    str::Free(str);

#if 0
    str = str::Join("\uFDEF", "\uFFFF");
    utassert(str::Eq(str, "\uFDEF\uFFFF"));
    str::Free(str);
#endif

    str::BufSet(buf, dimof(buf), "abc\1efg\1");
    size_t count = str::TransCharsInPlace(buf, "ace", "ACE");
    utassert(str::Eq(buf, "AbC\1Efg\1") && count == 3);
    count = str::TransCharsInPlace(buf, "\1", "\0");
    utassert(count == 2);
    utassert(str::Eq(buf, "AbC") && str::Eq(buf + 4, "Efg") && count == 2);
    count = str::TransCharsInPlace(buf, "", "X");
    utassert(str::Eq(buf, "AbC") && count == 0);

    str::BufSet(buf, dimof(buf), "blogarapato");
    count = str::RemoveCharsInPlace(buf, "bo");
    utassert(3 == count);
    utassert(str::Eq(buf, "lgarapat"));

    str::BufSet(buf, dimof(buf), "one\r\ntwo\t\v\f\tthree");
    count = str::NormalizeWSInPlace(buf);
    utassert(4 == count);
    utassert(str::Eq(buf, "one two three"));

    str::BufSet(buf, dimof(buf), " one    two three ");
    count = str::NormalizeWSInPlace(buf);
    utassert(5 == count);
    utassert(str::Eq(buf, "one two three"));

    count = str::NormalizeWSInPlace(buf);
    utassert(0 == count);
    utassert(str::Eq(buf, "one two three"));

    {
        const char* str2 = "[Open(\"filename.pdf\",0,1,0)]";
        {
            uint u1 = 0;
            char* str1 = nullptr;
            const char* end = str::Parse(str2, "[Open(\"%s\",%? 0,%u,0)]", &str1, &u1);
            utassert(end && !*end);
            utassert(u1 == 1 && str::Eq(str1, "filename.pdf"));
            free(str1);
        }

        {
            uint u1 = 0;
            AutoFreeStr str1;
            const char* end = str::Parse(str2, "[Open(\"%S\",0%?,%u,0)]", &str1, &u1);
            utassert(end && !*end);
            utassert(u1 == 1 && str::Eq(str1, "filename.pdf"));

            utassert(str::Parse("0xABCD", "%x", &u1));
            utassert(u1 == 0xABCD);
            utassert(str::Parse("ABCD", "%2x%S", &u1, &str1));
            utassert(u1 == 0xAB && str::Eq(str1, "CD"));
        }
    }
    {
        int i1, i2;
        const char* end = str::Parse("1, 2+3", "%d,%d", &i1, &i2);
        utassert(end && str::Eq(end, "+3"));
        utassert(i1 == 1 && i2 == 2);
        end = str::Parse(end, "+3");
        utassert(end && !*end);

        utassert(str::Parse(" -2", "%d", &i1));
        utassert(i1 == -2);
        utassert(str::Parse(" 2", " %u", &i1));
        utassert(i1 == 2);
        utassert(str::Parse("123-456", "%3d%3d6", &i1, &i2));
        utassert(i1 == 123 && i2 == -45);
        utassert(!str::Parse("123", "%4d", &i1));
        utassert(str::Parse("654", "%3d", &i1));
        utassert(i1 == 654);
    }

    utassert(str::Parse("abc", "abc%$"));
    utassert(str::Parse("abc", "a%?bc%?d%$"));
    utassert(!str::Parse("abc", "ab%$"));
    utassert(str::Parse("a \r\n\t b", "a%_b"));
    utassert(str::Parse("ab", "a%_b"));
    utassert(!str::Parse("a,b", "a%_b"));
    utassert(str::Parse("a\tb", "a% b"));
    utassert(!str::Parse("a\r\nb", "a% b"));
    utassert(str::Parse("a\r\nb", "a% %_b"));
    utassert(!str::Parse("ab", "a% b"));
    utassert(!str::Parse("%+", "+") && !str::Parse("%+", "%+"));

    utassert(str::Parse("abcd", 3, "abc%$"));
    utassert(str::Parse("abc", 3, "a%?bc%?d%$"));
    utassert(!str::Parse("abcd", 3, "abcd"));

    {
        const char* str1 = "string";
        utassert(str::Parse(str1, 4, "str") == str1 + 3);

        float f1, f2;
        const char* end = str::Parse("%1.23y -2e-3z", "%%%fy%fz%$", &f1, &f2);
        utassert(end && !*end);
        utassert(f1 == 1.23f && f2 == -2e-3f);
        f1 = 0;
        f2 = 0;
        const char* end2 = str::Parse("%1.23y -2e-3zlah", 13, "%%%fy%fz%$", &f1, &f2);
        utassert(end2 && str::Eq(end2, "lah"));
        utassert(f1 == 1.23f && f2 == -2e-3f);
    }

    {
        char* str1 = nullptr;
        char c1;
        utassert(!str::Parse("no exclamation mark?", "%s!", &str1));
        utassert(!str1);
        utassert(str::Parse("xyz", "x%cz", &c1));
        utassert(c1 == 'y');
        utassert(!str::Parse("leaks memory!?", "%s!%$", &str1));
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
        AutoFreeStr str1;
        int i, j;
        float f;
        utassert(str::Parse("wide string, -30-20 1.5%", "%S,%d%?-%2u%f%%%$", &str1, &i, &j, &f));
        utassert(str::Eq(str1, "wide string") && i == -30 && j == 20 && f == 1.5f);
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

    {
// the test string should only contain ASCII characters,
// as all others might not be available in all code pages
#define TEST_STRING "aBc"
        AutoFree strA = strconv::WStrToAnsi(TEXT(TEST_STRING));
        utassert(str::Eq(strA.Get(), TEST_STRING));
        auto res = strconv::AnsiToWStrTemp(strA.Get());
        utassert(str::Eq(res, TEXT(TEST_STRING)));
#undef TEST_STRING
    }

    utassert(str::IsDigit('0') && str::IsDigit(TEXT('5')) && str::IsDigit(L'9'));
    utassert(iswdigit(L'\u0660') && !str::IsDigit(L'\xB2'));

    utassert(str::CmpNatural(".hg", "2.pdf") < 0);
    utassert(str::CmpNatural("100.pdf", "2.pdf") > 0);
    utassert(str::CmpNatural("2.pdf", "zzz") < 0);
    utassert(str::CmpNatural("abc", ".svn") > 0);
    utassert(str::CmpNatural("ab0200", "AB333") < 0);
    utassert(str::CmpNatural("a b", "a  c") < 0);

#ifndef LOCALE_INVARIANT
#define LOCALE_INVARIANT (MAKELCID(MAKELANGID(LANG_INVARIANT, SUBLANG_NEUTRAL), SORT_DEFAULT))
#endif

    // clang-format off
    struct {
        size_t number;
        const char* result;
    } formatNumData[] = {
        {1, "1"},
        {12, "12"},
        {123, "123"},
        {1234, "1,234"},
        {12345, "12,345"},
        {123456, "123,456"},
        {1234567, "1,234,567"},
        {12345678, "12,345,678"},
    };
    // clang-format on

    for (int i = 0; i < dimof(formatNumData); i++) {
        char* tmp = str::FormatNumWithThousandSepTemp(formatNumData[i].number, LOCALE_INVARIANT);
        utassert(str::Eq(tmp, formatNumData[i].result));
    }

    // clang-format off
    struct {
        double number;
        const char* result;
    } formatFloatData[] = {
        {1, "1.0"},
        {1.2, "1.2"},
        {1.23, "1.23"},
        {1.234, "1.23"},
        {12.345, "12.35"},
        {123.456, "123.46"},
        {1234.5678, "1,234.57"},
    };
    // clang-format on

    for (int i = 0; i < dimof(formatFloatData); i++) {
        char* tmp = str::FormatFloatWithThousandSepTemp(formatFloatData[i].number, LOCALE_INVARIANT);
        utassert(str::Eq(tmp, formatFloatData[i].result));
    }

    {
        char str1[] = "aAbBcC... 1-9";
        str::ToLowerInPlace(str1);
        utassert(str::Eq(str1, "aabbcc... 1-9"));
    }

    // clang-format off
    struct {
        int number;
        const char* result;
    } formatRomanData[] = {
        {1, "I"},
        {3, "III"},
        {6, "VI"},
        {14, "XIV"},
        {49, "XLIX"},
        {176, "CLXXVI"},
        {499, "CDXCIX"},
        {1666, "MDCLXVI"},
        {2011, "MMXI"},
        {12345, "MMMMMMMMMMMMCCCXLV"},
        {0, nullptr},
        {-133, nullptr},
    };
    // clang-format on

    for (int i = 0; i < dimof(formatRomanData); i++) {
        TempStr tmp = str::FormatRomanNumeralTemp(formatRomanData[i].number);
        utassert(str::Eq(tmp, formatRomanData[i].result));
    }

    {
        size_t trimmed;
        char* s = str::Dup("");
        trimmed = str::TrimWSInPlace(s, str::TrimOpt::Both);
        utassert(trimmed == 0);
        utassert(str::Eq(s, ""));
        trimmed = str::TrimWSInPlace(s, str::TrimOpt::Right);
        utassert(trimmed == 0);
        utassert(str::Eq(s, ""));
        trimmed = str::TrimWSInPlace(s, str::TrimOpt::Left);
        utassert(trimmed == 0);
        utassert(str::Eq(s, ""));

        free(s);
        s = str::Dup("  \n\t  ");
        trimmed = str::TrimWSInPlace(s, str::TrimOpt::Both);
        utassert(trimmed == 6);
        utassert(str::Eq(s, ""));

        free(s);
        s = str::Dup("  \n\t  ");
        trimmed = str::TrimWSInPlace(s, str::TrimOpt::Right);
        utassert(trimmed == 6);
        utassert(str::Eq(s, ""));

        free(s);
        s = str::Dup("  \n\t  ");
        trimmed = str::TrimWSInPlace(s, str::TrimOpt::Left);
        utassert(trimmed == 6);
        utassert(str::Eq(s, ""));

        free(s);
        s = str::Dup("  lola");
        trimmed = str::TrimWSInPlace(s, str::TrimOpt::Both);
        utassert(trimmed == 2);
        utassert(str::Eq(s, "lola"));

        free(s);
        s = str::Dup("  lola");
        trimmed = str::TrimWSInPlace(s, str::TrimOpt::Left);
        utassert(trimmed == 2);
        utassert(str::Eq(s, "lola"));

        free(s);
        s = str::Dup("  lola");
        trimmed = str::TrimWSInPlace(s, str::TrimOpt::Right);
        utassert(trimmed == 0);
        utassert(str::Eq(s, "  lola"));

        free(s);
        s = str::Dup("lola\r\t");
        trimmed = str::TrimWSInPlace(s, str::TrimOpt::Both);
        utassert(trimmed == 2);
        utassert(str::Eq(s, "lola"));

        free(s);
        s = str::Dup("lola\r\t");
        trimmed = str::TrimWSInPlace(s, str::TrimOpt::Right);
        utassert(trimmed == 2);
        utassert(str::Eq(s, "lola"));

        free(s);
        s = str::Dup("lola\r\t");
        trimmed = str::TrimWSInPlace(s, str::TrimOpt::Left);
        utassert(trimmed == 0);
        utassert(str::Eq(s, "lola\r\t"));

        free(s);
    }

    {
        TempStr tmp = strconv::ToMultiByteTemp("abc", 9876, 123456);
        utassert(!tmp);
    }
    {
        AutoFree tmp = strconv::WStrToCodePage(98765, L"abc");
        utassert(!tmp.Get());
    }
    {
        TempWStr tmp = strconv::StrCPToWStrTemp("abc", 12345);
        utassert(str::IsEmpty(tmp));
    }
    {
        AutoFree tmp = strconv::WStrToCodePage(987654, L"abc");
        utassert(str::IsEmpty(tmp.Get()));
    }

    {
        char buf1[6]{};
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
        for (int c = 0x00; c < 0x100; c++) {
            utassert(!!isspace((u8)c) == str::IsWs((char)c));
        }
        for (int c = 0x00; c < 0x10000; c++) {
            utassert(!!iswspace((WCHAR)c) == str::IsWs((WCHAR)c));
        }
    }

    strStrTest();
    StrIsDigitTest();
    StrReplaceTest();
    StrSeqTest();
    StrConvTest();
    StrUrlExtractTest();
    // ParseUntilTest();
}
