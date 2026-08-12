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
        StrReplaceTestOne(d[i * 4], d[(i * 4) + 1], d[(i * 4) + 2], d[(i * 4) + 3]);
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
    utassert(str::Eq(s, StrL("bar")));
    utassert(num == -3);

    s = SeqStrNumStrByNumber(seq, 10);
    utassert(str::Eq(s, StrL("foo")));
    s = SeqStrNumStrByNumber(seq, 0x1234);
    utassert(str::Eq(s, StrL("baz")));
    utassert(!SeqStrNumStrByNumber(seq, 99));

    int off = 0;
    int idx = 0;
    SeqStrNumAdvance(seq, off, &idx);
    utassert(idx == 1);
    utassert(str::Eq(SeqStrNumAt(seq, off), StrL("bar")));
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
    utassert(conv == 3 && str::Eq(cbuf, StrL("abc")));
    conv = strconv::Utf8ToWcharBuf("ab\xF0\x90\x82\x80", 6, wbuf, dimof(wbuf));
    utassert(conv == 3 && str::StartsWith(wbuf, L"ab") && wbuf[2] == 0xD800);
    conv = strconv::Utf8ToWcharBuf("ab\xF0\x90\x82\x80", 6, wbuf, dimof(wbuf) - 1);
    utassert(conv == 1 && str::Eq(wbuf, L"a"));
    conv = strconv::WStrToUtf8Buf(L"ab\u20AC", cbuf, dimof(cbuf));
    utassert(conv == 0 && str::Eq(cbuf, StrL("")));
    conv = strconv::WStrToUtf8Buf(L"abcd", cbuf, dimof(cbuf));
    utassert(conv == 0 && str::Eq(cbuf, StrL("")));
#endif
}

static void StrUrlExtractTest() {
    utassert(!url::GetFileNameTemp(""));
    utassert(!url::GetFileNameTemp("#hash_only"));
    utassert(!url::GetFileNameTemp("?query=only"));
    TempStr fileName = url::GetFileNameTemp("http://example.net/filename.ext");
    utassert(str::Eq(fileName, StrL("filename.ext")));
    fileName = url::GetFileNameTemp("http://example.net/filename.ext#with_hash");
    utassert(str::Eq(fileName, StrL("filename.ext")));
    fileName = url::GetFileNameTemp("http://example.net/path/to/filename.ext?more=data");
    utassert(str::Eq(fileName, StrL("filename.ext")));
    fileName = url::GetFileNameTemp("http://example.net/pa%74h/na%2f%6d%65%2ee%78t");
    utassert(str::Eq(fileName, StrL("na/me.ext")));
    fileName = url::GetFileNameTemp("http://example.net/%E2%82%AC");
    utassert(str::Eq(fileName, StrL("\xE2\x82\xaC")));
    TempStr wiki = url::DecodeTemp(
        "https://ru.wikipedia.org/wiki/"
        "%D0%AD%D0%BD%D0%B5%D1%80%D0%B3%D0%B8%D1%8F_%E2%80%94_%D0%91%D1%83%D1%80%D0%B0%D0%BD");
    utassert(str::Eq(wiki, StrL("https://ru.wikipedia.org/wiki/"
                                "\xD0\xAD\xD0\xBD\xD0\xB5\xD1\x80\xD0\xB3\xD0\xB8\xD1\x8F_\xE2\x80\x94_"
                                "\xD0\x91\xD1\x83\xD1\x80\xD0\xB0\xD0\xBD")));
    utassert(!url::DecodeTemp({}));
    utassert(str::Eq(url::DecodeTemp("nothing to decode"), StrL("nothing to decode")));
    // a stray or truncated escape is left alone
    utassert(str::Eq(url::DecodeTemp("100%"), StrL("100%")));
    utassert(str::Eq(url::DecodeTemp("%zz%41"), StrL("%zzA")));

    // decoding shrinks the string, so the result's len must match its bytes, or
    // the bytes past the NUL travel with it into anything that copies by len
    // (issue #5926). str::Eq stops at the NUL and can't see that, so compare
    // the lengths directly.
    TempStr decoded = url::DecodeTemp("umlaut_test_%C3%A4.html");
    utassert(len(decoded) == LenL("umlaut_test_\xC3\xA4.html"));
    TempStr joined = str::JoinTemp(StrL("dir\\"), decoded);
    utassert(str::Eq(joined, StrL("dir\\umlaut_test_\xC3\xA4.html")));
    utassert(len(joined) == LenL("dir\\umlaut_test_\xC3\xA4.html"));
    utassert(len(url::GetFullPathTemp("na%2Fme.ext?q=1")) == LenL("na/me.ext"));
    utassert(len(url::GetFileNameTemp("http://example.net/na%2Fme.ext")) == LenL("na/me.ext"));
}

