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
    conv = strconv::WstrToUtf8Buf(L"abc", cbuf, dimof(cbuf));
    utassert(conv == 3 && str::Eq(cbuf, "abc"));
    conv = strconv::Utf8ToWcharBuf("ab\xF0\x90\x82\x80", 6, wbuf, dimof(wbuf));
    utassert(conv == 3 && str::StartsWith(wbuf, L"ab") && wbuf[2] == 0xD800);
    conv = strconv::Utf8ToWcharBuf("ab\xF0\x90\x82\x80", 6, wbuf, dimof(wbuf) - 1);
    utassert(conv == 1 && str::Eq(wbuf, L"a"));
    conv = strconv::WstrToUtf8Buf(L"ab\u20AC", cbuf, dimof(cbuf));
    utassert(conv == 0 && str::Eq(cbuf, ""));
    conv = strconv::WstrToUtf8Buf(L"abcd", cbuf, dimof(cbuf));
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

void strWStrTest() {
    {
        // verify that we use buf for initial allocations
        str::WStr str;
        WCHAR* buf = str.Get();
        str.Append(L"blah");
        WCHAR* buf2 = str.Get();
        utassert(buf == buf2);
        utassert(str::Eq(buf2, L"blah"));
        str.Append(L"lost");
        buf2 = str.Get();
        utassert(str::Eq(buf2, L"blahlost"));
        utassert(buf == buf2);
        str.Reset();
        for (int i = 0; i < str::Str::kBufChars + 4; i++) {
            str.AppendChar((WCHAR)i);
        }
        buf2 = str.Get();
        // we should have allocated buf on the heap
        utassert(buf != buf2);
        for (int i = 0; i < str::Str::kBufChars + 4; i++) {
            WCHAR c = str.at(i);
            utassert(c == (WCHAR)i);
        }
    }

    {
        // verify that initialCapacity hint works
        str::WStr str(1024);
        WCHAR* buf = nullptr;

        for (int i = 0; i < 50; i++) {
            str.Append(L"01234567890123456789");
            if (i == 2) {
                // we filled Str::buf (32 bytes) by putting 20 bytes
                // and allocated heap for 1024 bytes. Remember the
                buf = str.Get();
            }
        }
        // we've appended 100*10 = 1000 chars, which is less than 1024
        // so WStr::buf should be the same as buf
        WCHAR* buf2 = str.Get();
        utassert(buf == buf2);
    }
}

static void assertStrEq(const char* s1, const char* s2) {
    bool ok = str::Eq(s1, s2);
    utassert(ok);
}

static void CheckRemoveAt(StrVec& v) {
    while (v.Size() > 0) {
        int n = v.Size();
        int idx = v.Size() / 2;
        auto exp = v[idx];
        char* got;
        if (n % 2 == 0) {
            got = v.RemoveAt(idx);
        } else {
            got = v.RemoveAtFast(idx);
        }
        utassert(exp == got); // should be exact same pointer value
        utassert(v.Size() == n - 1);
    }
}

static void StrVecCheckIter(StrVec& v, const char** strs) {
    int i = 0;
    for (char* s : v) {
        char* s2 = v[i];
        utassert(str::Eq(s, s2));
        if (strs) {
            const char* s3 = strs[i];
            utassert(str::Eq(s, s3));
        }
        i++;
    }
}

static void StrVecTest() {
    const char* strs[] = {"foo", "bar", "Blast", nullptr, "this is a large string, my friend"};
    int unsortedOrder[] = {0, 1, 2, 3, 4};
    int sortedOrder[]{3, 2, 1, 0, 4};
    int sortedNoCaseOrder[]{3, 1, 2, 0, 4};

    int n = (int)dimof(strs);
    StrVec v;
    utassert(v.Size() == 0);
    for (int i = 0; i < n; i++) {
        v.Append(strs[i]);
        utassert(v.Size() == i + 1);
    }
    StrVecCheckIter(v, strs);

    StrVec sortedView = v;
    sortedView.Sort();

    for (int i = 0; i < n; i++) {
        char* got = sortedView.at(i);
        auto exp = strs[sortedOrder[i]];
        assertStrEq(got, exp);
    }

    // allocate a bunch to test allocating
    for (int i = 0; i < 1024; i++) {
        v.Append(strs[4]);
    }
    utassert(v.Size() == 1024 + n);

    for (int i = 0; i < n; i++) {
        auto got = v.at(i);
        auto exp = strs[unsortedOrder[i]];
        assertStrEq(got, exp);
    }

    for (int i = 0; i < 1024; i++) {
        auto got = v.at(i + n);
        auto exp = strs[4];
        assertStrEq(got, exp);
    }
    sortedView.SortNoCase();

    for (int i = 0; i < n; i++) {
        auto got = sortedView.at(i);
        auto exp = strs[sortedNoCaseOrder[i]];
        assertStrEq(got, exp);
    }

    v.Sort();
    for (int i = 0; i < n; i++) {
        char* got = v.at(i);
        auto exp = strs[sortedOrder[i]];
        assertStrEq(got, exp);
    }
    StrVecCheckIter(v, nullptr);
    v.SortNoCase();
    for (int i = 0; i < n; i++) {
        char* got = v.at(i);
        auto exp = strs[sortedNoCaseOrder[i]];
        assertStrEq(got, exp);
    }
    v.SetAt(3, nullptr);
    utassert(nullptr == v[3]);
    CheckRemoveAt(v);
}

