/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace str {

// argument to a formatting instruction
// at the front are arguments given with i(), s() etc.
// at the end are FormatStr arguments from format string
struct FmtArg {
    enum class Kind {
        // concrete types for FmtArg
        Char,
        Int,
        Ptr,
        Float,
        Double,
        Str,
        WStr,

        // for Inst.t
        RawStr, // copy part of format string
        Any,

        None,
    };

    Kind t{Kind::None};
    // A FmtArg only ever holds one value, selected by t, so the value fields
    // share storage. Str/WStr are trivially copyable (only their default ctor is
    // non-trivial), so they are valid union members; the union has no implicit
    // default ctor, hence FmtArg() initializes a trivial member explicitly.
    union {
        Str str;
        WStr wstr;
        char c;
        i64 i;
        float f;
        double d;
        const void* ptr;
    };

    FmtArg() : i{0} {} // t stays None; init the union via a trivial member

    // All single-argument constructors are explicit so values never implicitly
    // convert to FmtArg (e.g. a stray pointer or bool). FormatTemp() is a
    // variadic template that constructs each FmtArg explicitly, so call sites
    // stay terse.

    explicit FmtArg(char c_) : t{Kind::Char}, c{c_} {}

    // integer family: a constructor per fundamental type so e.g. unsigned int
    // (UINT/DWORD), unsigned long, size_t etc. are not ambiguous between
    // FmtArg(int)/FmtArg(i64)/FmtArg(size_t). The conversion char (%d vs %u vs
    // %x) decides the rendering; we just carry the value as i64.
    explicit FmtArg(int arg) : t{Kind::Int}, i{(i64)arg} {}
    explicit FmtArg(unsigned int arg) : t{Kind::Int}, i{(i64)arg} {}
    explicit FmtArg(long arg) : t{Kind::Int}, i{(i64)arg} {}
    explicit FmtArg(unsigned long arg) : t{Kind::Int}, i{(i64)arg} {}
    explicit FmtArg(long long arg) : t{Kind::Int}, i{(i64)arg} {}
    explicit FmtArg(unsigned long long arg) : t{Kind::Int}, i{(i64)arg} {}

    explicit FmtArg(float f_) : t{Kind::Float}, f{f_} {}

    explicit FmtArg(double d_) : t{Kind::Double}, d{d_} {}

    explicit FmtArg(Str arg) : t{Kind::Str}, str{arg} {}

    explicit FmtArg(WStr arg) : t{Kind::WStr}, wstr{arg} {}

    explicit FmtArg(const void* p) : t{Kind::Ptr}, ptr{p} { // for %p
    }

    // raw C strings are not allowed: pass Str / WStr explicitly so we never
    // depend on a NUL-terminated char*/wchar_t*
    FmtArg(char*) = delete;
    FmtArg(const char*) = delete;
    FmtArg(wchar_t*) = delete;
    FmtArg(const wchar_t*) = delete;
};

TempStr FormatTempArgs(const char* fmt, const FmtArg** args, int nArgs);

inline TempStr FormatTemp(const char* fmt) {
    return FormatTempArgs(fmt, nullptr, 0);
}

template <typename... TArgs>
TempStr FormatTemp(const char* fmt, const TArgs&... args) {
    const FmtArg argv[] = {FmtArg(args)...};
    const FmtArg* argp[sizeof...(TArgs)];
    int n = (int)sizeof...(TArgs);
    for (int i = 0; i < n; i++) {
        argp[i] = &argv[i];
    }
    return FormatTempArgs(fmt, argp, n);
}

Str FormatArgs(Arena* a, const char* fmt, const FmtArg** args, int nArgs);

inline Str Format(Arena* a, const char* fmt) {
    return FormatArgs(a, fmt, nullptr, 0);
}

template <typename... TArgs>
Str Format(Arena* a, const char* fmt, const TArgs&... args) {
    const FmtArg argv[] = {FmtArg(args)...};
    const FmtArg* argp[sizeof...(TArgs)];
    int n = (int)sizeof...(TArgs);
    for (int i = 0; i < n; i++) {
        argp[i] = &argv[i];
    }
    return FormatArgs(a, fmt, argp, n);
}

// Type-safe scanf-style parsing (analogous to str::Format). Each output arg
// is a pointer whose type is captured by ParseArg's explicit constructors, so a
// format/arg mismatch (e.g. %d into a WORD*) is a compile error, not silent UB.
// %s / %S write a TempStr (or TempWStr) into the passed-in pointer -- the result
// lives in the temp arena, so the caller doesn't free it; the numeric/char specs
// write via the matching pointer type.
struct ParseArg {
    enum class Kind : u8 {
        None,
        Int,
        UInt,
        Float,
        Char,
        StrOut,
        WStrOut
    };
    Kind kind = Kind::None;
    void* ptr = nullptr;

    ParseArg() = default;
    explicit ParseArg(int* p) : kind(Kind::Int), ptr(p) {}
    explicit ParseArg(unsigned int* p) : kind(Kind::UInt), ptr(p) {}
    explicit ParseArg(float* p) : kind(Kind::Float), ptr(p) {}
    explicit ParseArg(char* p) : kind(Kind::Char), ptr(p) {}
    explicit ParseArg(Str* p) : kind(Kind::StrOut), ptr(p) {}
    explicit ParseArg(WStr* p) : kind(Kind::WStrOut), ptr(p) {}
};

Str ParseArgs(Str str, const char* fmt, const ParseArg* args, int nArgs);

inline Str Parse(Str str, const char* fmt) {
    return ParseArgs(str, fmt, nullptr, 0);
}

template <typename... TArgs>
Str Parse(Str str, const char* fmt, TArgs*... args) {
    const ParseArg argv[] = {ParseArg(args)...};
    return ParseArgs(str, fmt, argv, (int)sizeof...(TArgs));
}

TempStr FormatFloatWithThousandSepTemp(double number, LCID locale = LOCALE_USER_DEFAULT, bool stripTrailingZero = true);
TempStr FormatNumWithThousandSepTemp(i64 num, LCID locale = LOCALE_USER_DEFAULT);
TempStr FormatSizeShortTemp(i64 size);
TempStr FormatSizeShortTemp(i64 size, Str const* sizeUnits);
TempStr FormatFileSizeTemp(i64);
TempStr FormatRomanNumeralTemp(int number);

} // namespace str

