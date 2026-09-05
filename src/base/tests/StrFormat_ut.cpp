/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include <locale.h>

// must be last due to assert() over-write
#include "base/UtAssert.h"

using str::FormatTemp;

// printf-based reference implementation of the old fmt(), preserved here so the
// tests can compare FormatTemp() against real printf semantics even after fmt()
// is itself replaced by FormatTemp(). FmtVTemp and the helpers it depends on
// used to live in Str.cpp; they moved here once fmt() was the only caller.

#ifdef _MSC_VER
static _locale_t GetUtf8FormatLocale() {
    static _locale_t loc = _create_locale(LC_ALL, ".UTF-8");
    return loc;
}
#endif

static int VsnprintfUtf8(char* buf, size_t bufCchSize, const char* f, va_list args) {
#ifdef _MSC_VER
    _locale_t loc = GetUtf8FormatLocale();
    if (loc) {
        return _vsnprintf_l(buf, bufCchSize, f, loc, args);
    }
#endif
    return vsnprintf(buf, bufCchSize, f, args);
}

static int VscprintfUtf8(const char* f, va_list args) {
#ifdef _MSC_VER
    _locale_t loc = GetUtf8FormatLocale();
    if (loc) {
        return _vscprintf_l(f, loc, args);
    }
#endif
    va_list argsCopy;
    va_copy(argsCopy, args);
    int res = vsnprintf(nullptr, 0, f, argsCopy);
    va_end(argsCopy);
    return res;
}

static TempStr FmtVTemp(const char* f, va_list args) {
    Arena* a = GetTempArena();
    char message[512]{};
    va_list argsCopy;
    va_copy(argsCopy, args);
    int count = VsnprintfUtf8(message, dimof(message), f, argsCopy);
    va_end(argsCopy);
    if ((count >= 0) && (count < dimofi(message))) {
        return str::Dup(a, Str(message, count));
    }

    va_copy(argsCopy, args);
    count = VscprintfUtf8(f, argsCopy);
    va_end(argsCopy);
    ReportIf(count == -1);
    if (count < 0) {
        return str::Dup(a, StrL("vsnprintf() returned -1"));
    }

    char* buf = AllocArray<char>(a, count + 1);
    if (!buf) {
        return {};
    }

    va_copy(argsCopy, args);
    int count2 = VsnprintfUtf8(buf, (size_t)count + 1, f, argsCopy);
    va_end(argsCopy);
    ReportIf(count2 != count);
    if (count2 < 0) {
        return str::Dup(a, StrL("vsnprintf() returned -1"));
    }
    return Str(buf, count);
}

static TempStr fmtRef(const char* f, ...) {
    va_list args;
    va_start(args, f);
    TempStr res = FmtVTemp(f, args);
    va_end(args);
    return res;
}

static void check(Str got, Str expected) {
    utassert(str::Eq(got, expected));
}

// verify FormatTemp() matches printf-based fmtRef() for the same value+spec.
// `expected` documents the result so the test stays meaningful after fmt()
// itself is switched to FormatTemp().
#define checkFmt(expected, spec, val)                    \
    do {                                                 \
        check(FormatTemp(spec, val), fmtRef(spec, val)); \
        check(FormatTemp(spec, val), StrL(expected));    \
    } while (0)

