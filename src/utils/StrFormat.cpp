/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "StrFormat.h"

namespace fmt {

// formatting instruction
struct Inst {
    Type t = Type::None;
    int width = 0;           // length, for numbers e.g. %4d, length is 4
    int prec = 0;            // precision for floating numbers e.g. %.2f, prec is 2
    char fill = 0;           // filler, for number e.g. '%04d', filler is '0', '% 6d', filler is ' '
    int argNo = 0;           // <0 for strings that come from formatting string
    const char* s = nullptr; // if t is Type::FormatStr
    int sLen = 0;
};

struct Fmt {
    Fmt() = default;
    ~Fmt() = default;

    bool Eval(const Arg** args, int nArgs);

    bool isOk = true; // true if mismatch between formatting instruction and args

    const char* format = nullptr;
    Inst instructions[32]{}; // 32 should be big enough for everybody
    int nInst = 0;

    int currArgNo = 0;
    int currPercArgNo = 0;
    str::Str res;

    char buf[256] = {};
};

static void addRawStr(Fmt& fmt, const char* s, size_t len) {
    if (len == 0) {
        return;
    }
    ReportIf(fmt.nInst >= dimof(fmt.instructions));
    auto& i = fmt.instructions[fmt.nInst++];
    i.t = Type::RawStr;
    i.s = s;
    i.sLen = (int)len;
    i.argNo = -1;
}

// parse: {$n}
static const char* parseArgDefPositional(Fmt& fmt, const char* s) {
    ReportIf(*s != '{');
    ++s;
    int n = 0;
    while (*s != '}') {
        // TODO: this could be more featurful
        ReportIf(!str::IsDigit(*s));
        n = n * 10 + (*s - '0');
        ++s;
    }
    auto& i = fmt.instructions[fmt.nInst++];
    i.t = Type::Any;
    i.argNo = n;
    i.fill = 0;
    i.width = 0;
    i.prec = 0;
    return s + 1;
}

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
        case 'v':
            return Type::Any;
    }
    return Type::None;
}

static bool isFmtChar(char c) {
    if (c >= '0' && c <= '9') {
        return true;
    }
    if (c == '.' || c == ' ') {
        return true;
    }
    return false;
}

// parse: %[<fmt>][csfd]
static const char* parseArgDefPerc(Fmt& fmt, const char* s) {
    ReportIf(*s != '%');
    s++;
    const char* fmtStart = s;
    while (*s && isFmtChar(*s)) {
        ++s;
    }
    const char* fmtEnd = s;
    Type tp = typeFromChar(*s++);

    auto& i = fmt.instructions[fmt.nInst];
    i.t = tp;
    i.argNo = fmt.currPercArgNo++;
    i.fill = 0;
    i.width = 0;
    ++fmt.nInst;
    char c;
    // for now we only support ' ' or 0 for filler and a single digit for nLen
    int n = (int)(fmtEnd - fmtStart);
    if (n > 0) {
        c = *fmtStart++;
        if (c == ' ' || c == '0') {
            i.fill = c;
            n--;
        }
    }
    ReportIf(n > 1); // TODO: only support a single digit for nLen
    if (n > 0) {
        c = *fmtStart++;
        if (c >= '0' && c <= '9') {
            i.width = c - '0';
        } else {
            ReportIf(true);
        }
    }
    return s;
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
    if (instType == Type::Any || instType == Type::RawStr) {
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
                addRawStr(o, start, fmt - start);
                start = fmt + 1;
                fmt += 2; // skip '{'
                continue;
            }
            continue;
        }
        if ('{' == c) {
            addRawStr(o, start, fmt - start);
            fmt = parseArgDefPositional(o, fmt);
            start = fmt;
            continue;
        }
        if ('%' == c) {
            // handle %%
            if ('%' == fmt[1]) {
                addRawStr(o, start, fmt - start);
                start = fmt + 1;
                fmt += 2; // skip '%'
                continue;
            }
            addRawStr(o, start, fmt - start);
            fmt = parseArgDefPerc(o, fmt);
            start = fmt;
            continue;
        }
        ++fmt;
    }
    addRawStr(o, start, fmt - start);

    int maxArgNo = o.currArgNo;
    // check that arg numbers in {$n} makes sense
    for (int i = 0; i < o.nInst; i++) {
        if (o.instructions[i].t == Type::RawStr) {
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
        ReportIf(!isOk);
        if (!isOk) {
            return false;
        }
    }
    return true;
}

