/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/*
fmt::Fmt is type-safe printf()-like system with support for both %-style formatting
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
fmt::Fmt fmt("%d = %s");
char *s = fmt.i(5).s("5").Get(); // returns "5 = 5"
// s is valid until fmt is valid
// use .GetDup() to get a copy that must be free()d
// you can re-use fmt as:
s = fmt.ParseFormat("{1} = {2} + {0}").i(3).s("3").s(L"

You can mix % and {} directives but beware, as the rule for assigning argument
number to % directive is simple (n-th argument position for n-th % directive)
but it's easy to mis-count when adding {} to the mix.

*/

namespace fmt {

enum class Type {
    // concrete types for Arg
    Char,
    Int,
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
    union {
        char c;
        i64 i;
        float f;
        double d;
        const char* s;
        const WCHAR* ws;
    } u{};

    Arg() = default;

    Arg(char c) {
        t = Type::Char;
        u.c = c;
    }

    Arg(int arg) {
        t = Type::Int;
        u.i = (i64)arg;
    }

    Arg(size_t arg) {
        t = Type::Int;
        u.i = (i64)arg;
    }

    Arg(i64 arg) {
        t = Type::Int;
        u.i = arg;
    }

    Arg(float f) {
        t = Type::Float;
        u.f = f;
    }

    Arg(double d) {
        t = Type::Double;
        u.d = d;
    }

    Arg(const char* arg) {
        t = Type::Str;
        u.s = arg;
    }

    Arg(const WCHAR* arg) {
        t = Type::WStr;
        u.ws = arg;
    }
};

char* Format(const char* s, const Arg& a1 = Arg(), const Arg& a2 = Arg(), const Arg& a3 = Arg(), const Arg& a4 = Arg(),
             const Arg& a5 = Arg(), const Arg& a6 = Arg());

char* FormatTemp(const char* s, const Arg);
char* FormatTemp(const char* s, const Arg, const Arg);
char* FormatTemp(const char* s, const Arg, const Arg, const Arg);

} // namespace fmt
