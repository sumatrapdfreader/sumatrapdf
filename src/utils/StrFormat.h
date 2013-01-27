/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/*
str::Format() is similar to C# String.Format() i.e. we use positional {0}, {1}
formatting directives.

For backwards compatiblity, we also support %-style formatting directives.

Poistional args have 2 advantages:
- it's safer (no way to mismatch % directive and the type of the argument)
- it's more flexible. In some languages it's impossible to write a non-awkward
  translation without changing the order of words, which is impossible with %-style formatting

When using %-style formatting, we also verify the type of the argument is right.

Mixing %-style and {0}-style formatting is not supported.

TODO: once this is working and apptranslator.org has ability to rename translations, we should
change at least those translations that involve 2 or more substitutions.
*/

namespace str {

class Arg
{
public:
    enum {
        None = 0,
        Int = 1,
        Str = 2,
        WStr = 3
    };

    Arg();
    Arg(int);
    explicit Arg(const char*);
    explicit Arg(const WCHAR*);

    bool IsNull() const { return tp == None; }

    union {
        int i;
        const char *s;
        const WCHAR *ws;
    };

    int tp;
    static Arg null;
};

char *Fmt(const char *fmt, const Arg& a0, const Arg& a1=Arg::null,
             const Arg& a2=Arg::null, const Arg& a3=Arg::null,
             const Arg& a4=Arg::null, const Arg& a5=Arg::null);

WCHAR *Fmt(const WCHAR *fmt, const Arg& a0, const Arg& a1=Arg::null,
             const Arg& a2=Arg::null, const Arg& a3=Arg::null,
             const Arg& a4=Arg::null, const Arg& a5=Arg::null);

}