// Run fn once with no external buf, once with a stack buf of random size 1..128.
static void StrBuilderRunTwice(void (*fn)(str::Builder&)) {
    {
        str::Builder b;
        fn(b);
    }
    {
        char stack[128];
        int n = 1 + (rand() % 128); // 1..128
        str::Builder b(Str(stack, n));
        fn(b);
    }
}

static void StrBuilderContainsAppend(str::Builder& str) {
    utassert(str.IsEmpty());
    str.Append("blah");
    utassert(str.begin() != nullptr);
    utassert(str::Contains(str, StrL("blah")));
    utassert(str::Contains(str, StrL("ah")));
    utassert(str::Contains(str, StrL("h")));
    utassert(!str::Contains(str, StrL("lahd")));
    utassert(!str::Contains(str, StrL("blahd")));
    utassert(!str::Contains(str, StrL("blas")));
    utassert(str::Eq(ToStr(str), StrL("blah")));
    str.Append("lost");
    utassert(str::Eq(ToStr(str), StrL("blahlost")));
    utassert(str::Contains(str, StrL("blahlost")));
    utassert(str::Contains(str, StrL("ahlo")));
}

static void StrBuilderGrowPastExternal(str::Builder& str) {
    str.Append("blah");
    utassert(str::Eq(ToStr(str), StrL("blah")));
    str.Append("lost");
    utassert(str::Eq(ToStr(str), StrL("blahlost")));
    str.Reset();
    // 200 chars always exceeds external scratch of at most 128
    for (int i = 0; i < 200; i++) {
        str.AppendChar((char)i);
    }
    utassert(!str.UsesExternalBuf());
    for (int i = 0; i < 200; i++) {
        utassert(str[i] == (char)i);
    }
}

static void StrBuilderRemoveAtStaysOnHeap(str::Builder& str) {
    // Grow past any external buf (max 128) so storage is on the heap.
    for (int i = 0; i < 200; i++) {
        str.AppendChar((char)('a' + (i % 26)));
    }
    uintptr_t heap = (uintptr_t)str.begin();
    utassert(!str.UsesExternalBuf());
    // RemoveAt shrinks len; further appends must not switch back to external
    // (that would lose data and leak the heap allocation).
    str.RemoveAt(0, 190);
    utassert(len(str) == 10);
    utassert((uintptr_t)str.begin() == heap);
    str.Append("xyz");
    utassert((uintptr_t)str.begin() == heap);
    // last 10 of 200 chars (i=190..199): i%26 => 8..17 => "ijklmnopqr"
    utassert(str::Eq(ToStr(str), StrL("ijklmnopqrxyz")));
}

static void StrBuilderManyAppends(str::Builder& str) {
    for (int i = 0; i < 50; i++) {
        str.Append("01234567890123456789");
    }
    utassert(len(str) == 1000);
    utassert(str::StartsWith(ToStr(str), StrL("01234567890123456789")));
    utassert(str::EndsWith(ToStr(str), StrL("01234567890123456789")));
}

static void StrBuilderTakeStr(str::Builder& str) {
    str.Append("hello");
    char* before = str.begin();
    bool wasExternal = str.UsesExternalBuf();
    Str taken = str.TakeStr();
    utassert(str::Eq(taken, StrL("hello")));
    if (wasExternal) {
        // data was in the external buffer; TakeStr must copy
        utassert(taken.s != before);
    }
    str::Free(taken);
    utassert(str.IsEmpty());
}

