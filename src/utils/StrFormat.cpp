/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "StrFormat.h"

namespace fmt {

static Arg::Type typeFromChar(char c) {
    switch (c) {
        case 'c': // char
            return Arg::Char;
        case 's': // string or wstring
            return Arg::Str;
        case 'd': // integer in base 10
            return Arg::Int;
        case 'f': // float or double
            return Arg::Float;
    }
    CrashIf(true);
    return Arg::Invalid;
}

Fmt::Fmt(const char *fmt) {
    isOk = true;
    nStrings = 0;
    nArgDefs = 0;
    nArgsExpected = 0;
    nArgs = 0;
    currPercArg = 0;
    parseFormat(fmt);
}

void Fmt::addStr(const char *s, size_t len) {
    CrashIf(nStrings >= MaxArgs);
    strings[nStrings].s = s;
    strings[nStrings].len = len;
    nStrings++;
}

void Fmt::addArg(const char *s, size_t len) {
    CrashIf(nArgDefs >= MaxArgs);
    if (*s == '{') {
        CrashIf(s[len - 1] != '}');
        argDefs[nArgDefs].t = Arg::Any;
        // TODO: extract number between { } and set as argDefs[nArgs].argNo
        CrashIf(true);
    } else if (*s == '%') {
        argDefs[nArgDefs].t = typeFromChar(s[1]);
        argDefs[nArgDefs].argNo = currPercArg;
        ++currPercArg;
    } else {
        CrashIf(true);
    }
    ++nArgDefs;
}

// get until a %$c or {$n}
// %% is how we escape %, \{ is how we escape {
const char *Fmt::parseStr(const char *fmt) {
    const char *start = fmt;
    char c;
    while (*fmt) {
        c = *fmt;
        if ('\\' == c) {
            // detect and skip \{
            if ('{' == fmt[1]) {
                fmt += 2;
                continue;
            }
            continue;
        }
        if ('{' == c) {
            addStr(start, fmt - start);
            return fmt;
        }
        if ('%' == c) {
            // detect and skip %%
            if ('%' == fmt[1]) {
                fmt += 2;
                continue;
            }
            addStr(start, fmt - start);
            return fmt;
        }
        ++fmt;
    }
    return NULL;
}

const char *Fmt::parseArg(const char *fmt) {
    const char *start = fmt;
    if ('{' == *fmt) {
        ++fmt;
        while (*fmt != '}') {
            // TODO: this could be more featurful
            CrashIf(0 == *fmt);
            CrashIf(!str::IsDigit(*fmt));
            ++fmt;
        }
        ++fmt;
        addArg(fmt, start - fmt);
    } else if ('%' == *fmt) {
        // TODO: this assumes only a single-letter format like %s
        ++fmt;
        ++fmt;
        addArg(start, start - fmt);
    } else {
        CrashIf(true);
    }
    return fmt;
}

static bool hasArgDefWithNo(Arg *argDefs, int nArgDefs, int no) {
    for (int i = 0; i < nArgDefs; i++) {
        if (argDefs[i].argNo == no) {
            return true;
        }
    }
    return false;
}

void Fmt::parseFormat(const char *fmt) {
    while (*fmt) {
        fmt = parseStr(fmt);
        fmt = parseArg(fmt);
    }
    // check that arg numbers in {$n} makes sense
    for (int i = 0; i < nArgDefs; i++) {
        if (argDefs[i].argNo > nArgsExpected) {
            nArgsExpected = argDefs[i].argNo;
        }
    }
    // we must have argDefs[i].argNo can be duplicate
    // (we can have positional arg like {0} multiple times
    // but must cover all space from 0..nArgsExpected
    for (int i = 0; i <= nArgsExpected; i++) {
        CrashIf(!hasArgDefWithNo(argDefs, nArgDefs, i));
    }
}

Fmt &Fmt::i(int i) {
    CrashIf(nArgs >= MaxArgs);
    args[nArgs].t = Arg::Int;
    args[nArgs].i = i;
    nArgs++;
    return *this;
}

Fmt &Fmt::s(const char *s) {
    CrashIf(nArgs >= MaxArgs);
    args[nArgs].t = Arg::Str;
    args[nArgs].s = s;
    nArgs++;
    return *this;
}

Fmt &Fmt::s(const WCHAR *s) {
    CrashIf(nArgs >= MaxArgs);
    args[nArgs].t = Arg::WStr;
    args[nArgs].ws = s;
    nArgs++;
    return *this;
}

Fmt &Fmt::c(char c) {
    CrashIf(nArgs >= MaxArgs);
    args[nArgs].t = Arg::Char;
    args[nArgs].c = c;
    nArgs++;
    return *this;
}

Fmt &Fmt::f(float f) {
    CrashIf(nArgs >= MaxArgs);
    args[nArgs].t = Arg::Float;
    args[nArgs].f = f;
    nArgs++;
    return *this;
}

Fmt &Fmt::f(double d) {
    CrashIf(nArgs >= MaxArgs);
    args[nArgs].t = Arg::Double;
    args[nArgs].d = d;
    nArgs++;
    return *this;
}

static bool validArgTypes(Arg::Type defType, Arg::Type argType) {
    if (defType == Arg::Any) {
        return true;
    }
    if (defType == Arg::Char) {
        return argType == Arg::Char;
    }
    if (defType == Arg::Int) {
        return argType == Arg::Int;
    }
    if (defType == Arg::Float) {
        return argType == Arg::Float || argType == Arg::Double;
    }
    if (defType == Arg::Str) {
        return argType == Arg::Str || argType == Arg::WStr;
    }
    return false;
}

void Fmt::serializeArg(int argDefNo) {
    if (argDefNo >= nArgDefs) {
        return;
    }

    int argNo = argDefs[argDefNo].argNo;
    Arg::Type defType = argDefs[argDefNo].t;
    Arg arg = args[argNo];
    CrashIf(!validArgTypes(defType, arg.t));
    switch (arg.t) {
        case Arg::Char:
            res.Append(arg.c);
            break;
        case Arg::Int:
            // TODO: cheating
            res.AppendFmt("%d", arg.i);
            break;
        case Arg::Float:
        case Arg::Double:
            // TODO: NYI
            CrashIf(true);
            break;
        case Arg::Str:
            res.Append(arg.s);
            break;
        case Arg::WStr:
            char *sUtf8 = str::conv::ToUtf8(arg.ws);
            res.AppendAndFree(sUtf8);
            break;
    };
}

char *Fmt::Get() {
    CrashIf(nArgs != nArgsExpected + 1);
    for (int i = 0; i < nStrings; i++) {
        res.Append(strings[i].s, strings[i].len);
        serializeArg(i);
    }
    return res.Get();
}

char *Fmt::GetDup() { return str::Dup(Get()); }
};

void RunFmtTests() {
    fmt::Fmt f("int: %d, s: %s");
    const char *s = f.i(5).s("foo").Get();
    CrashIf(!str::Eq(s, "int: 5, s: foo"));
}

#if 0
namespace str {

// parse '{$n}' part. Returns true if string follows this pattern
// and false if not.
// If returns true, sInOut is repositioned after '}'
static bool ParsePositional(const char **sInOut, int *nOut) {
    const char *s = *sInOut;
    CrashIf(*s != '{');
    s++;
    int n = 0;
    while (IsDigit(*s)) {
        n = n * 10 + (*s - '0');
        ++s;
    }
    if (s == *sInOut)
        return false;
    if (*s != '}')
        return false;
    s++;
    *sInOut = s;
    *nOut = n;
    return true;
}

}
#endif
