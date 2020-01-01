/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
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
    Invalid,
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
    Type t = Type::Invalid;
    size_t len = 0; // for s when FormatStr
    union {
        char c;
        int i;
        float f;
        double d;
        const char* s;
        const WCHAR* ws;
    };
    Arg() = default;
    Arg(int arg) {
        t = Type::Int;
        i = arg;
    }
    Arg(const char* arg) {
        t = Type::Str;
        s = arg;
    }
    Arg(const WCHAR* arg) {
        t = Type::WStr;
        ws = arg;
    }
};

class Fmt {
  public:
    Fmt(const char* fmt);
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

    bool isOk; // true if mismatch between formatting instruction and args

  private:
    const char* parseArgDef(const char* fmt);
    void addFormatStr(const char* s, size_t len);
    const char* parseArgDefPerc(const char*);
    const char* parseArgDefPositional(const char*);
    Fmt& addArgType(Type t);
    void serializeInst(int n);

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

std::string_view Format(const char* s, Arg& a1);
std::string_view Format(const char* s, Arg& a1, Arg& a2);
std::string_view Format(const char* s, Arg& a1, Arg& a2, Arg& a3);
} // namespace fmt
