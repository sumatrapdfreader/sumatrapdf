/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#pragma once

/*
strfmt::Fmt is type-safe printf()-like system with support for both %-style formatting
directives and C#-like positional directives ({0}, {1} etc.).

Type safety is achieved by using strongly typed methods for adding arguments
(i(), c(), s() etc.). We also verify that the type of the argument matches
the type of formatting directive.

Positional directives are useful in translations with more than 1 argument
because in some languages translation is akward if you can't re-arrange
the order of arguments.

TODO: we should change at least those translations that involve 2 or more arguments.

TODO: ultimately we shouldn't use sprintf() in the implementation, so that we are
sure what the semantics are and they are always the same (independent of platform).
We should serialize all values ourselves.

TODO: similar approach could be used for type-safe scanf() replacement.

Idiomatic usage:
strfmt::Fmt fmt("%d = %s");
char *s = fmt.i(5).s("5").Get(); // returns "5 = 5"
// s is valid until fmt is valid
// use .GetDup() to get a copy that must be free()d
// you can re-use fmt as:
s = fmt.ParseFormat("{1} = {2} + {0}").i(3).s("3").s(L"

You can mix % and {} directives but beware, as the rule for assigning argument
number to % directive is simple (n-th argument position for n-th % directive)
but it's easy to mis-count when adding {} to the mix.

*/

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
    Str str;
    WStr wstr;
    char c = 0;
    i64 i = 0;
    float f = 0;
    double d = 0;
    const void* ptr = nullptr;

    Arg() = default;

    // All single-argument constructors are explicit so values never implicitly
    // convert to Arg (e.g. a stray pointer or bool). FormatTemp() is a variadic
    // template that constructs each Arg explicitly, so call sites stay terse.

    explicit Arg(char c_) {
        t = Type::Char;
        c = c_;
    }

    // integer family: a constructor per fundamental type so e.g. unsigned int
    // (UINT/DWORD), unsigned long, size_t etc. are not ambiguous between
    // Arg(int)/Arg(i64)/Arg(size_t). The conversion char (%d vs %u vs %x)
    // decides the rendering; we just carry the value as i64.
    explicit Arg(int arg) {
        t = Type::Int;
        i = (i64)arg;
    }
    explicit Arg(unsigned int arg) {
        t = Type::Int;
        i = (i64)arg;
    }
    explicit Arg(long arg) {
        t = Type::Int;
        i = (i64)arg;
    }
    explicit Arg(unsigned long arg) {
        t = Type::Int;
        i = (i64)arg;
    }
    explicit Arg(long long arg) {
        t = Type::Int;
        i = (i64)arg;
    }
    explicit Arg(unsigned long long arg) {
        t = Type::Int;
        i = (i64)arg;
    }

    explicit Arg(float f_) {
        t = Type::Float;
        f = f_;
    }

    explicit Arg(double d_) {
        t = Type::Double;
        d = d_;
    }

    explicit Arg(Str arg) {
        t = Type::Str;
        str = arg;
    }

    explicit Arg(WStr arg) {
        t = Type::WStr;
        wstr = arg;
    }

    explicit Arg(const void* p) { // for %p
        t = Type::Ptr;
        ptr = p;
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