/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

// must be last due to assert() over-write
#include "base/UtAssert.h"

static void StrReplaceTestOne(Str s, Str toReplace, Str replaceWith, Str expected) {
    TempStr res = str::ReplaceTemp(s, toReplace, replaceWith);
    utassert(str::Eq(res, expected));
}

static void StrReplaceTest() {
    Str d[] = {
        "golagon", "gon", "rabato", "golarabato", "a",   "a",      "bor", "bor", "abora", "a",
        "",        "bor", "aaaaaa", "a",          "b",   "bbbbbb", "aba", "a",   "ccc",   "cccbccc",
        "Aba",     "a",   "c",      "Abc",        "abc", "abc",    "",    "",    {},      "a",
        "b",       {},    "a",      "",           "b",   {},       "a",   "b",   {},      {},
    };
    size_t n = dimof(d) / 4;
    for (size_t i = 0; i < n; i++) {
        StrReplaceTestOne(d[i * 4], d[i * 4 + 1], d[i * 4 + 2], d[i * 4 + 3]);
    }

    struct {
        Str string, find, replace, result;
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

static void StrSeqNumTest() {
    str::Builder b;
    SeqStrNumAppend(&b, "foo", 10);
    SeqStrNumAppend(&b, "bar", -3);
    SeqStrNumAppend(&b, "baz", 0x1234);
    SeqStrNumFinish(&b);
    SeqStrNum seq = ToStr(b).s;

    i64 num = 0;
    utassert(0 == SeqStrNumIndex(seq, "foo", &num));
    utassert(num == 10);
    utassert(1 == SeqStrNumIndex(seq, "bar", &num));
    utassert(num == -3);
    utassert(2 == SeqStrNumIndex(seq, "baz", &num));
    utassert(num == 0x1234);
    utassert(-1 == SeqStrNumIndex(seq, "missing", &num));

    Str s = SeqStrNumByIndex(seq, 1, &num);
    utassert(str::Eq(s, "bar"));
    utassert(num == -3);

    s = SeqStrNumStrByNumber(seq, 10);
    utassert(str::Eq(s, "foo"));
    s = SeqStrNumStrByNumber(seq, 0x1234);
    utassert(str::Eq(s, "baz"));
    utassert(!SeqStrNumStrByNumber(seq, 99));

    int off = 0;
    int idx = 0;
    SeqStrNumAdvance(seq, off, &idx);
    utassert(idx == 1);
    utassert(str::Eq(SeqStrNumAt(seq, off), "bar"));
}

static void StrSeqTest() {
    static const char seqData[] = "foo\0a\0bar\0";
    Str s(seqData, (int)(sizeof(seqData) - 1));
    utassert(0 == SeqStrIndex(s.s, "foo"));
    utassert(1 == SeqStrIndex(s.s, "a"));
    utassert(2 == SeqStrIndex(s.s, "bar"));

    utassert(str::Eq("foo", SeqStrByIndex(s.s, 0)));
    utassert(str::Eq("a", SeqStrByIndex(s.s, 1)));
    utassert(str::Eq("bar", SeqStrByIndex(s.s, 2)));

    utassert(0 == SeqStrIndex(s.s, "foo"));
    utassert(1 == SeqStrIndex(s.s, "a"));
    utassert(2 == SeqStrIndex(s.s, "bar"));
    utassert(-1 == SeqStrIndex(s.s, "fo"));
    utassert(-1 == SeqStrIndex(s.s, ""));
    utassert(-1 == SeqStrIndex(s.s, "ab"));
    utassert(-1 == SeqStrIndex(s.s, "baro"));
    utassert(-1 == SeqStrIndex(s.s, "ba"));
}

static void StrIsDigitTest() {
    Str nonDigits = "/:.bz{}";
    Str digits = "0123456789";
    for (int i = 0; i < len(nonDigits); i++) {
#if 0
        if (str::IsDigit(nonDigits[i])) {
            char c = nonDigits[i];
            printf("%c is incorrectly determined as a digit\n", c);
        }
#endif
        utassert(!str::IsDigit(nonDigits.s[i]));
    }
    for (int i = 0; i < len(digits); i++) {
        utassert(str::IsDigit(digits.s[i]));
    }

    WStr nonDigitsW = L"/:.bz{}";
    WStr digitsW = L"0123456789";
    for (int i = 0; i < len(nonDigitsW); i++) {
        utassert(!wstr::IsDigit(nonDigitsW.s[i]));
    }
    for (int i = 0; i < len(digitsW); i++) {
        utassert(wstr::IsDigit(digitsW.s[i]));
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
    char wikiUrl[] =
        "https://ru.wikipedia.org/wiki/"
        "%D0%AD%D0%BD%D0%B5%D1%80%D0%B3%D0%B8%D1%8F_%E2%80%94_%D0%91%D1%83%D1%80%D0%B0%D0%BD";
    url::DecodeInPlace(wikiUrl);
    utassert(str::Eq(wikiUrl,
                     "https://ru.wikipedia.org/wiki/"
                     "\xD0\xAD\xD0\xBD\xD0\xB5\xD1\x80\xD0\xB3\xD0\xB8\xD1\x8F_\xE2\x80\x94_"
                     "\xD0\x91\xD1\x83\xD1\x80\xD0\xB0\xD0\xBD"));
}

void strStrTest() {
    {
        // verify that we use buf for initial allocations
        str::Builder str;
        uintptr_t buf = (uintptr_t)str.begin();
        str.Append("blah");
        utassert(str::Contains(str, StrL("blah")));
        utassert(str::Contains(str, StrL("ah")));
        utassert(str::Contains(str, StrL("h")));
        utassert(!str::Contains(str, StrL("lahd")));
        utassert(!str::Contains(str, StrL("blahd")));
        utassert(!str::Contains(str, StrL("blas")));

        uintptr_t buf2 = (uintptr_t)str.begin();
        utassert(buf == buf2);
        utassert(str::Eq(ToStr(str), "blah"));
        str.Append("lost");
        buf2 = (uintptr_t)str.begin();
        utassert(str::Eq(ToStr(str), "blahlost"));
        utassert(str::Contains(str, StrL("blahlost")));
        utassert(str::Contains(str, StrL("ahlo")));
        utassert(buf == buf2);
        str.Reset();
        for (int i = 0; i < str::Builder::kBufChars + 4; i++) {
            str.AppendChar((char)i);
        }
        buf2 = (uintptr_t)str.begin();
        // we should have allocated buf on the heap
        utassert(buf != buf2);
        for (int i = 0; i < str::Builder::kBufChars + 4; i++) {
            char c = str[i];
            utassert(c == (char)i);
        }
    }

    {
        // verify that initialCapacity hint works
        str::Builder str(1024);
        uintptr_t buf = 0;

        for (int i = 0; i < 50; i++) {
            str.Append("01234567890123456789");
            if (i == 2) {
                // we filled Str::buf (32 bytes) by putting 20 bytes
                // and allocated heap for 1024 bytes. Remember the
                buf = (uintptr_t)str.begin();
            }
        }
        // we've appended 100*10 = 1000 chars, which is less than 1024
        // so Str::buf should be the same as buf
        uintptr_t buf2 = (uintptr_t)str.begin();
        utassert(buf == buf2);
    }
}

// case-insensitive Find/Contains must work for non-Latin scripts, not just
// ASCII (issue #5717: TOC "*" palette search was case-sensitive for Cyrillic)
static void StrFindITest() {
    // ASCII still works (fast path, regression guard)
    Str hello = "Hello World";
    utassert(str::ContainsI(hello, "hello"));
    utassert(str::ContainsI(hello, "WORLD"));
    utassert(!str::ContainsI(hello, "xyz"));
    utassert(str::IndexOfI(hello, "WORLD") == 6);

    // Cyrillic: "Привет" (capitalized) vs "привет" (lowercase needle)
    Str privetCap = "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82";
    Str privetLow = "\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82";
    utassert(str::Contains(privetCap, privetCap));
    utassert(!str::Contains(privetCap, privetLow)); // case-sensitive: no match
    utassert(str::ContainsI(privetCap, privetLow)); // case-insensitive: matches
    utassert(str::ContainsI(privetLow, privetCap)); // and the reverse
    utassert(str::IndexOfI(privetCap, privetLow) == 0);

    // mixed ASCII + Cyrillic: the returned offset must be the correct byte
    // offset into the original UTF-8 string ("abc " is 4 bytes)
    Str mixed = "abc \xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82";
    utassert(str::IndexOfI(mixed, privetLow) == 4);
    utassert(str::IndexOfI(mixed, "xyz") < 0);

    // Greek: "ΛΟΓΟΣ" vs "λογος"
    Str logosCap = "\xCE\x9B\xCE\x9F\xCE\x93\xCE\x9F\xCE\xA3";
    Str logosLow = "\xCE\xBB\xCE\xBF\xCE\xB3\xCE\xBF\xCF\x83";
    utassert(str::ContainsI(logosCap, logosLow));
}

static void StrCutTest() {
    // IndexOfAfter: offset just past the match, or -1
    Str s = "key=value";
    utassert(str::IndexOfAfter(s, "=") == 4);
    utassert(str::IndexOfAfter(s, "key") == 3);
    utassert(str::IndexOfAfter(s, "xyz") == -1);
    utassert(str::IndexOfAfter(s, "value") == 9); // match at end -> len

    // Cut: split around first occurrence
    Str before, after;
    utassert(str::Cut(s, "=", &before, &after));
    utassert(str::Eq(before, "key") && str::Eq(after, "value"));

    // only one side requested
    after = {};
    utassert(str::Cut(s, "=", nullptr, &after) && str::Eq(after, "value"));
    before = {};
    utassert(str::Cut(s, "=", &before, nullptr) && str::Eq(before, "key"));

    // separator not found: returns false, before = whole string, after = {}
    before = {};
    after = "sentinel";
    utassert(!str::Cut(s, "#", &before, &after));
    utassert(str::Eq(before, s) && str::IsEmpty(after));

    // separator at the very end -> after is empty but Cut returns true
    utassert(str::Cut(s, "value", &before, &after));
    utassert(str::Eq(before, "key=") && str::IsEmpty(after));

    // multi-char separator, only first occurrence splits
    utassert(str::Cut("a::b::c", "::", &before, &after));
    utassert(str::Eq(before, "a") && str::Eq(after, "b::c"));
}

static void StrNextLineTest() {
    Str line, rest;

    // LF, CR and CRLF are all single line terminators
    rest = "a\nb\rc\r\nd";
    utassert(str::NextLine(rest, line, rest) && str::Eq(line, "a"));
    utassert(str::NextLine(rest, line, rest) && str::Eq(line, "b"));
    utassert(str::NextLine(rest, line, rest) && str::Eq(line, "c"));
    utassert(str::NextLine(rest, line, rest) && str::Eq(line, "d"));
    utassert(!str::NextLine(rest, line, rest));

    // empty input -> no line
    rest = Str{};
    utassert(!str::NextLine(rest, line, rest));

    // a trailing terminator does not yield an extra empty line
    rest = "a\n";
    utassert(str::NextLine(rest, line, rest) && str::Eq(line, "a"));
    utassert(!str::NextLine(rest, line, rest));

    // empty lines are returned as empty (not skipped)
    rest = "\n\nx";
    utassert(str::NextLine(rest, line, rest) && str::IsEmpty(line));
    utassert(str::NextLine(rest, line, rest) && str::IsEmpty(line));
    utassert(str::NextLine(rest, line, rest) && str::Eq(line, "x"));
    utassert(!str::NextLine(rest, line, rest));

    // final line without a terminator
    rest = "only";
    utassert(str::NextLine(rest, line, rest) && str::Eq(line, "only"));
    utassert(str::IsEmpty(rest));
    utassert(!str::NextLine(rest, line, rest));
}

void StrTest() {
    char buf[32];
    Str str = "a string";
    utassert(str.len == 8);
    utassert(str::Eq(str, "a string") && str::Eq(str, str));
    utassert(!str::Eq(str, Str{}) && !str::Eq(str, "A String"));
    utassert(str::EqI(str, "A String") && str::EqI(str, str));
    utassert(!str::EqI(str, Str{}) && str::EqI(Str{}, Str{}));
    utassert(str::EqI(Str("AbCx", 3), Str("abcY", 3)));
    utassert(!str::EqI(Str("AbCx", 3), Str("abcY", 4)));
    utassert(str::EqN("abcd", "abce", 3) && !str::EqN("abcd", "Abcd", 3));
    utassert(str::StartsWith(str, "a s") && str::StartsWithI(str, "A Str"));
    utassert(!str::StartsWith(str, "Astr"));
    utassert(str::EndsWith(str, "ing") && str::EndsWithI(str, "ING"));
    utassert(!str::EndsWith(str, "ung"));
    utassert(str::IsEmpty(Str{}) && str::IsEmpty("") && !str::IsEmpty(str));
    utassert(str::ContainsChar(str, 's') && !str::ContainsChar(str, 'S'));
    utassert(str::IndexOfChar(str, 's') == 2);
    utassert(str::IndexOfChar(str, 'g') == 7);
    utassert(!str::ContainsChar(str, 'x'));
    utassert(!str::ContainsChar(Str{}, 'a'));
    int n = str::BufSet(Str(buf, dimof(buf)), str);
    utassert(n == len(buf) && str::Eq(buf, str));
    n = str::BufSet(Str(buf, 6), str);
    utassert(n == 5 && str::Eq(buf, "a str"));

    str = str::Dup(buf);
    utassert(str::Eq(str, buf));
    str::Free(str);
    str = str::Dup(Str(buf, 4));
    utassert(str::Eq(str, "a st"));
    str::Free(str);
    str = fmt("%s", Str(buf));
    utassert(str::Eq(str, buf));
    str = fmt("%S", WStrL(L"a"
                          L"\x2019"
                          L"a.pdf"));
    utassert(str::Eq(str,
                     "a\xE2\x80\x99"
                     "a.pdf"));
    {
        Str str2;
        char* large = AllocArrayTemp<char>(2000);
        memset(large, 0x11, 1998);
        str2 = fmt("%s", Str(large));
        utassert(str::Eq(str2, Str(large)));
    }
#if 0
    // TODO: this test slows down DEBUG builds significantly
    str = fmt("%s", StrL("\uFFFF"));
    // TODO: in VS2015, str matches "\uFFFF" instead of nullptr
    utassert(str::Eq(str, nullptr));
#endif
    str = str::Join(buf, buf);
    utassert(len(str) == 2 * len(buf));
    str::Free(str);
    str = str::Join(nullptr, "ab");
    utassert(str::Eq(str, "ab"));
    str::Free(str);

#if 0
    str = str::Join("\uFDEF", "\uFFFF");
    utassert(str::Eq(str, "\uFDEF\uFFFF"));
    str::Free(str);
#endif

    str::BufSet(Str(buf, dimof(buf)), "abc\1efg\1");
    size_t count = str::TransCharsInPlace(Str(buf), StrL("ace"), StrL("ACE"));
    utassert(str::Eq(buf, "AbC\1Efg\1") && count == 3);
    count = str::TransCharsInPlace(Str(buf), StrL("\1"), StrL("\0"));
    utassert(count == 2);
    utassert(str::Eq(buf, "AbC") && str::Eq(buf + 4, "Efg") && count == 2);
    count = str::TransCharsInPlace(Str(buf), StrL(""), StrL("X"));
    utassert(str::Eq(buf, "AbC") && count == 0);

    str::BufSet(Str(buf, dimof(buf)), "blogarapato");
    count = str::RemoveCharsInPlace(buf, "bo");
    utassert(3 == count);
    utassert(str::Eq(buf, "lgarapat"));

    str::BufSet(Str(buf, dimof(buf)), "one\r\ntwo\t\v\f\tthree");
    count = str::NormalizeWSInPlace(Str(buf));
    utassert(4 == count);
    utassert(str::Eq(buf, "one two three"));

    str::BufSet(Str(buf, dimof(buf)), " one    two three ");
    count = str::NormalizeWSInPlace(Str(buf));
    utassert(5 == count);
    utassert(str::Eq(buf, "one two three"));

    count = str::NormalizeWSInPlace(Str(buf));
    utassert(0 == count);
    utassert(str::Eq(buf, "one two three"));

    {
        Str str2 = "[Open(\"filename.pdf\",0,1,0)]";
        {
            uint u1 = 0;
            TempStr str1;
            Str end = str::Parse(str2, "[Open(\"%s\",%? 0,%u,0)]", &str1, &u1);
            utassert(!str::IsNull(end) && !end.s[0]);
            utassert(u1 == 1 && str::Eq(str1, "filename.pdf"));
        }

        {
            uint u1 = 0;
            TempStr str1;
            Str end = str::Parse(str2, "[Open(\"%S\",0%?,%u,0)]", &str1, &u1);
            utassert(!str::IsNull(end) && !end.s[0]);
            utassert(u1 == 1 && str::Eq(str1, "filename.pdf"));

            utassert(str::Parse(StrL("0xABCD"), "%x", &u1).s);
            utassert(u1 == 0xABCD);
            utassert(str::Parse(StrL("ABCD"), "%2x%S", &u1, &str1).s);
            utassert(u1 == 0xAB && str::Eq(str1, "CD"));
        }
    }
    {
        int i1, i2;
        Str end = str::Parse(StrL("1, 2+3"), "%d,%d", &i1, &i2);
        utassert(!str::IsNull(end) && str::Eq(end, "+3"));
        utassert(i1 == 1 && i2 == 2);
        end = str::Parse(end, "+3");
        utassert(!str::IsNull(end) && !end.s[0]);

        utassert(str::Parse(StrL(" -2"), "%d", &i1).s);
        utassert(i1 == -2);
        utassert(str::Parse(StrL(" 2"), " %u", &i1).s);
        utassert(i1 == 2);
        utassert(str::Parse(StrL("123-456"), "%3d%3d6", &i1, &i2).s);
        utassert(i1 == 123 && i2 == -45);
        utassert(!str::Parse(StrL("123"), "%4d", &i1).s);
        utassert(str::Parse(StrL("654"), "%3d", &i1).s);
        utassert(i1 == 654);
    }

    utassert(str::Parse(StrL("abc"), "abc%$").s);
    utassert(str::Parse(StrL("abc"), "a%?bc%?d%$").s);
    utassert(!str::Parse(StrL("abc"), "ab%$").s);
    utassert(str::Parse(StrL("a \r\n\t b"), "a%_b").s);
    utassert(str::Parse(StrL("ab"), "a%_b").s);
    utassert(!str::Parse(StrL("a,b"), "a%_b").s);
    utassert(str::Parse(StrL("a\tb"), "a% b").s);
    utassert(!str::Parse(StrL("a\r\nb"), "a% b").s);
    utassert(str::Parse(StrL("a\r\nb"), "a% %_b").s);
    utassert(!str::Parse(StrL("ab"), "a% b").s);
    utassert(str::IsNull(str::Parse(StrL("%+"), "+")) && str::IsNull(str::Parse(StrL("%+"), "%+")));

    utassert(str::Parse(Str(StrL("abcd").s, 3), "abc%$").s);
    utassert(str::Parse(Str(StrL("abc").s, 3), "a%?bc%?d%$").s);
    utassert(!str::Parse(Str(StrL("abcd").s, 3), "abcd").s);

    {
        Str str1 = "string";
        utassert(str::Parse(Str(str1.s, 4), "str").s == str1.s + 3);

        float f1, f2;
        Str end = str::Parse(StrL("%1.23y -2e-3z"), "%%%fy%fz%$", &f1, &f2);
        utassert(!str::IsNull(end) && !end.s[0]);
        utassert(f1 == 1.23f && f2 == -2e-3f);
        f1 = 0;
        f2 = 0;
        Str end2 = str::Parse(Str(StrL("%1.23y -2e-3zlah").s, 13), "%%%fy%fz%$", &f1, &f2);
        utassert(!str::IsNull(end2) && end2.len == 0);
        utassert(f1 == 1.23f && f2 == -2e-3f);
    }

    {
        TempStr str1;
        char c1;
        utassert(!str::Parse(StrL("no exclamation mark?"), "%s!", &str1).s);
        utassert(!str1);
        utassert(str::Parse(StrL("xyz"), "x%cz", &c1).s);
        utassert(c1 == 'y');
        utassert(!str::Parse(StrL("leaks memory!?"), "%s!%$", &str1).s);
        utassert(str::Eq(str1, "leaks memory"));
    }

    {
        TempStr str1;
        int i, j;
        float f;
        utassert(str::Parse(StrL("ansi string, -30-20 1.5%"), "%S,%d%?-%2u%f%%%$", &str1, &i, &j, &f).s);
        utassert(str::Eq(str1, "ansi string") && i == -30 && j == 20 && f == 1.5f);
    }
    {
        TempStr str1;
        int i, j;
        float f;
        utassert(str::Parse(StrL("wide string, -30-20 1.5%"), "%S,%d%?-%2u%f%%%$", &str1, &i, &j, &f).s);
        utassert(str::Eq(str1, "wide string") && i == -30 && j == 20 && f == 1.5f);
    }

    {
        Str path =
            "M10 80 C 40 10, 65\r\n10,\t95\t80 S 150 150, 180 80\nA 45 45, 0, 1, 0, 125 125\nA 1 2 3\n0\n1\n20  -20";
        float f[6];
        int b[2];
        Str s = str::Parse(path, "M%f%_%f", &f[0], &f[1]);
        utassert(!str::IsNull(s) && f[0] == 10 && f[1] == 80);
        s = str::Parse(Str(s.s + 1), "C%f%_%f,%f%_%f,%f%_%f", &f[0], &f[1], &f[2], &f[3], &f[4], &f[5]);
        utassert(!str::IsNull(s) && f[0] == 40 && f[1] == 10 && f[2] == 65 && f[3] == 10 && f[4] == 95 && f[5] == 80);
        s = str::Parse(Str(s.s + 1), "S%f%_%f,%f%_%f", &f[0], &f[1], &f[2], &f[3], &f[4]);
        utassert(!str::IsNull(s) && f[0] == 150 && f[1] == 150 && f[2] == 180 && f[3] == 80);
        s = str::Parse(Str(s.s + 1), "A%f%_%f%?,%f%?,%d%?,%d%?,%f%_%f", &f[0], &f[1], &f[2], &b[0], &b[1], &f[4],
                       &f[5]);
        utassert(!str::IsNull(s) && f[0] == 45 && f[1] == 45 && f[2] == 0 && b[0] == 1 && b[1] == 0 && f[4] == 125 &&
                 f[5] == 125);
        s = str::Parse(Str(s.s + 1), "A%f%_%f%?,%f%?,%d%?,%d%?,%f%_%f", &f[0], &f[1], &f[2], &b[0], &b[1], &f[4],
                       &f[5]);
        utassert(!str::IsNull(s) && f[0] == 1 && f[1] == 2 && f[2] == 3 && b[0] == 0 && b[1] == 1 && f[4] == 20 &&
                 f[5] == -20);
    }

    {
// the test string should only contain ASCII characters,
// as all others might not be available in all code pages
#define TEST_STRING "aBc"
        char* strA = strconv::WStrToAnsi(TEXT(TEST_STRING)).s;
        defer {
            free(strA);
        };
        utassert(str::Eq(strA, TEST_STRING));
        auto res = strconv::AnsiToWStrTemp(Str(strA));
        utassert(wstr::Eq(res, TEXT(TEST_STRING)));
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
        Str result;
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
        TempStr tmp = str::FormatNumWithThousandSepTemp(formatNumData[i].number, LOCALE_INVARIANT);
        utassert(str::Eq(tmp, formatNumData[i].result));
    }

    // clang-format off
    struct {
        double number;
        Str result;
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
        TempStr tmp = str::FormatFloatWithThousandSepTemp(formatFloatData[i].number, LOCALE_INVARIANT);
        utassert(str::Eq(tmp, formatFloatData[i].result));
    }

    {
        char str1[] = "aAbBcC... 1-9";
        str::ToLowerInPlace(Str(str1));
        utassert(str::Eq(str1, "aabbcc... 1-9"));
    }

    // clang-format off
    struct {
        int number;
        Str result;
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
        {0, {}},
        {-133, {}},
    };
    // clang-format on

    for (int i = 0; i < dimof(formatRomanData); i++) {
        TempStr tmp = str::FormatRomanNumeralTemp(formatRomanData[i].number);
        utassert(str::Eq(tmp, formatRomanData[i].result));
    }

    {
        size_t trimmed;
        Str s = str::Dup(StrL(""));
        trimmed = str::TrimWSInPlace(Str(s), str::TrimOpt::Both);
        utassert(trimmed == 0);
        utassert(str::Eq(s, ""));
        trimmed = str::TrimWSInPlace(Str(s), str::TrimOpt::Right);
        utassert(trimmed == 0);
        utassert(str::Eq(s, ""));
        trimmed = str::TrimWSInPlace(Str(s), str::TrimOpt::Left);
        utassert(trimmed == 0);
        utassert(str::Eq(s, ""));

        str::ReplaceWithCopy(&s, "  \n\t  ");
        trimmed = str::TrimWSInPlace(s, str::TrimOpt::Both);
        utassert(trimmed == 6);
        utassert(str::Eq(s, ""));

        str::ReplaceWithCopy(&s, "  \n\t  ");
        trimmed = str::TrimWSInPlace(s, str::TrimOpt::Right);
        utassert(trimmed == 6);
        utassert(str::Eq(s, ""));

        str::ReplaceWithCopy(&s, "  \n\t  ");
        trimmed = str::TrimWSInPlace(Str(s), str::TrimOpt::Left);
        utassert(trimmed == 6);
        utassert(str::Eq(s, ""));

        str::ReplaceWithCopy(&s, "  lola");
        trimmed = str::TrimWSInPlace(Str(s), str::TrimOpt::Both);
        utassert(trimmed == 2);
        utassert(str::Eq(s, "lola"));

        str::ReplaceWithCopy(&s, "  lola");
        trimmed = str::TrimWSInPlace(Str(s), str::TrimOpt::Left);
        utassert(trimmed == 2);
        utassert(str::Eq(s, "lola"));

        str::ReplaceWithCopy(&s, "  lola");
        trimmed = str::TrimWSInPlace(Str(s), str::TrimOpt::Right);
        utassert(trimmed == 0);
        utassert(str::Eq(s, "  lola"));

        str::ReplaceWithCopy(&s, "lola\r\t");
        trimmed = str::TrimWSInPlace(Str(s), str::TrimOpt::Both);
        utassert(trimmed == 2);
        utassert(str::Eq(s, "lola"));

        str::ReplaceWithCopy(&s, "lola\r\t");
        trimmed = str::TrimWSInPlace(Str(s), str::TrimOpt::Right);
        utassert(trimmed == 2);
        utassert(str::Eq(s, "lola"));

        str::ReplaceWithCopy(&s, "lola\r\t");
        trimmed = str::TrimWSInPlace(Str(s), str::TrimOpt::Left);
        utassert(trimmed == 0);
        utassert(str::Eq(s, "lola\r\t"));

        str::Free(s);
    }

    {
        TempStr tmp = strconv::ToMultiByteTemp("abc", 9876, 123456);
        utassert(!tmp);
    }
    {
        Str tmp = strconv::WStrToCodePage(98765, L"abc");
        utassert(!tmp);
    }
    {
        TempWStr tmp = strconv::StrCPToWStrTemp("abc", 12345);
        utassert(len(tmp) == 0);
    }
    {
        Str tmp = strconv::WStrToCodePage(987654, L"abc");
        utassert(str::IsEmpty(tmp));
    }

    {
        char buf1[6]{};
        size_t cnt = str::BufAppend(Str(buf1, dimof(buf1)), "");
        utassert(0 == cnt);
        cnt = str::BufAppend(Str(buf1, dimof(buf1)), "1234");
        utassert(4 == cnt);
        utassert(str::Eq("1234", buf1));
        cnt = str::BufAppend(Str(buf1, dimof(buf1)), "56");
        utassert(1 == cnt);
        utassert(str::Eq("12345", buf1));
        cnt = str::BufAppend(Str(buf1, dimof(buf1)), "6");
        utassert(0 == cnt);
        utassert(str::Eq("12345", buf1));
    }

    {
        for (int c = 0x00; c < 0x100; c++) {
            utassert(!!isspace((u8)c) == str::IsWs((char)c));
        }
        for (int c = 0x00; c < 0x10000; c++) {
            utassert(!!iswspace((WCHAR)c) == wstr::IsWs((WCHAR)c));
        }
    }

    strStrTest();
    StrIsDigitTest();
    StrReplaceTest();
    StrSeqTest();
    StrSeqNumTest();
    StrConvTest();
    StrUrlExtractTest();
    StrFindITest();
    StrCutTest();
    StrNextLineTest();
    // ParseUntilTest();
}