// capHint is independent of external buf: large hint should avoid realloc while
// content stays under the hint (and ignore a small external scratch once heap
// is allocated with that hint).
static void StrBuilderCapHint() {
    str::Builder str(1024);
    uintptr_t heap = 0;
    for (int i = 0; i < 50; i++) {
        str.Append("01234567890123456789");
        if (i == 0) {
            heap = (uintptr_t)str.begin();
            utassert(heap != 0);
        }
    }
    // 50*20 = 1000 chars < 1024, so no further realloc
    utassert((uintptr_t)str.begin() == heap);
    utassert(str.nReallocs == 1);

    // same with a small external buf: once content needs heap, capHint applies
    // (set .cap after construct — preferred capacity while still on external storage)
    char stack[16];
    str::Builder str2(Str(stack, sizeofi(stack)));
    str2.cap = 1024 + 1; // +1 NUL padding, same as Builder(1024)
    heap = 0;
    int reallocsAtHeap = -1;
    for (int i = 0; i < 50; i++) {
        str2.Append("01234567890123456789");
        if (!str2.UsesExternalBuf() && reallocsAtHeap < 0) {
            heap = (uintptr_t)str2.begin();
            reallocsAtHeap = str2.nReallocs;
        }
    }
    utassert(heap != 0);
    utassert((uintptr_t)str2.begin() == heap);
    // only the grow-from-external realloc, no further ones for 1000 chars
    utassert(str2.nReallocs == reallocsAtHeap);
}

void strStrTest() {
    StrBuilderRunTwice(StrBuilderContainsAppend);
    StrBuilderRunTwice(StrBuilderGrowPastExternal);
    StrBuilderRunTwice(StrBuilderRemoveAtStaysOnHeap);
    StrBuilderRunTwice(StrBuilderManyAppends);
    StrBuilderRunTwice(StrBuilderTakeStr);
    StrBuilderCapHint();
}

// --- wstr::Builder (same external-buf contract as str::Builder) ---

static void WStrBuilderRunTwice(void (*fn)(wstr::Builder&)) {
    {
        wstr::Builder b;
        fn(b);
    }
    {
        WCHAR stack[128];
        int n = 1 + (rand() % 128); // 1..128
        wstr::Builder b(WStr(stack, n));
        fn(b);
    }
}

static void WStrBuilderContainsAppend(wstr::Builder& str) {
    utassert(str.IsEmpty());
    str.Append(L"blah");
    utassert(str.begin() != nullptr);
    utassert(wstr::Eq(ToWStr(str), WStrL(L"blah")));
    str.Append(L"lost");
    utassert(wstr::Eq(ToWStr(str), WStrL(L"blahlost")));
    utassert(wstr::ContainsChar(str, L'a'));
    utassert(!wstr::ContainsChar(str, L'z'));
}

static void WStrBuilderGrowPastExternal(wstr::Builder& str) {
    str.Append(L"blah");
    utassert(wstr::Eq(ToWStr(str), WStrL(L"blah")));
    str.Append(L"lost");
    utassert(wstr::Eq(ToWStr(str), WStrL(L"blahlost")));
    str.Reset();
    for (int i = 0; i < 200; i++) {
        str.AppendChar((WCHAR)i);
    }
    utassert(!str.UsesExternalBuf());
    for (int i = 0; i < 200; i++) {
        utassert(str[i] == (WCHAR)i);
    }
}

static void WStrBuilderRemoveAtStaysOnHeap(wstr::Builder& str) {
    for (int i = 0; i < 200; i++) {
        str.AppendChar((WCHAR)(L'a' + (i % 26)));
    }
    uintptr_t heap = (uintptr_t)str.begin();
    utassert(!str.UsesExternalBuf());
    str.RemoveAt(0, 190);
    utassert(len(str) == 10);
    utassert((uintptr_t)str.begin() == heap);
    str.Append(L"xyz");
    utassert((uintptr_t)str.begin() == heap);
    utassert(wstr::Eq(ToWStr(str), WStrL(L"ijklmnopqrxyz")));
}

