/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/*
fmt::Fmt is similar to C# String.Format() i.e. we use positional {0}, {1}
formatting directives.

For backwards compatiblity, we also support %-style formatting directives.

Poistional args have 2 advantages:
- it's safer (no way to mismatch % directive and the type of the argument)
- it's more flexible. In some languages it's impossible to write a non-awkward
  translation without changing the order of words, which is impossible with %-style formatting

When using %-style formatting, we also verify the type of the argument is right.

TODO: once this is working and apptranslator.org has ability to rename translations, we should
change at least those translations that involve 2 or more substitutions.

TODO: similar approach could be used for type-safe scanf() replacement.
*/

namespace fmt {

enum { MaxArgs = 16 };

struct FmtStr {
    const char *s;
    size_t len;
};

// both a definition of the expected type and argument value
struct Arg {
    enum Type {
        Char,
        Int,
        Float,
        Double,
        Str,
        WStr,
        Any,
        Invalid,
    };
    Type t;
    int argNo;
    union {
        char c;
        int i;
        float f;
        double d;
        const char *s;
        const WCHAR *ws;
    };
};

class Fmt {
  public:
    Fmt(const char *fmt);
    Fmt &i(int);
    Fmt &s(const char *);
    Fmt &s(const WCHAR *);
    Fmt &c(char);
    Fmt &f(float);
    Fmt &f(double);
    char *Get();
    char *GetDup();

    void parseFormat(const char *fmt);
    const char *parseStr(const char *fmt);
    const char *parseArg(const char *fmt);
    void addStr(const char *s, size_t len);
    void addArg(const char *s, size_t len);
    void serializeArg(int argDefNo);

    // when "foo {0} bar %d" is parsed,
    // strings has "foo ", " bar "
    // argDefs has "{0}" and "%d"
    FmtStr strings[MaxArgs];
    Arg argDefs[MaxArgs];
    Arg args[MaxArgs];
    bool isOk;
    int nStrings;
    int nArgDefs;
    int nArgsExpected;
    int nArgs;
    int currPercArg;
    str::Str<char> res;
};
}

void RunFmtTests();