static void testStrings() {
    check(FormatTemp("%s", StrL("foo")), StrL("foo"));
    check(FormatTemp("%s", StrL("")), StrL("")); // empty string
    check(FormatTemp("[%s]", StrL("")), StrL("[]"));
    check(FormatTemp("%s%s%s", StrL("a"), StrL("b"), StrL("c")), StrL("abc"));
    check(FormatTemp("%s", WStrL(L"wide")), StrL("wide")); // WStr -> utf8
    // %S with a non-ASCII wide string (U+2019 -> utf8 e2 80 99)
    check(FormatTemp("%S", WStrL(L"a"
                                 L"\x2019"
                                 L"a.pdf")),
          StrL("a\xE2\x80\x99"
               "a.pdf"));
    // width / left-justify / precision (truncation, byte-based like printf)
    check(FormatTemp("%-16s", StrL("hi")), fmtRef("%-16s", "hi"));
    check(FormatTemp("%-16s", StrL("hi")), StrL("hi              "));
    check(FormatTemp("%10s", StrL("hi")), fmtRef("%10s", "hi"));
    check(FormatTemp("%10s", StrL("hi")), StrL("        hi"));
    check(FormatTemp("%.3s", StrL("abcdef")), fmtRef("%.3s", "abcdef"));
    check(FormatTemp("%.3s", StrL("abcdef")), StrL("abc"));
    check(FormatTemp("%.0s", StrL("abc")), StrL(""));
    check(FormatTemp("%-9s|", StrL("abc")), StrL("abc      |"));
    // a string longer than the internal numeric buffer must be fine (%s is manual)
    Str long600;
    {
        str::Builder sb;
        for (int i = 0; i < 600; i++) {
            sb.AppendChar('x');
        }
        long600 = ToStrTemp(sb);
    }
    check(FormatTemp("%s", long600), long600);
}

static void testChars() {
    check(FormatTemp("%c", 'x'), StrL("x"));
    check(FormatTemp("[%c]", 'A'), StrL("[A]"));
    check(FormatTemp("%c", 'x'), fmtRef("%c", 'x'));
}

static void testInts() {
    checkFmt("0", "%d", 0);
    checkFmt("5", "%d", 5);
    checkFmt("-23", "%d", -23);
    checkFmt("2147483647", "%d", INT_MAX);
    checkFmt("-2147483648", "%d", INT_MIN);
    // width / zero pad / left justify
    checkFmt("0034", "%04d", 34);
    checkFmt("  34", "%4d", 34);
    checkFmt("34  ", "%-4d", 34);
    checkFmt("-007", "%04d", -7);
    checkFmt("+7", "%+d", 7);
    // 64-bit
    check(FormatTemp("%lld", (i64)9223372036854775807LL), StrL("9223372036854775807"));
    check(FormatTemp("%lld", (i64)(-9223372036854775807LL - 1)), StrL("-9223372036854775808"));
    check(FormatTemp("%lld", (i64)9223372036854775807LL), fmtRef("%lld", (i64)9223372036854775807LL));
    check(FormatTemp("%ld", (i64)-7), fmtRef("%ld", -7l));
}

static void testUnsigned() {
    check(FormatTemp("%u", (i64)0), StrL("0"));
    check(FormatTemp("%u", (i64)4000000000), StrL("4000000000"));
    check(FormatTemp("%u", (i64)4000000000), fmtRef("%u", 4000000000u));
    check(FormatTemp("%llu", (i64)18446744073709551615ULL), fmtRef("%llu", 18446744073709551615ULL));
    check(FormatTemp("%llu", (i64)18446744073709551615ULL), StrL("18446744073709551615"));
    check(FormatTemp("%zu", (size_t)1234), StrL("1234"));
}

static void testHex() {
    check(FormatTemp("%x", 255), StrL("ff"));
    check(FormatTemp("%X", 255), StrL("FF"));
    check(FormatTemp("%x", 255), fmtRef("%x", 255));
    check(FormatTemp("%02x", 5), StrL("05"));
    check(FormatTemp("%08X", (int)0xdeadbeef), fmtRef("%08X", 0xdeadbeef));
    check(FormatTemp("%08X", (int)0xdeadbeef), StrL("DEADBEEF"));
    check(FormatTemp("%#x", 255), StrL("0xff"));
    check(FormatTemp("%#x", 255), fmtRef("%#x", 255));
    check(FormatTemp("%06x", 0x1234), StrL("001234"));
    // negative int via %x reads as 32-bit unsigned (matches printf)
    check(FormatTemp("%x", -1), StrL("ffffffff"));
    check(FormatTemp("%x", -1), fmtRef("%x", -1));
    // 64-bit hex
    check(FormatTemp("%016llX", (i64)0x123456789ABCLL), fmtRef("%016llX", (i64)0x123456789ABCLL));
    check(FormatTemp("%016llX", (i64)0x123456789ABCLL), StrL("0000123456789ABC"));
    check(FormatTemp("%016I64X", (i64)0xFFFFFFFFFFFFFFFFLL), StrL("FFFFFFFFFFFFFFFF"));
    check(FormatTemp("%lx", (i64)0xabc), fmtRef("%lx", 0xabcul));
}

