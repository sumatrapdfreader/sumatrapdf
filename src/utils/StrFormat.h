/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
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

// arbitrary limits, but should be large enough for us
// the bigger the limit, the bigger the sizeof(Fmt)
enum { MaxInstructions = 32 };
enum {
    // more than MaxInstructions for positional args
    MaxArgs = 32 + 8
};

enum class Type {
    FormatStr, // from format string
    Char,
    Int,
    Float,
    Double,
    Str,
    WStr,
    Any,
    Empty,
};

// formatting instruction
struct Inst {
    Type t;
    int argNo; // <0 for strings that come from formatting string
};

// argument to a formatting instruction
// at the front are arguments given with i(), s() etc.
// at the end are FormatStr arguments from format string
struct Arg {
    Type t{Type::Empty};
    union {
        char c;
        int i;
        float f;
        double d;
        std::string_view sv;
        std::wstring_view wsv;
    } u = {0};

    Arg() = default;

    Arg(char c) {
        t = Type::Char;
        u.c = c;
    }

    Arg(int arg) {
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

    Arg(std::string_view arg) {
        t = Type::Str;
        u.sv = arg;
    }

    Arg(const char* arg) {
        t = Type::Str;
        u.sv = arg;
    }

    Arg(std::wstring_view arg) {
        t = Type::WStr;
        u.wsv = arg;
    }

    Arg(const WCHAR* arg) {
        t = Type::WStr;
        u.wsv = arg;
    }
};

class Fmt {
  public:
    explicit Fmt(const char* fmt);
    Fmt& i(int);
    Fmt& s(const char*);
    Fmt& s(const WCHAR*);
    Fmt& c(char);
    Fmt& f(float);
    Fmt& f(double);

    Fmt& ParseFormat(const char* fmt);
    Fmt& Reset();
    char* Get();
    char* GetDup();

    bool isOk{false}; // true if mismatch between formatting instruction and args

    DWORD threadId;
    const char* format;
    Inst instructions[MaxInstructions];
    int nInst;
    Arg args[MaxArgs];
    int nArgs;
    int nArgsUsed;
    int maxArgNo;
    int currPercArgNo;
    int currArgFromFormatNo; // counts from the end of args
    str::Str res;
};

std::string_view Format(const char* s, const Arg& a1);
std::string_view Format(const char* s, const Arg& a1, const Arg& a2);
std::string_view Format(const char* s, const Arg& a1, const Arg& a2, const Arg& a3);
} // namespace fmt
