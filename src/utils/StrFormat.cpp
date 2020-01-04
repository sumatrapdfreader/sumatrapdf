/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
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
    return Type::Invalid;
}

Fmt::Fmt(const char* fmt) {
    threadId = GetCurrentThreadId();
    ParseFormat(fmt);
}

void Fmt::addFormatStr(const char* s, size_t len) {
    if (len == 0) {
        return;
    }
    CrashIf(nInst >= MaxInstructions);
    instructions[nInst].t = Type::FormatStr;
    instructions[nInst].argNo = currArgFromFormatNo;
    ++nInst;

    CrashIf(nArgsUsed >= MaxArgs);
    args[currArgFromFormatNo].t = Type::FormatStr;
    args[currArgFromFormatNo].s = s;
    args[currArgFromFormatNo].len = len;
    nArgsUsed++;
    --currArgFromFormatNo;
}

const char* Fmt::parseArgDefPositional(const char* fmt) {
    CrashIf(*fmt != '{');
    ++fmt;
    int n = 0;
    while (*fmt != '}') {
        // TODO: this could be more featurful
        CrashIf(!str::IsDigit(*fmt));
        n = n * 10 + (*fmt - '0');
        ++fmt;
    }
    instructions[nInst].t = Type::Any;
    instructions[nInst].argNo = n;
    ++nInst;
    return fmt + 1;
}

const char* Fmt::parseArgDefPerc(const char* fmt) {
    CrashIf(*fmt != '%');
    // TODO: more features
    instructions[nInst].t = typeFromChar(fmt[1]);
    instructions[nInst].argNo = currPercArgNo;
    ++nInst;
    ++currPercArgNo;
    return fmt + 2;
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

Fmt& Fmt::addArgType(Type t) {
    CrashIf(nArgsUsed >= MaxArgs);
    args[nArgs].t = t;
    nArgs++;
    nArgsUsed++;
    return *this;
}

Fmt& Fmt::i(int i) {
    args[nArgs].i = i;
    return addArgType(Type::Int);
}

Fmt& Fmt::s(const char* s) {
    args[nArgs].s = s;
    return addArgType(Type::Str);
}

Fmt& Fmt::s(const WCHAR* s) {
    args[nArgs].ws = s;
    return addArgType(Type::WStr);
}

Fmt& Fmt::c(char c) {
    args[nArgs].c = c;
    return addArgType(Type::Char);
}

Fmt& Fmt::f(float f) {
    args[nArgs].f = f;
    return addArgType(Type::Float);
}

Fmt& Fmt::f(double d) {
    args[nArgs].d = d;
    return addArgType(Type::Double);
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
        case Type::Char:
            res.AppendChar(arg.c);
            break;
        case Type::Int:
            // TODO: using AppendFmt is cheating
            res.AppendFmt("%d", arg.i);
            break;
        case Type::Float:
            // TODO: using AppendFmt is cheating
            // Note: %G, unlike %f, avoid trailing '0'
            res.AppendFmt("%G", arg.f);
            break;
        case Type::Double:
            // TODO: using AppendFmt is cheating
            // Note: %G, unlike %f, avoid trailing '0'
            res.AppendFmt("%G", arg.d);
            break;
        case Type::FormatStr:
            CrashIf(arg.len == 0);
            res.Append(arg.s, arg.len);
            break;
        case Type::Str:
            res.Append(arg.s);
            break;
        case Type::WStr:
            auto sUtf8 = strconv::WstrToUtf8(arg.ws);
            res.AppendAndFree(sUtf8.data());
            break;
    };
}

char* Fmt::Get() {
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

char* Fmt::GetDup() {
    return str::Dup(Get());
}

std::string_view Format(const char* s, Arg& a1) {
    UNUSED(s);
    UNUSED(a1);
    return {};
}

std::string_view Format(const char* s, Arg& a1, Arg& a2) {
    UNUSED(s);
    UNUSED(a1);
    UNUSED(a2);
    return {};
}

std::string_view Format(const char* s, Arg& a1, Arg& a2, Arg& a3) {
    UNUSED(s);
    UNUSED(a1);
    UNUSED(a2);
    UNUSED(a3);
    return {};
}

} // namespace fmt
