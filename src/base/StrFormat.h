/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#pragma once

namespace strfmt {

enum class Type {
    // concrete types for Arg
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

// argument to a formatting instruction
// at the front are arguments given with i(), s() etc.
// at the end are FormatStr arguments from format string
struct Arg {
    Type t{Type::None};
    // An Arg only ever holds one value, selected by t, so the value fields share
    // storage. Str/WStr are trivially copyable (only their default ctor is
    // non-trivial), so they are valid union members; the union has no implicit
    // default ctor, hence Arg() initializes a trivial member explicitly.
    union {
        Str str;
        WStr wstr;
        char c;
        i64 i;
        float f;
        double d;
        const void* ptr;
    };

    Arg() : i{0} {} // t stays None; init the union via a trivial member

    // All single-argument constructors are explicit so values never implicitly
    // convert to Arg (e.g. a stray pointer or bool). FormatTemp() is a variadic
    // template that constructs each Arg explicitly, so call sites stay terse.

    explicit Arg(char c_) : t{Type::Char}, c{c_} {}

    // integer family: a constructor per fundamental type so e.g. unsigned int
    // (UINT/DWORD), unsigned long, size_t etc. are not ambiguous between
    // Arg(int)/Arg(i64)/Arg(size_t). The conversion char (%d vs %u vs %x)
    // decides the rendering; we just carry the value as i64.
    explicit Arg(int arg) : t{Type::Int}, i{(i64)arg} {}
    explicit Arg(unsigned int arg) : t{Type::Int}, i{(i64)arg} {}
    explicit Arg(long arg) : t{Type::Int}, i{(i64)arg} {}
    explicit Arg(unsigned long arg) : t{Type::Int}, i{(i64)arg} {}
    explicit Arg(long long arg) : t{Type::Int}, i{(i64)arg} {}
    explicit Arg(unsigned long long arg) : t{Type::Int}, i{(i64)arg} {}

    explicit Arg(float f_) : t{Type::Float}, f{f_} {}

    explicit Arg(double d_) : t{Type::Double}, d{d_} {}

    explicit Arg(Str arg) : t{Type::Str}, str{arg} {}

    explicit Arg(WStr arg) : t{Type::WStr}, wstr{arg} {}

    explicit Arg(const void* p) : t{Type::Ptr}, ptr{p} { // for %p
    }

    // raw C strings are not allowed: pass Str / WStr explicitly so we never
    // depend on a NUL-terminated char*/wchar_t*
    Arg(char*) = delete;
    Arg(const char*) = delete;
    Arg(wchar_t*) = delete;
    Arg(const wchar_t*) = delete;
};

TempStr FormatTempArgs(const char* fmt, const Arg** args, int nArgs);

inline TempStr FormatTemp(const char* fmt) {
    return FormatTempArgs(fmt, nullptr, 0);
}

template <typename... TArgs>
TempStr FormatTemp(const char* fmt, const TArgs&... args) {
    const Arg argv[] = {Arg(args)...};
    const Arg* argp[sizeof...(TArgs)];
    int n = (int)sizeof...(TArgs);
    for (int i = 0; i < n; i++) {
        argp[i] = &argv[i];
    }
    return FormatTempArgs(fmt, argp, n);
}

} // namespace strfmt

// fmt() is the type-safe positional/printf-style formatter from StrFormat.h.
// A function-like macro (not a function) so only fmt(...) call syntax is
// rewritten -- identifiers named `fmt` (params, locals like `Format fmt`) are
// untouched.
#define fmt(...) strfmt::FormatTemp(__VA_ARGS__)