static void testPointers() {
    void* p = (void*)(uintptr_t)0x1234;
    check(FormatTemp("%p", p), fmtRef("%p", p));
    void* nullp = nullptr;
    check(FormatTemp("%p", nullp), fmtRef("%p", nullp));
}

static void testFloats() {
    check(FormatTemp("%f", 3.45f), fmtRef("%f", 3.45f));
    check(FormatTemp("%f", 3.45f), StrL("3.450000"));
    check(FormatTemp("%.2f", 3.456), fmtRef("%.2f", 3.456));
    check(FormatTemp("%.2f", 3.456), StrL("3.46"));
    check(FormatTemp("%.1f", -18.38f), fmtRef("%.1f", -18.38f));
    check(FormatTemp("%.0f", 2.0), StrL("2"));
    check(FormatTemp("%.0f", 0.0), StrL("0"));
    check(FormatTemp("%g", 0.5), fmtRef("%g", 0.5));
    check(FormatTemp("%6.2f", 3.5), fmtRef("%6.2f", 3.5));
    check(FormatTemp("%6.2f", 3.5), StrL("  3.50"));
    check(FormatTemp("%+.1f", 1.25), StrL("+1.2")); // banker-ish rounding via printf
    check(FormatTemp("%+.1f", 1.25), fmtRef("%+.1f", 1.25));
    // double argument
    check(FormatTemp("%.2f", 1234.5678), StrL("1234.57"));
}

// integer-family specs (%d %x %c %p) accept any integer-like arg (char / int /
// pointer), matching printf. This is what lets e.g. an HWND print with %x.
static void testCrossType() {
    void* p = (void*)(uintptr_t)0xabcd;
    check(FormatTemp("0x%x", p), StrL("0xabcd"));                 // pointer via %x
    check(FormatTemp("%p", 0x1234), fmtRef("%p", (void*)0x1234)); // int via %p
    check(FormatTemp("%c", 65), StrL("A"));                       // int via %c
    check(FormatTemp("%d", 'A'), StrL("65"));                     // char via %d
    check(FormatTemp("%d", (char)-1), StrL("-1"));                // signed char via %d
}

static void testEscapeAndRaw() {
    check(FormatTemp("100%%"), StrL("100%"));
    check(FormatTemp("%d%% done", 50), StrL("50% done"));
    check(FormatTemp("no specifiers"), StrL("no specifiers"));
    check(FormatTemp(""), StrL(""));
    // '{' is not special, so nothing needs escaping: registry paths, GUIDs and
    // CSS / JS templates go through fmt unharmed
    check(FormatTemp("a{b"), StrL("a{b"));
    check(FormatTemp("{}"), StrL("{}"));
    check(FormatTemp("Class\\{4d36e968}\\%d", 7), StrL("Class\\{4d36e968}\\7"));
}

static void testPositional() {
    check(FormatTemp("c: %{0}, i: %{1}", 'x', -18), StrL("c: x, i: -18"));
    check(FormatTemp("be%{0}-af", 888723), StrL("be888723-af"));
    check(FormatTemp("int: %{1}, s: %{0}", WStrL(L"hello"), -1), StrL("int: -1, s: hello"));
    check(FormatTemp("%{1}-%{0}", StrL("so"), WStrL(L"r")), StrL("r-so"));
    check(FormatTemp("%{0}%{0}%{0}", StrL("ab")), StrL("ababab")); // repeated positional
    check(FormatTemp("foo %v", -23), StrL("foo -23"));
    check(FormatTemp("%v %v %v", 'c', 5, StrL("s")), StrL("c 5 s"));
}

