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

Fmt::Fmt(const char *fmt) { ParseFormat(fmt); }

void Fmt::addStr(const char *s, size_t len) {
    CrashIf(nStrings >= MaxArgs);
    strings[nStrings].s = s;
    strings[nStrings].len = len;
    nStrings++;
}

const char *Fmt::parseArgDefPositional(const char *fmt) {
    CrashIf(*fmt != '{');
    ++fmt;
    int n = 0;
    while (*fmt != '}') {
        // TODO: this could be more featurful
        CrashIf(!str::IsDigit(*fmt));
        n = n * 10 + (*fmt - '0');
        ++fmt;
    }
    argDefs[nArgDefs].t = Arg::Any;
    argDefs[nArgDefs].argNo = n;
    ++nArgDefs;
    return fmt + 1;
}

const char *Fmt::parseArgDefPerc(const char *fmt) {
    CrashIf(*fmt != '%');
    // TODO: more features
    argDefs[nArgDefs].t = typeFromChar(fmt[1]);
    argDefs[nArgDefs].argNo = currPercArg;
    ++nArgDefs;
    ++currPercArg;
    return fmt + 2;
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
            return parseArgDefPositional(fmt);
        }
        if ('%' == c) {
            // detect and skip %%
            if ('%' == fmt[1]) {
                fmt += 2;
                continue;
            }
            addStr(start, fmt - start);
            return parseArgDefPerc(fmt);
        }
        ++fmt;
    }
    return NULL;
}

static bool hasArgDefWithNo(Arg *argDefs, int nArgDefs, int no) {
    for (int i = 0; i < nArgDefs; i++) {
        if (argDefs[i].argNo == no) {
            return true;
        }
    }
    return false;
}

Fmt &Fmt::ParseFormat(const char *fmt) {
    nStrings = 0;
    nArgDefs = 0;
    nArgsExpected = 0;
    nArgs = 0;
    currPercArg = 0;
    res.Reset();

    while (*fmt) {
        fmt = parseStr(fmt);
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
        if (!hasArgDefWithNo(argDefs, nArgDefs, i)) {
            CrashIf(true);
        }
    }
    return *this;
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
    if (!validArgTypes(defType, arg.t)) {
        CrashIf(true);
    }
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
}

// TODO: merge into tests\StrFormat_ut.cpp
void RunFmtTests() {
    fmt::Fmt f("int: %d, s: %s");
    const char *s = f.i(5).s("foo").Get();
    CrashIf(!str::Eq(s, "int: 5, s: foo"));
    s = f.ParseFormat("int: {1}, s: {0}").s(L"hello").i(-1).Get();
    CrashIf(!str::Eq(s, "int: -1, s: hello"));
}