static void WStrBuilderManyAppends(wstr::Builder& str) {
    for (int i = 0; i < 50; i++) {
        str.Append(L"01234567890123456789");
    }
    utassert(len(str) == 1000);
    utassert(wstr::StartsWith(ToWStr(str), WStrL(L"01234567890123456789")));
    utassert(wstr::EndsWith(ToWStr(str), WStrL(L"01234567890123456789")));
}

static void WStrBuilderTakeWStr(wstr::Builder& str) {
    str.Append(L"hello");
    WCHAR* before = str.begin();
    bool wasExternal = str.UsesExternalBuf();
    WStr taken = str.TakeWStr();
    utassert(wstr::Eq(taken, WStrL(L"hello")));
    if (wasExternal) {
        utassert(taken.s != before);
    }
    wstr::Free(taken);
    utassert(str.IsEmpty());
}

static void WStrBuilderCapHint() {
    wstr::Builder str(1024);
    uintptr_t heap = 0;
    for (int i = 0; i < 50; i++) {
        str.Append(L"01234567890123456789");
        if (i == 0) {
            heap = (uintptr_t)str.begin();
            utassert(heap != 0);
        }
    }
    utassert((uintptr_t)str.begin() == heap);
    utassert(str.nReallocs == 1);

    WCHAR stack[16];
    wstr::Builder str2(WStr(stack, dimofi(stack)));
    str2.cap = 1024 + 1; // +1 NUL padding, same as Builder(1024)
    heap = 0;
    int reallocsAtHeap = -1;
    for (int i = 0; i < 50; i++) {
        str2.Append(L"01234567890123456789");
        if (!str2.UsesExternalBuf() && reallocsAtHeap < 0) {
            heap = (uintptr_t)str2.begin();
            reallocsAtHeap = str2.nReallocs;
        }
    }
    utassert(heap != 0);
    utassert((uintptr_t)str2.begin() == heap);
    utassert(str2.nReallocs == reallocsAtHeap);
}

static void wstrBuilderTest() {
    WStrBuilderRunTwice(WStrBuilderContainsAppend);
    WStrBuilderRunTwice(WStrBuilderGrowPastExternal);
    WStrBuilderRunTwice(WStrBuilderRemoveAtStaysOnHeap);
    WStrBuilderRunTwice(WStrBuilderManyAppends);
    WStrBuilderRunTwice(WStrBuilderTakeWStr);
    WStrBuilderCapHint();
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
    utassert(str::Eq(before, StrL("key")) && str::Eq(after, StrL("value")));

    // only one side requested
    after = {};
    utassert(str::Cut(s, "=", nullptr, &after) && str::Eq(after, StrL("value")));
    before = {};
    utassert(str::Cut(s, "=", &before, nullptr) && str::Eq(before, StrL("key")));

    // separator not found: returns false, before = whole string, after = {}
    before = {};
    after = "sentinel";
    utassert(!str::Cut(s, "#", &before, &after));
    utassert(str::Eq(before, s) && len(after) == 0);

    // separator at the very end -> after is empty but Cut returns true
    utassert(str::Cut(s, "value", &before, &after));
    utassert(str::Eq(before, StrL("key=")) && len(after) == 0);

    // multi-char separator, only first occurrence splits
    utassert(str::Cut("a::b::c", "::", &before, &after));
    utassert(str::Eq(before, StrL("a")) && str::Eq(after, StrL("b::c")));
}