// %{} is "the next argument, whatever its type", like %v
static void testAnyType() {
    check(FormatTemp("foo %{}", -23), StrL("foo -23"));
    check(FormatTemp("%{} %{} %{}", 'c', 5, StrL("s")), StrL("c 5 s"));
    check(FormatTemp("%{} = %d", StrL("n"), 7), StrL("n = 7"));
    check(FormatTemp("%d = %{}", 7, WStrL(L"n")), StrL("7 = n"));
}

// A '%{' with no closing '}' used to walk past the end of the format string
// (ASAN: global-buffer-overflow in parseArgDefPositional). Malformed input must
// fail safe instead. Reachable via a translated format string.
static void testUnterminatedPositional() {
    check(FormatTemp("%{1"), StrL(""));
    check(FormatTemp("abc%{12"), StrL(""));
    check(FormatTemp("%{"), StrL(""));
    check(FormatTemp("%{x}"), StrL("")); // only digits may go inside
    check(FormatTemp("{1"), StrL("{1")); // no '%': raw text, not a directive
}

// Fmt holds 32 instructions and no more. Every parser that adds one has to
// check that, or a format string with more directives than that walks off the
// end of the array into the fields behind it. A translated format string is
// runtime input, so this is reachable.
static void testMoreDirectivesThanFit() {
    check(FormatTemp("a%%a%%a%%a%%a%%a%%a%%a%%a%%a%%a%%a%%a%%a%%a%%a%%a%%a%%a%%a%%a%%a%%a%%a%%a%%a%%a%%a%%a%%a%%a%%a%%"
                     "a%%a%%a%%"),
          StrL(""));
    check(FormatTemp("a%%b%%c"), StrL("a%b%c"));

    // The same past the array with % specs, which parse down a different path
    // than the literals do: thirty-four directives, all of them fed.
    check(
        FormatTemp("%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d", 1, 2, 3, 4, 5, 6, 7, 8, 9,
                   10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34),
        StrL(""));
    // And thirty-two of them, which is the most that fits, still formats.
    utassert(FormatTemp("%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d", 1, 2, 3, 4, 5, 6, 7, 8, 9,
                        10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32)
                 .len > 0);
}

// Every conversion but %s goes through a 256-byte buffer in Fmt. A field too
// wide for it is written straight into the answer instead, so the width says
// what it says rather than being cut to 255 characters with nothing to say so.
static void testFieldWiderThanTheScratchBuffer() {
    Str s = FormatTemp("%500d", 1);
    utassert(s.len == 500);
    utassert(s.s[0] == ' ' && s.s[498] == ' ' && s.s[499] == '1');
    check(s, fmtRef("%500d", 1));

    // what follows the wide field is still appended after it
    Str after = FormatTemp("%500d|", 1);
    utassert(after.len == 501 && after.s[500] == '|');

    // a precision, which is the other way to outgrow the buffer
    Str precise = FormatTemp("%.400f", 0.5);
    utassert(precise.len == 402);
    check(precise, fmtRef("%.400f", 0.5));

    // two of them in one format, so the second is not written over the first:
    // the answer grows, it is not a buffer being reused
    Str both = FormatTemp("%300d;%300d", 1, 2);
    utassert(both.len == 601 && both.s[300] == ';');
    utassert(both.s[299] == '1' && both.s[600] == '2');
}

void StrFormatTest() {
    testStrings();
    testChars();
    testInts();
    testUnsigned();
    testHex();
    testPointers();
    testFloats();
    testCrossType();
    testEscapeAndRaw();
    testPositional();
    testAnyType();
    testUnterminatedPositional();
    testMoreDirectivesThanFit();
    testFieldWiderThanTheScratchBuffer();
}