static void StrVecTest2() {
    StrVec v;
    v.Append("foo");
    v.Append("bar");
    char* s = Join(v);
    utassert(v.size() == 2);
    utassert(str::Eq("foobar", s));
    str::Free(s);

    s = Join(v, ";");
    utassert(v.size() == 2);
    utassert(str::Eq("foo;bar", s));
    str::Free(s);

    v.Append(nullptr);
    utassert(v.size() == 3);

    v.Append("glee");
    s = Join(v, "_ _");
    utassert(v.size() == 4);
    utassert(str::Eq("foo_ _bar_ _glee", s));
    str::Free(s);

    StrVecCheckIter(v, nullptr);
    v.Sort();
    const char* strsSorted[] = {nullptr, "bar", "foo", "glee"};
    StrVecCheckIter(v, strsSorted);

    s = Join(v, "++");
    utassert(v.size() == 4);
    utassert(str::Eq("bar++foo++glee", s));
    str::Free(s);

    s = Join(v);
    utassert(str::Eq("barfooglee", s));
    str::Free(s);

    {
        StrVec v2(v);
        utassert(str::Eq(v2.at(2), "foo"));
        v2.Append("nobar");
        utassert(str::Eq(v2.at(4), "nobar"));
        v2 = v;
        utassert(v2.size() == 4);
        // copies should be same values but at different addresses
        utassert(v2.at(1) != v.at(1));
        utassert(str::Eq(v2.at(1), v.at(1)));
        s = v2.at(2);
        utassert(str::Eq(s, "foo"));
        CheckRemoveAt(v2);
    }

    {
        StrVec v2;
        size_t count = Split(v2, "a,b,,c,", ",");
        utassert(count == 5 && v2.Find("c") == 3);
        utassert(v2.Find("") == 2);
        utassert(v2.Find("", 3) == 4);
        utassert(v2.Find("", 5) == -1);
        utassert(v2.Find("B") == -1 && v2.FindI("B") == 1);
        TempStr joined = JoinTemp(v2, ";");
        utassert(str::Eq(joined, "a;b;;c;"));
        CheckRemoveAt(v2);
    }

    {
        StrVec v2;
        size_t count = Split(v2, "a,b,,c,", ",", true);
        utassert(count == 3 && v2.Find("c") == 2);
        TempStr joined = JoinTemp(v2, ";");
        utassert(str::Eq(joined, "a;b;c"));
        StrVecCheckIter(v2, nullptr);

#if 0
        AutoFreeWstr last(v2.Pop());
        utassert(v2.size() == 2 && str::Eq(last, L"c"));
#endif
        CheckRemoveAt(v2);
    }
    CheckRemoveAt(v);
}

static void StrVecTest3() {
    StrVec v;
    utassert(v.size() == 0);
    v.Append(str::Dup("one"));
    v.Append(str::Dup("two"));
    v.Append(str::Dup("One"));
    utassert(v.size() == 3);
    utassert(str::Eq(v.at(0), "one"));
    utassert(str::EqI(v.at(2), "one"));
    utassert(v.Find("One") == 2);
    utassert(v.FindI("One") == 0);
    utassert(v.Find("Two") == -1);
    StrVecCheckIter(v, nullptr);
    CheckRemoveAt(v);
}