bool Fmt::Eval(const Arg** args, int nArgs) {
    if (!isOk) {
        // if failed parsing format
        return false;
    }

    for (int n = 0; n < nInst; n++) {
        ReportIf(n >= dimof(instructions));

        auto& inst = instructions[n];
        int argNo = inst.argNo;
        ReportIf(argNo >= nArgs);
        if (argNo >= nArgs) {
            isOk = false;
            return false;
        }

        if (inst.t == Type::RawStr) {
            res.Append(inst.s, (size_t)inst.sLen);
            continue;
        }

        const Arg& arg = *args[argNo];
        isOk = validArgTypes(inst.t, arg.t);
        ReportIf(!isOk);
        if (!isOk) {
            return false;
        }

        char* s;
        switch (arg.t) {
            case Type::Char:
                res.AppendChar(arg.u.c);
                break;
            case Type::Int: {
                // TODO: i64 is potentially bigger than int
                char f[5] = {'%', 0};
                int i = 1;
                if (inst.fill) {
                    f[i++] = inst.fill;
                }
                if (inst.width > 0) {
                    f[i++] = '0' + (char)inst.width;
                }
                f[i++] = 'd';
                ReportIf(i >= dimof(f));
                str::BufFmt(buf, dimof(buf), f, (int)arg.u.i);
                res.Append(buf);
            } break;
            case Type::Float:
                // Note: %G, unlike %f, avoid trailing '0'
                str::BufFmt(buf, dimof(buf), "%G", arg.u.f);
                res.Append(buf);
                break;
            case Type::Double:
                // Note: %G, unlike %f, avoid trailing '0'
                str::BufFmt(buf, dimof(buf), "%G", arg.u.d);
                res.Append(buf);
                break;
            case Type::Str:
                res.Append(arg.u.s);
                break;
            case Type::WStr:
                s = ToUtf8Temp(arg.u.ws);
                res.Append(s);
                break;
            default:
                ReportIf(true);
                break;
        };
    }
    return true;
}

char* Format(const char* s, const Arg& a1, const Arg& a2, const Arg& a3, const Arg& a4, const Arg& a5, const Arg& a6) {
    const Arg* args[6];
    int nArgs = 0;
    args[nArgs++] = &a1;
    args[nArgs++] = &a2;
    args[nArgs++] = &a3;
    args[nArgs++] = &a4;
    args[nArgs++] = &a5;
    args[nArgs++] = &a6;
    ReportIf(nArgs > dimofi(args));
    // arguments at the end could be empty
    while (nArgs > 0 && args[nArgs - 1]->t == Type::None) {
        nArgs--;
    }

    if (nArgs == 0) {
        // TODO: verify that format has no references to args
        return str::Dup(s);
    }

    Fmt fmt;
    bool ok = ParseFormat(fmt, s);
    if (!ok) {
        return nullptr;
    }
    ok = fmt.Eval(args, nArgs);
    if (!ok) {
        return nullptr;
    }
    char* res = fmt.res.StealData();
    return res;
}

char* FormatTemp(const char* s, const Arg** args, int nArgs) {
    // arguments at the end could be empty
    while (nArgs >= 0 && args[nArgs - 1]->t == Type::None) {
        nArgs--;
    }

    if (nArgs == 0) {
        // TODO: verify that format has no references to args
        return (char*)s;
    }

    Fmt fmt;
    bool ok = ParseFormat(fmt, s);
    if (!ok) {
        return nullptr;
    }
    ok = fmt.Eval(args, nArgs);
    if (!ok) {
        return nullptr;
    }
    char* res = fmt.res.Get();
    size_t n = fmt.res.size();
    return str::DupTemp(res, n);
}

char* FormatTemp(const char* s, const Arg& a1, const Arg& a2, const Arg& a3, const Arg& a4, const Arg& a5,
                 const Arg& a6) {
    const Arg* args[6];
    int nArgs = 0;
    args[nArgs++] = &a1;
    args[nArgs++] = &a2;
    args[nArgs++] = &a3;
    args[nArgs++] = &a4;
    args[nArgs++] = &a5;
    args[nArgs++] = &a6;
    ReportIf(nArgs > dimofi(args));
    return FormatTemp(s, args, nArgs);
}

char* FormatTemp(const char* s, const Arg a1) {
    const Arg* args[3] = {&a1};
    return FormatTemp(s, args, 1);
}

char* FormatTemp(const char* s, const Arg a1, const Arg a2) {
    const Arg* args[3] = {&a1, &a2};
    return FormatTemp(s, args, 2);
}

char* FormatTemp(const char* s, const Arg a1, const Arg a2, const Arg a3) {
    const Arg* args[3] = {&a1, &a2, &a3};
    return FormatTemp(s, args, 3);
}

} // namespace fmt
