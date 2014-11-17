/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "StrFormat.h"

namespace fmt {

static Type typeFromChar(char c) {
    switch (c) {
        case 'c': // char
            return Char;
        case 's': // string or wstring
            return Str;
        case 'd': // integer in base 10
            return Int;
        case 'f': // float or double
            return Float;
    }
    CrashIf(true);
    return Invalid;
}

Fmt::Fmt(const char *fmt) { ParseFormat(fmt); }

void Fmt::addFormatStr(const char *s, size_t len) {
    if (len == 0) {
        return;
    }
    CrashIf(nInst >= MaxInstructions);
    instructions[nInst].t = FormatStr;
    instructions[nInst].argNo = currArgFromFormatNo;
    ++nInst;

    CrashIf(nArgsUsed >= MaxArgs);
    args[currArgFromFormatNo].t = FormatStr;
    args[currArgFromFormatNo].s = s;
    args[currArgFromFormatNo].len = len;
    nArgsUsed++;
    --currArgFromFormatNo;
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
    instructions[nInst].t = Any;
    instructions[nInst].argNo = n;
    ++nInst;
    return fmt + 1;
}

const char *Fmt::parseArgDefPerc(const char *fmt) {
    CrashIf(*fmt != '%');
    // TODO: more features
    instructions[nInst].t = typeFromChar(fmt[1]);
    instructions[nInst].argNo = currPercArgNo;
    ++nInst;
    ++currPercArgNo;
    return fmt + 2;
}

static bool hasInstructionWithArgNo(Inst *insts, int nInst, int argNo) {
    for (int i = 0; i < nInst; i++) {
        if (insts[i].argNo == argNo) {
            return true;
        }
    }
    return false;
}

Fmt &Fmt::ParseFormat(const char *fmt) {
    format = fmt;
    nInst = 0;
    nArgs = 0;
    nArgsUsed = 0;
    maxArgNo = 0;
    currPercArgNo = 0;
    currArgFromFormatNo = dimof(args) - 1; // points at the end
    res.Reset();

    // parse formatting string, until a %$c or {$n}
    // %% is how we escape %, \{ is how we escape {
    const char *start = fmt;
    char c;
    while (*fmt) {
        c = *fmt;
        if ('\\' == c) {
            // handle \{
            if ('{' == fmt[1]) {
                addFormatStr(start, fmt - start);
                start = fmt + 1;
                fmt += 2; // skip '{'
                continue;
            }
            continue;
        }
        if ('{' == c) {
            addFormatStr(start, fmt - start);
            fmt = parseArgDefPositional(fmt);
            start = fmt;
            continue;
        }
        if ('%' == c) {
            // handle %%
            if ('%' == fmt[1]) {
                addFormatStr(start, fmt - start);
                start = fmt + 1;
                fmt += 2; // skip '%'
                continue;
            }
            addFormatStr(start, fmt - start);
            fmt = parseArgDefPerc(fmt);
            start = fmt;
            continue;
        }
        ++fmt;
    }
    addFormatStr(start, fmt - start);

    // check that arg numbers in {$n} makes sense
    for (int i = 0; i < nInst; i++) {
        if (instructions[i].t == FormatStr) {
            continue;
        }
        if (instructions[i].argNo > maxArgNo) {
            maxArgNo = instructions[i].argNo;
        }
    }

    // instructions[i].argNo can be duplicate
    // (we can have positional arg like {0} multiple times
    // but must cover all space from 0..nArgsExpected
    for (int i = 0; i <= maxArgNo; i++) {
        isOk = hasInstructionWithArgNo(instructions, nInst, i);
        CrashIf(!isOk);
        if (!isOk) {
            return *this;
        }
    }
    return *this;
}

Fmt &Fmt::addArgType(Type t) {
    CrashIf(nArgsUsed >= MaxArgs);
    args[nArgs].t = t;
    nArgs++;
    nArgsUsed++;
    return *this;
}

Fmt &Fmt::i(int i) {
    args[nArgs].i = i;
    return addArgType(Int);
}

Fmt &Fmt::s(const char *s) {
    args[nArgs].s = s;
    return addArgType(Str);
}

Fmt &Fmt::s(const WCHAR *s) {
    args[nArgs].ws = s;
    return addArgType(WStr);
}

Fmt &Fmt::c(char c) {
    args[nArgs].c = c;
    return addArgType(Char);
}

Fmt &Fmt::f(float f) {
    args[nArgs].f = f;
    return addArgType(Float);
}

Fmt &Fmt::f(double d) {
    args[nArgs].d = d;
    return addArgType(Double);
}

static bool validArgTypes(Type instType, Type argType) {
    if (instType == Any) {
        return true;
    }
    if (instType == FormatStr) {
        return argType == FormatStr;
    }
    if (instType == Char) {
        return argType == Char;
    }
    if (instType == Int) {
        return argType == Int;
    }
    if (instType == Float) {
        return argType == Float || argType == Double;
    }
    if (instType == Str) {
        return argType == Str || argType == WStr;
    }
    return false;
}

void Fmt::serializeInst(int n) {
    CrashIf(n >= dimof(instructions));
    CrashIf(n >= nInst);

    Type tInst = instructions[n].t;
    int argNo = instructions[n].argNo;
    Arg arg = args[argNo];
    Type tArg = arg.t;
    isOk = validArgTypes(tInst, tArg);
    CrashIf(!isOk);
    if (!isOk) {
        return;
    }
    switch (tArg) {
        case Char:
            res.Append(arg.c);
            break;
        case Int:
            // TODO: using AppendFmt is cheating
            res.AppendFmt("%d", arg.i);
            break;
        case Float:
            // TODO: using AppendFmt is cheating
            // Note: %G, unlike %f, avoid trailing '0'
            res.AppendFmt("%G", arg.f);
            break;
        case Double:
            // TODO: using AppendFmt is cheating
            // Note: %G, unlike %f, avoid trailing '0'
            res.AppendFmt("%G", arg.d);
            break;
        case FormatStr:
            CrashIf(arg.len == 0);
            res.Append(arg.s, arg.len);
            break;
        case Str:
            res.Append(arg.s);
            break;
        case WStr:
            char *sUtf8 = str::conv::ToUtf8(arg.ws);
            res.AppendAndFree(sUtf8);
            break;
    };
}

char *Fmt::Get() {
    CrashIf(nArgs != maxArgNo + 1);
    for (int i = 0; isOk && i < nInst; i++) {
        serializeInst(i);
    }
    if (!isOk) {
        res.Reset();
        res.Append("Mismatch format/args for format string: ");
        res.Append(format);
    }
    return res.Get();
}

char *Fmt::GetDup() { return str::Dup(Get()); }
}