static void StrNextLineTest() {
    Str line, rest;

    // LF, CR and CRLF are all single line terminators
    rest = "a\nb\rc\r\nd";
    utassert(str::NextLine(rest, line, rest) && str::Eq(line, StrL("a")));
    utassert(str::NextLine(rest, line, rest) && str::Eq(line, StrL("b")));
    utassert(str::NextLine(rest, line, rest) && str::Eq(line, StrL("c")));
    utassert(str::NextLine(rest, line, rest) && str::Eq(line, StrL("d")));
    utassert(!str::NextLine(rest, line, rest));

    // empty input -> no line
    rest = Str{};
    utassert(!str::NextLine(rest, line, rest));

    // a trailing terminator does not yield an extra empty line
    rest = "a\n";
    utassert(str::NextLine(rest, line, rest) && str::Eq(line, StrL("a")));
    utassert(!str::NextLine(rest, line, rest));

    // empty lines are returned as empty (not skipped)
    rest = "\n\nx";
    utassert(str::NextLine(rest, line, rest) && len(line) == 0);
    utassert(str::NextLine(rest, line, rest) && len(line) == 0);
    utassert(str::NextLine(rest, line, rest) && str::Eq(line, StrL("x")));
    utassert(!str::NextLine(rest, line, rest));

    // final line without a terminator
    rest = "only";
    utassert(str::NextLine(rest, line, rest) && str::Eq(line, StrL("only")));
    utassert(len(rest) == 0);
    utassert(!str::NextLine(rest, line, rest));
}

static void StrStartsWithTest() {
    char emptyBuf1[] = "";
    char emptyBuf2[] = "";
    Str empty1(emptyBuf1, 0);
    Str empty2(emptyBuf2, 0);
    utassert(str::StartsWith(StrL("abc"), empty1));
    utassert(str::StartsWith(empty1, empty2));
    utassert(str::StartsWithI(StrL("abc"), empty1));
    utassert(str::StartsWithI(empty1, empty2));

    WCHAR emptyWBuf1[] = L"";
    WCHAR emptyWBuf2[] = L"";
    WStr emptyW1(emptyWBuf1, 0);
    WStr emptyW2(emptyWBuf2, 0);
    utassert(wstr::StartsWith(WStrL(L"abc"), emptyW1));
    utassert(wstr::StartsWith(emptyW1, emptyW2));
    utassert(wstr::StartsWithI(WStrL(L"abc"), emptyW1));
    utassert(wstr::StartsWithI(emptyW1, emptyW2));
}

static void StrArenaTest() {
    Arena* a = ArenaNew();
    utassert(a != nullptr);

    utassert(StrArenaToStr(a, 0).s == nullptr);
    utassert(StrArenaToStr(a, 0).len == 0);

    StrArena empty = StrArenaDupStr(a, StrL(""));
    utassert(empty != 0);
    Str emptyS = StrArenaToStr(a, empty);
    utassert(emptyS.len == 0);
    utassert(emptyS.s != nullptr);
    utassert(emptyS.s[0] == 0);

    StrArena sa = StrArenaDupStr(a, StrL("hello"));
    utassert(sa != 0);
    Str s = StrArenaToStr(a, sa);
    utassert(str::Eq(s, StrL("hello")));
    utassert(s.s[5] == 0); // C terminator after payload

    // multi-byte LEB128 length: 200 > 127
    StrArena big = StrArenaAlloc(a, 200);
    utassert(big != 0);
    Str bigS = StrArenaToStr(a, big);
    utassert(bigS.len == 200);
    utassert(bigS.s != nullptr);
    memset(bigS.s, 'x', 200);
    utassert(bigS.s[200] == 0);
    utassert(str::Eq(StrArenaToStr(a, big), Str(bigS.s, 200)));

    // multi-block arena: force a second chain block, then store a string there
    {
        ArenaParams params = ArenaDefaultParams();
        params.reserveSize = 4 * 1024;
        params.commitSize = 4 * 1024;
        Arena* a2 = ArenaNew(params);
        utassert(a2 != nullptr);
        // ArenaNew rounds the reserve up to a page, and a page is 16K on arm64
        // macOS, not 4K - so size the pushes from the block we actually got
        u64 half = a2->reserved / 2;
        void* filler = a2->Push(half, 8, true);
        utassert(filler != nullptr);
        // second large push forces a chained block (two halves + the header
        // don't fit in one)
        void* filler2 = a2->Push(half, 8, true);
        utassert(filler2 != nullptr);
        utassert(a2->current != a2);
        StrArena sa2 = StrArenaDupStr(a2, StrL("second-block"));
        utassert(sa2 != 0);
        utassert(sa2 >= (u32)a2->reserved); // compressed offset past first block
        utassert(str::Eq(StrArenaToStr(a2, sa2), StrL("second-block")));
        ArenaDelete(a2);
    }

    ArenaDelete(a);
}

