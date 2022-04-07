/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "StrFormat.h"

namespace fmt {

static Type typeFromChar(char c) {
    switch (c) {
        case 'c': // char
            return Type::Char;
        case 'd': // integer in base 10
            return Type::Int;
        case 'f': // float or double
            return Type::Float;
        case 's': // string or wstring
            return Type::Str;
    }
    CrashIf(true);
    return Type::None;
}

static void addFormatStr(Fmt& fmt, const char* s, size_t len) {
    if (len == 0) {
        return;
    }
    CrashIf(fmt.nInst >= dimof(fmt.instructions));
    fmt.instructions[fmt.nInst].t = Type::FormatStr;
    fmt.instructions[fmt.nInst].sv = {s, len};
    fmt.instructions[fmt.nInst].argNo = -1;
    ++fmt.nInst;
}

// parse: {$n}
static const char* parseArgDefPositional(Fmt& fmt, const char* s) {
    CrashIf(*s != '{');
    ++s;
    int n = 0;
    while (*s != '}') {
        // TODO: this could be more featurful
        CrashIf(!str::IsDigit(*s));
        n = n * 10 + (*s - '0');
        ++s;
    }
    fmt.instructions[fmt.nInst].t = Type::Any;
    fmt.instructions[fmt.nInst].argNo = n;
    ++fmt.nInst;
    return s + 1;
}

// parse: %[csfd]
static const char* parseArgDefPerc(Fmt& fmt, const char* s) {
    CrashIf(*s != '%');
    // TODO: more features
    fmt.instructions[fmt.nInst].t = typeFromChar(s[1]);
    fmt.instructions[fmt.nInst].argNo = fmt.currPercArgNo++;
    ++fmt.nInst;
    return s + 2;
}

static bool hasInstructionWithArgNo(Inst* insts, int nInst, int argNo) {
    for (int i = 0; i < nInst; i++) {
        if (insts[i].argNo == argNo) {
            return true;
        }
    }
    return false;
}

static bool validArgTypes(Type instType, Type argType) {
    if (instType == Type::Any || instType == Type::FormatStr) {
        return true;
    }
    if (instType == Type::Char) {
        return argType == Type::Char;
    }
    if (instType == Type::Int) {
        return argType == Type::Int;
    }
    if (instType == Type::Float) {
        return argType == Type::Float || argType == Type::Double;
    }
    if (instType == Type::Str) {
        return argType == Type::Str || argType == Type::WStr;
    }
    return false;
}

static bool ParseFormat(Fmt& o, const char* fmt) {
    o.format = fmt;
    o.nInst = 0;
    o.currPercArgNo = 0;
    o.currArgNo = 0;
    o.res.Reset();

    // parse formatting string, until a %$c or {$n}
    // %% is how we escape %, \{ is how we escape {
    const char* start = fmt;
    char c;
    while (*fmt) {
        c = *fmt;
        if ('\\' == c) {
            // handle \{
            if ('{' == fmt[1]) {
                addFormatStr(o, start, fmt - start);
                start = fmt + 1;
                fmt += 2; // skip '{'
                continue;
            }
            continue;
        }
        if ('{' == c) {
            addFormatStr(o, start, fmt - start);
            fmt = parseArgDefPositional(o, fmt);
            start = fmt;
            continue;
        }
        if ('%' == c) {
            // handle %%
            if ('%' == fmt[1]) {
                addFormatStr(o, start, fmt - start);
                start = fmt + 1;
                fmt += 2; // skip '%'
                continue;
            }
            addFormatStr(o, start, fmt - start);
            fmt = parseArgDefPerc(o, fmt);
            start = fmt;
            continue;
        }
        ++fmt;
    }
    addFormatStr(o, start, fmt - start);

    int maxArgNo = o.currArgNo;
    // check that arg numbers in {$n} makes sense
    for (int i = 0; i < o.nInst; i++) {
        if (o.instructions[i].t == Type::FormatStr) {
            continue;
        }
        if (o.instructions[i].argNo > maxArgNo) {
            maxArgNo = o.instructions[i].argNo;
        }
    }

    // instructions[i].argNo can be duplicate
    // (we can have positional arg like {0} multiple times
    // but must cover all space from 0..nArgsExpected
    for (int i = 0; i <= maxArgNo; i++) {
        bool isOk = hasInstructionWithArgNo(o.instructions, o.nInst, i);
        CrashIf(!isOk);
        if (!isOk) {
            return false;
        }
    }
    return true;
}

Fmt::Fmt(const char* fmt) {
    isOk = ParseFormat(*this, fmt);
}

std::string_view Fmt::Eval(const Arg** args, int nArgs) {
    if (!isOk) {
        // if failed parsing format
        return {};
    }

    for (int n = 0; n < nInst; n++) {
        CrashIf(n >= dimof(instructions));
        CrashIf(n >= nInst);

        auto& inst = instructions[n];
        int argNo = inst.argNo;
        CrashIf(argNo >= nArgs);
        if (argNo >= nArgs) {
            isOk = false;
            return {};
        }

        if (inst.t == Type::FormatStr) {
            res.AppendView(inst.sv);
            continue;
        }

        const Arg& arg = *args[argNo];
        isOk = validArgTypes(inst.t, arg.t);
        CrashIf(!isOk);
        if (!isOk) {
            return {};
        }

        switch (arg.t) {
            case Type::Char:
                res.AppendChar(arg.u.c);
                break;
            case Type::Int:
                // TODO: using AppendFmt is cheating
                res.AppendFmt("%d", (int)arg.u.i);
                break;
            case Type::Float:
                // TODO: using AppendFmt is cheating
                // Note: %G, unlike %f, avoid trailing '0'
                res.AppendFmt("%G", arg.u.f);
                break;
            case Type::Double:
                // TODO: using AppendFmt is cheating
                // Note: %G, unlike %f, avoid trailing '0'
                res.AppendFmt("%G", arg.u.d);
                break;
            case Type::Str:
                res.AppendView(arg.u.sv);
                break;
            case Type::WStr:
                auto s = ToUtf8Temp(arg.u.wsv);
                res.AppendView(s.AsView());
                break;
        };
    }
    return res.StealAsView();
}

std::string_view Format(const char* s, const Arg& a1, const Arg& a2, const Arg& a3, const Arg& a4, const Arg& a5,
                        const Arg& a6) {
    const Arg* args[6];
    int nArgs = 0;
    args[nArgs++] = &a1;
    args[nArgs++] = &a2;
    args[nArgs++] = &a3;
    args[nArgs++] = &a4;
    args[nArgs++] = &a5;
    args[nArgs++] = &a6;
    CrashIf(nArgs > dimof(args));
    // arguments at the end could be empty
    while (nArgs >= 0 && args[nArgs - 1]->t == Type::None) {
        nArgs--;
    }

    if (nArgs == 0) {
        // TODO: verify that format has no references to args
        return {s};
    }

    Fmt fmt(s);
    auto res = fmt.Eval(args, nArgs);
    return res;
}

} // namespace fmt
