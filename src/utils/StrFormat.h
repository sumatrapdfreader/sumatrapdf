/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/*
str::Format() is similar to C# String.Format(). Instead of using % formatting directives,
we use positional {0}, {1} directives.

It has 2 advantages:
- it's safer (no way to mismatch % directive and the type of the argument)
- it's more flexible. In some languages it's impossible to write a non-awkward
  translation without changing the order of words, which is impossible with %-style formatting

TODO: once this is working and apptranslator.org has ability to rename translations, we should
change at least those translations that involve 2 or more substitutions.
*/

#include "BaseUtil.h"

class StrFormatArg
{
public:
    StrFormatArg();
    StrFormatArg(int);
    explicit StrFormatArg(const char*);
    explicit StrFormatArg(const WCHAR*);

    bool IsNull() const { return tp == TpNone; }

    enum {
        TpNone = 0,
        TpInt = 1,
        TpStr = 2,
        TpWStr = 3
    };

    union {
        int i;
        const char *s;
        const WCHAR *ws;
    };

    int tp;
    static StrFormatArg null;
};

namespace str {

char *Fmt(const char *fmt, const StrFormatArg& a1, const StrFormatArg& a2=StrFormatArg::null,
             const StrFormatArg& a3=StrFormatArg::null, const StrFormatArg& a4=StrFormatArg::null,
             const StrFormatArg& a5=StrFormatArg::null, const StrFormatArg& a6=StrFormatArg::null);

WCHAR *Fmt(const WCHAR *fmt, const StrFormatArg& a1, const StrFormatArg& a2=StrFormatArg::null,
             const StrFormatArg& a3=StrFormatArg::null, const StrFormatArg& a4=StrFormatArg::null,
             const StrFormatArg& a5=StrFormatArg::null, const StrFormatArg& a6=StrFormatArg::null);

}