void StrTest() {
    StrArenaTest();

    char buf[32];
    Str str = "a string";
    utassert(str.len == 8);
    utassert(str::Eq(str, StrL("a string")) && str::Eq(str, str));
    utassert(!str::Eq(str, Str{}) && !str::Eq(str, StrL("A String")));
    utassert(str::EqI(str, StrL("A String")) && str::EqI(str, str));
    utassert(!str::EqI(str, Str{}) && str::EqI(Str{}, Str{}));
    utassert(str::EqI(Str("AbCx", 3), Str("abcY", 3)));
    utassert(!str::EqI(Str("AbCx", 3), Str("abcY", 4)));
    utassert(str::EqN("abcd", "abce", 3) && !str::EqN("abcd", "Abcd", 3));
    utassert(str::StartsWith(str, StrL("a s")) && str::StartsWithI(str, StrL("A Str")));
    utassert(!str::StartsWith(str, StrL("Astr")));
    Str withoutPrefix = str;
    utassert(str::TrimPrefix(withoutPrefix, StrL("a ")) && str::Eq(withoutPrefix, StrL("string")));
    utassert(!str::TrimPrefix(withoutPrefix, StrL("a ")) && str::Eq(withoutPrefix, StrL("string")));
    utassert(str::EndsWith(str, StrL("ing")) && str::EndsWithI(str, StrL("ING")));
    utassert(!str::EndsWith(str, StrL("ung")));
    utassert(str::ContainsChar(str, 's') && !str::ContainsChar(str, 'S'));
    utassert(str::IndexOfChar(str, 's') == 2);
    utassert(str::IndexOfChar(str, 'g') == 7);
    utassert(!str::ContainsChar(str, 'x'));
    utassert(!str::ContainsChar(Str{}, 'a'));
    utassert(str::ContainsCharAny(str, StrL("xyz g")));  // matches the space and 'g'
    utassert(str::ContainsCharAny(str, StrL("s")));      // single candidate
    utassert(!str::ContainsCharAny(str, StrL("XYZ")));   // none present (case-sensitive)
    utassert(!str::ContainsCharAny(str, Str{}));         // no candidates
    utassert(!str::ContainsCharAny(Str{}, StrL("abc"))); // empty subject
    int n = str::BufSet(Str(buf, dimof(buf)), str);
    utassert(n == len(buf) && str::Eq(buf, str));
    n = str::BufSet(Str(buf, 6), str);
    utassert(n == 5 && str::Eq(buf, StrL("a str")));

    str = str::Dup(buf);
    utassert(str::Eq(str, buf));
    str::Free(str);
    str = str::Dup(Str(buf, 4));
    utassert(str::Eq(str, StrL("a st")));
    str::Free(str);
    str = fmt("%s", Str(buf));
    utassert(str::Eq(str, buf));
    str = fmt("%S", WStrL(L"a"
                          L"\x2019"
                          L"a.pdf"));
    utassert(str::Eq(str, StrL("a\xE2\x80\x99"
                               "a.pdf")));
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
    utassert(str::Eq(str, StrL("ab")));
    str::Free(str);

#if 0
    str = str::Join("\uFDEF", "\uFFFF");
    utassert(str::Eq(str, StrL("\uFDEF\uFFFF")));
    str::Free(str);
#endif

    str::BufSet(Str(buf, dimof(buf)), "abc\1efg\1");
    Str bufStr(buf, 9);
    str::TransCharsInPlace(bufStr, StrL("ace"), StrL("ACE"));
    utassert(str::Eq(buf, StrL("AbC\1Efg\1")));
    str::TransCharsInPlace(bufStr, StrL("\1"), StrL("\0"));
    utassert(str::Eq(buf, StrL("AbC")) && str::Eq(buf + 4, StrL("Efg")));
    str::TransCharsInPlace(bufStr, StrL(""), StrL("X"));
    utassert(str::Eq(buf, StrL("AbC")));

    str::BufSet(Str(buf, dimof(buf)), "blogarapato");
    int count = str::RemoveCharsInPlace(buf, "bo");
    utassert(3 == count);
    utassert(str::Eq(buf, StrL("lgarapat")));

    str::BufSet(Str(buf, dimof(buf)), "one\r\ntwo\t\v\f\tthree");
    count = str::NormalizeWSInPlace(Str(buf));
    utassert(4 == count);
    utassert(str::Eq(buf, StrL("one two three")));

    str::BufSet(Str(buf, dimof(buf)), " one    two three ");
    count = str::NormalizeWSInPlace(Str(buf));
    utassert(5 == count);
    utassert(str::Eq(buf, StrL("one two three")));

    count = str::NormalizeWSInPlace(Str(buf));
    utassert(0 == count);
    utassert(str::Eq(buf, StrL("one two three")));

    {
        // NormalizeWSTemp: already-normalized input returns the same buffer (no copy)
        Str norm = "one two three";
        TempStr r = str::NormalizeWSTemp(norm);
        utassert(r.s == norm.s);
        utassert(str::Eq(r, StrL("one two three")));
        // needs normalizing: returns a distinct temp copy, original untouched
        Str raw = " one\t\rtwo  three ";
        r = str::NormalizeWSTemp(raw);
        utassert(r.s != raw.s);
        utassert(str::Eq(r, StrL("one two three")));
        utassert(str::Eq(raw, StrL(" one\t\rtwo  three ")));
        // empty input
        utassert(str::NormalizeWSTemp(Str()).len == 0);
    }

    {
        Str str2 = "[Open(\"filename.pdf\",0,1,0)]";
        {
            uint u1 = 0;
            TempStr str1;
            Str end = str::Parse(str2, "[Open(\"%s\",%? 0,%u,0)]", &str1, &u1);
            utassert(!str::IsNull(end) && !end.s[0]);
            utassert(u1 == 1 && str::Eq(str1, StrL("filename.pdf")));
        }

        {
            uint u1 = 0;
            TempStr str1;
            Str end = str::Parse(str2, "[Open(\"%S\",0%?,%u,0)]", &str1, &u1);
            utassert(!str::IsNull(end) && !end.s[0]);
            utassert(u1 == 1 && str::Eq(str1, StrL("filename.pdf")));

            utassert(str::Parse(StrL("0xABCD"), "%x", &u1).s);
            utassert(u1 == 0xABCD);
            utassert(str::Parse(StrL("ABCD"), "%2x%S", &u1, &str1).s);
            utassert(u1 == 0xAB && str::Eq(str1, StrL("CD")));
        }
    }
    {
        int i1, i2;
        Str end = str::Parse(StrL("1, 2+3"), "%d,%d", &i1, &i2);
        utassert(!str::IsNull(end) && str::Eq(end, StrL("+3")));
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
        utassert(str::Eq(str1, StrL("leaks memory")));
    }

    {
        TempStr str1;
        int i, j;
        float f;
        utassert(str::Parse(StrL("ansi string, -30-20 1.5%"), "%S,%d%?-%2u%f%%%$", &str1, &i, &j, &f).s);
        utassert(str::Eq(str1, StrL("ansi string")) && i == -30 && j == 20 && f == 1.5f);
    }
    {
        TempStr str1;
        int i, j;
        float f;
        utassert(str::Parse(StrL("wide string, -30-20 1.5%"), "%S,%d%?-%2u%f%%%$", &str1, &i, &j, &f).s);
        utassert(str::Eq(str1, StrL("wide string")) && i == -30 && j == 20 && f == 1.5f);
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
        AutoCall freeStrA(free, (void*)strA);
        utassert(str::Eq(strA, TEST_STRING));
        auto res = strconv::AnsiToWStrTemp(Str(strA));
        utassert(wstr::Eq(res, TEXT(TEST_STRING)));
#undef TEST_STRING
    }

    utassert(str::IsDigit('0') && str::IsDigit(TEXT('5')) && str::IsDigit(L'9'));
#if OS_WIN
    utassert(iswdigit(L'\u0660') && !str::IsDigit(L'\xB2'));
#else
    utassert(!str::IsDigit(L'\xB2'));
#endif

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
        TempStr tmp = str::FormatNumWithThousandSepTemp((i64)formatNumData[i].number, LOCALE_INVARIANT);
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
        utassert(str::Eq(str1, StrL("aabbcc... 1-9")));
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
        int trimmed;
        Str s = str::Dup(StrL(""));
        trimmed = str::TrimWSInPlace(s, str::TrimOpt::Both);
        utassert(trimmed == 0);
        utassert(s.len == 0);
        utassert(str::Eq(s, StrL("")));
        trimmed = str::TrimWSInPlace(s, str::TrimOpt::Right);
        utassert(trimmed == 0);
        utassert(s.len == 0);
        utassert(str::Eq(s, StrL("")));
        trimmed = str::TrimWSInPlace(s, str::TrimOpt::Left);
        utassert(trimmed == 0);
        utassert(s.len == 0);
        utassert(str::Eq(s, StrL("")));

        str::ReplaceWithCopy(&s, "  \n\t  ");
        trimmed = str::TrimWSInPlace(s, str::TrimOpt::Both);
        utassert(trimmed == 6);
        utassert(s.len == 0);
        utassert(str::Eq(s, StrL("")));

        str::ReplaceWithCopy(&s, "  \n\t  ");
        trimmed = str::TrimWSInPlace(s, str::TrimOpt::Right);
        utassert(trimmed == 6);
        utassert(s.len == 0);
        utassert(str::Eq(s, StrL("")));

        str::ReplaceWithCopy(&s, "  \n\t  ");
        trimmed = str::TrimWSInPlace(s, str::TrimOpt::Left);
        utassert(trimmed == 6);
        utassert(s.len == 0);
        utassert(str::Eq(s, StrL("")));

        str::ReplaceWithCopy(&s, "  lola");
        trimmed = str::TrimWSInPlace(s, str::TrimOpt::Both);
        utassert(trimmed == 2);
        utassert(s.len == 4);
        utassert(str::Eq(s, StrL("lola")));

        str::ReplaceWithCopy(&s, "  lola");
        trimmed = str::TrimWSInPlace(s, str::TrimOpt::Left);
        utassert(trimmed == 2);
        utassert(s.len == 4);
        utassert(str::Eq(s, StrL("lola")));

        str::ReplaceWithCopy(&s, "  lola");
        trimmed = str::TrimWSInPlace(s, str::TrimOpt::Right);
        utassert(trimmed == 0);
        utassert(s.len == 6);
        utassert(str::Eq(s, StrL("  lola")));

        str::ReplaceWithCopy(&s, "lola\r\t");
        trimmed = str::TrimWSInPlace(s, str::TrimOpt::Both);
        utassert(trimmed == 2);
        utassert(s.len == 4);
        utassert(str::Eq(s, StrL("lola")));

        str::ReplaceWithCopy(&s, "lola\r\t");
        trimmed = str::TrimWSInPlace(s, str::TrimOpt::Right);
        utassert(trimmed == 2);
        utassert(s.len == 4);
        utassert(str::Eq(s, StrL("lola")));

        str::ReplaceWithCopy(&s, "lola\r\t");
        trimmed = str::TrimWSInPlace(s, str::TrimOpt::Left);
        utassert(trimmed == 0);
        utassert(s.len == 6);
        utassert(str::Eq(s, StrL("lola\r\t")));

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
        utassert(len(tmp) == 0);
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
    wstrBuilderTest();
    StrIsDigitTest();
    StrReplaceTest();
    StrSeqTest();
    StrSeqNumTest();
    StrConvTest();
    StrUrlExtractTest();
    StrFindITest();
    StrCutTest();
    StrNextLineTest();
    StrStartsWithTest();
    // ParseUntilTest();
}