namespace wstr {

// Type-safe scanf-style parsing, the wide-char analogue of str::Parse. Each
// output arg is a pointer whose type is captured by ParseArg's explicit
// constructors, so a format/arg mismatch is a compile error, not silent UB.
// %s / %S write a TempWStr into the passed-in WStr* -- the result lives in the
// temp arena, so the caller doesn't free it; the numeric/char specs write via
// the matching pointer type.
struct ParseArg {
    enum class Kind : u8 {
        None,
        Int,
        UInt,
        Float,
        Char,
        WStrOut
    };
    Kind kind = Kind::None;
    void* ptr = nullptr;

    ParseArg() = default;
    explicit ParseArg(int* p) : kind(Kind::Int), ptr(p) {}
    explicit ParseArg(unsigned int* p) : kind(Kind::UInt), ptr(p) {}
    explicit ParseArg(float* p) : kind(Kind::Float), ptr(p) {}
    explicit ParseArg(WCHAR* p) : kind(Kind::Char), ptr(p) {}
    explicit ParseArg(WStr* p) : kind(Kind::WStrOut), ptr(p) {}
};

WStr ParseArgs(WStr str, const WCHAR* fmt, const ParseArg* args, int nArgs);

inline WStr Parse(WStr str, const WCHAR* fmt) {
    return ParseArgs(str, fmt, nullptr, 0);
}

template <typename... TArgs>
WStr Parse(WStr str, const WCHAR* fmt, TArgs*... args) {
    const ParseArg argv[] = {ParseArg(args)...};
    return ParseArgs(str, fmt, argv, (int)sizeof...(TArgs));
}

} // namespace wstr

// fmt() is the type-safe positional/printf-style formatter from StrFormat.h.
// A function-like macro (not a function) so only fmt(...) call syntax is
// rewritten -- identifiers named `fmt` (params, locals like `Format fmt`) are
// untouched.
#define fmt(...) str::FormatTemp(__VA_ARGS__)