void StrTest() {
    WCHAR buf[32];
    const WCHAR* str = L"a string";
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
    str::Free(str);
    str = str::Dup(buf, 4);
    utassert(str::Eq(str, L"a st"));
    str::Free(str);
    str = str::Format(L"%s", buf);
    utassert(str::Eq(str, buf));
    str::Free(str);
    {
        AutoFreeWstr large(AllocArray<WCHAR>(2000));
        memset(large, 0x11, 1998);
        str = str::Format(L"%s", large.Get());
        utassert(str::Eq(str, large));
        str::Free(str);
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
    str::Free(str);
    str = str::Join(nullptr, L"ab");
    utassert(str::Eq(str, L"ab"));
    str::Free(str);
    str = str::Join(L"\uFDEF", L"\uFFFF");
    utassert(str::Eq(str, L"\uFDEF\uFFFF"));
    str::Free(str);

    str::BufSet(buf, dimof(buf), L"abc\1efg\1");
    size_t count = str::TransCharsInPlace(buf, L"ace", L"ACE");
    utassert(str::Eq(buf, L"AbC\1Efg\1") && count == 3);
    count = str::TransCharsInPlace(buf, L"\1", L"\0");
    utassert(count == 2);
    utassert(str::Eq(buf, L"AbC") && str::Eq(buf + 4, L"Efg") && count == 2);
    count = str::TransCharsInPlace(buf, L"", L"X");
    utassert(str::Eq(buf, L"AbC") && count == 0);

    str::BufSet(buf, dimof(buf), L"blogarapato");
    count = str::RemoveCharsInPlace(buf, L"bo");
    utassert(3 == count);
    utassert(str::Eq(buf, L"lgarapat"));

    str::BufSet(buf, dimof(buf), L"one\r\ntwo\t\v\f\tthree");
    count = str::NormalizeWSInPlace(buf);
    utassert(4 == count);
    utassert(str::Eq(buf, L"one two three"));

    str::BufSet(buf, dimof(buf), L" one    two three ");
    count = str::NormalizeWSInPlace(buf);
    utassert(5 == count);
    utassert(str::Eq(buf, L"one two three"));

    count = str::NormalizeWSInPlace(buf);
    utassert(0 == count);
    utassert(str::Eq(buf, L"one two three"));

    str = L"[Open(\"filename.pdf\",0,1,0)]";
    {
        uint u1 = 0;
        WCHAR* str1 = nullptr;
        const WCHAR* end = str::Parse(str, L"[Open(\"%s\",%? 0,%u,0)]", &str1, &u1);
        utassert(end && !*end);
        utassert(u1 == 1 && str::Eq(str1, L"filename.pdf"));
        free(str1);
    }

    {
        uint u1 = 0;
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
    str = strconv::AnsiToWstr(strA.Get());
    utassert(str::Eq(str, TEXT(TEST_STRING)));
    str::Free(str);
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
        const char* result;
    } formatNumData[] = {
        {1, "1"},          {12, "12"},          {123, "123"},           {1234, "1,234"},
        {12345, "12,345"}, {123456, "123,456"}, {1234567, "1,234,567"}, {12345678, "12,345,678"},
    };

    for (int i = 0; i < dimof(formatNumData); i++) {
        char* tmp = str::FormatNumWithThousandSepTemp(formatNumData[i].number, LOCALE_INVARIANT);
        utassert(str::Eq(tmp, formatNumData[i].result));
    }

    struct {
        double number;
        const char* result;
    } formatFloatData[] = {
        {1, "1.0"},        {1.2, "1.2"},        {1.23, "1.23"},          {1.234, "1.23"},
        {12.345, "12.35"}, {123.456, "123.46"}, {1234.5678, "1,234.57"},
    };

    for (int i = 0; i < dimof(formatFloatData); i++) {
        char* tmp = str::FormatFloatWithThousandSepTemp(formatFloatData[i].number, LOCALE_INVARIANT);
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
        const char* result;
    } formatRomanData[] = {
        {1, "I"},        {3, "III"},      {6, "VI"},         {14, "XIV"},    {49, "XLIX"},
        {176, "CLXXVI"}, {499, "CDXCIX"}, {1666, "MDCLXVI"}, {2011, "MMXI"}, {12345, "MMMMMMMMMMMMCCCXLV"},
        {0, nullptr},    {-133, nullptr},
    };

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
        AutoFree tmp = strconv::ToMultiByte("abc", 9876, 123456);
        utassert(!tmp.Get());
    }
    {
        AutoFree tmp = strconv::WstrToCodePage(98765, L"abc");
        utassert(!tmp.Get());
    }
    {
        AutoFreeWstr tmp(strconv::StrToWstr("abc", 12345));
        utassert(str::IsEmpty(tmp.Get()));
    }
    {
        AutoFree tmp = strconv::WstrToCodePage(987654, L"abc");
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
        WCHAR buf1[6]{};
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
            utassert(!!isspace((u8)c) == str::IsWs((char)c));
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

    strStrTest();
    strWStrTest();
    StrIsDigitTest();
    StrReplaceTest();
    StrSeqTest();
    StrConvTest();
    StrUrlExtractTest();
    // ParseUntilTest();
    StrVecTest();
    StrVecTest2();
    StrVecTest3();
}
