/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "StrFormat.h"

namespace fmt {

static Type typeFromChar(char c) {
    switch (c) {
        case 'c': // char
            return Type::Char;
        case 's': // string or wstring
            return Type::Str;
        case 'd': // integer in base 10
            return Type::Int;
        case 'f': // float or double
            return Type::Float;
    }
    CrashIf(true);
    return Type::Empty;
}

Fmt::Fmt(const char* fmt) {
    threadId = GetCurrentThreadId();
    ParseFormat(fmt);
}

/*
void addFormatStr(const char* s, size_t len);
const char* parseArgDefPerc(const char*);
const char* parseArgDefPositional(const char*);
Fmt& addArgType(Type t);
void serializeInst(int n);
*/

static void addFormatStr(Fmt& fmt, const char* s, size_t len) {
    if (len == 0) {
        return;
    }
    CrashIf(fmt.nInst >= MaxInstructions);
    fmt.instructions[fmt.nInst].t = Type::FormatStr;
    fmt.instructions[fmt.nInst].argNo = fmt.currArgFromFormatNo;
    ++fmt.nInst;

    CrashIf(fmt.nArgsUsed >= MaxArgs);
    fmt.args[fmt.currArgFromFormatNo].t = Type::FormatStr;
    fmt.args[fmt.currArgFromFormatNo].u.sv = {s, len};
    fmt.nArgsUsed++;
    --fmt.currArgFromFormatNo;
}

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

static const char* parseArgDefPerc(Fmt& fmt, const char* s) {
    CrashIf(*s != '%');
    // TODO: more features
    fmt.instructions[fmt.nInst].t = typeFromChar(s[1]);
    fmt.instructions[fmt.nInst].argNo = fmt.currPercArgNo;
    ++fmt.nInst;
    ++fmt.currPercArgNo;
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

// as an optimization, we can re-use object by calling ParseFormat() only once
// and then using Reset() to restore the output
Fmt& Fmt::Reset() {
    CrashIf(threadId != GetCurrentThreadId()); // check no cross-thread use
    CrashIf(format != nullptr);
    res.Reset();
    return *this;
}

Fmt& Fmt::ParseFormat(const char* fmt) {
    // we can use Fmt in an optimized way by having only one global instance per
    // thread or an instance for a given format expression. To make that a bit
    // safer, we check that they are not used cross-thread
    CrashIf(threadId != GetCurrentThreadId());

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
    const char* start = fmt;
    char c;
    while (*fmt) {
        c = *fmt;
        if ('\\' == c) {
            // handle \{
            if ('{' == fmt[1]) {
                addFormatStr(*this, start, fmt - start);
                start = fmt + 1;
                fmt += 2; // skip '{'
                continue;
            }
            continue;
        }
        if ('{' == c) {
            addFormatStr(*this, start, fmt - start);
            fmt = parseArgDefPositional(*this, fmt);
            start = fmt;
            continue;
        }
        if ('%' == c) {
            // handle %%
            if ('%' == fmt[1]) {
                addFormatStr(*this, start, fmt - start);
                start = fmt + 1;
                fmt += 2; // skip '%'
                continue;
            }
            addFormatStr(*this, start, fmt - start);
            fmt = parseArgDefPerc(*this, fmt);
            start = fmt;
            continue;
        }
        ++fmt;
    }
    addFormatStr(*this, start, fmt - start);

    // check that arg numbers in {$n} makes sense
    for (int i = 0; i < nInst; i++) {
        if (instructions[i].t == Type::FormatStr) {
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

void addArg(Fmt& fmt, const Arg& arg) {
    if (arg.t == Type::Empty) {
        return;
    }
    fmt.args[fmt.nArgs] = arg;
    fmt.nArgs++;
}

Fmt& Fmt::i(int i) {
    addArg(*this, Arg(i));
    return *this;
}

Fmt& Fmt::s(const char* s) {
    addArg(*this, Arg(s));
    return *this;
}

Fmt& Fmt::s(const WCHAR* s) {
    addArg(*this, Arg(s));
    return *this;
}

Fmt& Fmt::c(char c) {
    addArg(*this, Arg(c));
    return *this;
}

Fmt& Fmt::f(float f) {
    addArg(*this, Arg(f));
    return *this;
}

Fmt& Fmt::f(double d) {
    addArg(*this, Arg(d));
    return *this;
}

static bool validArgTypes(Type instType, Type argType) {
    if (instType == Type::Any) {
        return true;
    }
    if (instType == Type::FormatStr) {
        return argType == Type::FormatStr;
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

static void serializeInst(Fmt& fmt, int n) {
    CrashIf(n >= dimof(fmt.instructions));
    CrashIf(n >= fmt.nInst);

    Type tInst = fmt.instructions[n].t;
    int argNo = fmt.instructions[n].argNo;
    Arg arg = fmt.args[argNo];
    Type tArg = arg.t;
    fmt.isOk = validArgTypes(tInst, tArg);
    CrashIf(!fmt.isOk);
    if (!fmt.isOk) {
        return;
    }
    switch (tArg) {
        case Type::Char:
            fmt.res.AppendChar(arg.u.c);
            break;
        case Type::Int:
            // TODO: using AppendFmt is cheating
            fmt.res.AppendFmt("%d", arg.u.i);
            break;
        case Type::Float:
            // TODO: using AppendFmt is cheating
            // Note: %G, unlike %f, avoid trailing '0'
            fmt.res.AppendFmt("%G", arg.u.f);
            break;
        case Type::Double:
            // TODO: using AppendFmt is cheating
            // Note: %G, unlike %f, avoid trailing '0'
            fmt.res.AppendFmt("%G", arg.u.d);
            break;
        case Type::FormatStr:
            fmt.res.AppendView(arg.u.sv);
            break;
        case Type::Str:
            fmt.res.AppendView(arg.u.sv);
            break;
        case Type::WStr:
            auto sUtf8 = strconv::WstrToUtf8(arg.u.wsv);
            fmt.res.AppendAndFree(sUtf8);
            break;
    };
}

char* Fmt::Get() {
    CrashIf(nArgs != maxArgNo + 1);
    for (int i = 0; isOk && i < nInst; i++) {
        serializeInst(*this, i);
    }
    if (!isOk) {
        res.Reset();
        res.Append("Mismatch format/args for format string: ");
        res.Append(format);
    }
    return res.Get();
}

char* Fmt::GetDup() {
    return str::Dup(Get());
}

std::string_view Format(const char* s, __unused const Arg& a1, __unused const Arg& a2, __unused const Arg& a3) {
    Fmt fmt(s);
    addArg(fmt, a1);
    addArg(fmt, a2);
    addArg(fmt, a3);
    auto res = fmt.GetDup();
    return {res, str::Len(res)};
}

std::string_view Format(const char* s, const Arg& a1, const Arg& a2) {
    return Format(s, a1, a2, Arg());
    return {};
}

std::string_view Format(const char* s, const Arg& a1) {
    return Format(s, a1, Arg(), Arg());
}

} // namespace fmt
