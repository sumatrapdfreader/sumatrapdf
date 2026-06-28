/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "StrFormat.h"

namespace strfmt {

// formatting instruction
struct Inst {
    Type t = Type::None;
    int width = 0;  // length, for numbers e.g. %4d, length is 4
    int prec = 0;   // precision for floating numbers e.g. %.2f, prec is 2
    char fill = 0;  // filler, for number e.g. '%04d', filler is '0', '% 6d', filler is ' '
    int argNo = 0;  // <0 for strings that come from formatting string
    int rawOff = 0; // offset into format for Type::RawStr
    int sLen = 0;
};

struct Fmt {
    Fmt() = default;
    ~Fmt() = default;

    bool Eval(const Arg** args, int nArgs);

    bool isOk = true; // true if mismatch between formatting instruction and args

    Str format;
    Inst instructions[32]{}; // 32 should be big enough for everybody
    int nInst = 0;

    int currArgNo = 0;
    int currPercArgNo = 0;
    StrBuilder res;

    char buf[256] = {};
};

static void addRawStr(Fmt& fmt, int off, size_t len) {
    if (len == 0) {
        return;
    }
    ReportIf(fmt.nInst >= dimof(fmt.instructions));
    auto& i = fmt.instructions[fmt.nInst++];
    i.t = Type::RawStr;
    i.rawOff = off;
    i.sLen = (int)len;
    i.argNo = -1;
}

// parse: {$n}
static int parseArgDefPositional(Fmt& fmt, int off) {
    ReportIf(fmt.format.s[off] != '{');
    off++;
    int n = 0;
    while (fmt.format.s[off] != '}') {
        // TODO: this could be more featurful
        ReportIf(!str::IsDigit(fmt.format.s[off]));
        n = n * 10 + (fmt.format.s[off] - '0');
        off++;
    }
    auto& i = fmt.instructions[fmt.nInst++];
    i.t = Type::Any;
    i.argNo = n;
    i.fill = 0;
    i.width = 0;
    i.prec = 0;
    return off + 1;
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
static int parseArgDefPerc(Fmt& fmt, int off) {
    ReportIf(fmt.format.s[off] != '%');
    off++;
    int fmtStart = off;
    while (off < fmt.format.len && isFmtChar(fmt.format.s[off])) {
        off++;
    }
    int fmtEnd = off;
    Type tp = typeFromChar(fmt.format.s[off++]);

    auto& i = fmt.instructions[fmt.nInst];
    i.t = tp;
    i.argNo = fmt.currPercArgNo++;
    i.fill = 0;
    i.width = 0;
    ++fmt.nInst;
    char c;
    // for now we only support ' ' or 0 for filler and a single digit for nLen
    int n = fmtEnd - fmtStart;
    int p = fmtStart;
    if (n > 0) {
        c = fmt.format.s[p++];
        if (c == ' ' || c == '0') {
            i.fill = c;
            n--;
        }
    }
    ReportIf(n > 1); // TODO: only support a single digit for nLen
    if (n > 0) {
        c = fmt.format.s[p++];
        if (c >= '0' && c <= '9') {
            i.width = c - '0';
        } else {
            ReportIf(true);
        }
    }
    return off;
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

static bool ParseFormat(Fmt& o, Str fmtStr) {
    o.format = fmtStr;
    o.nInst = 0;
    o.currPercArgNo = 0;
    o.currArgNo = 0;
    o.res.Reset();

    // parse formatting string, until a %$c or {$n}
    // %% is how we escape %, \{ is how we escape {
    int start = 0;
    int off = 0;
    while (off < fmtStr.len && fmtStr.s[off]) {
        char c = fmtStr.s[off];
        if ('\\' == c) {
            // handle \{
            if (off + 1 < fmtStr.len && '{' == fmtStr.s[off + 1]) {
                addRawStr(o, start, off - start);
                start = off + 1;
                off += 2; // skip '{'
                continue;
            }
            off++;
            continue;
        }
        if ('{' == c) {
            addRawStr(o, start, off - start);
            off = parseArgDefPositional(o, off);
            start = off;
            continue;
        }
        if ('%' == c) {
            // handle %%
            if (off + 1 < fmtStr.len && '%' == fmtStr.s[off + 1]) {
                addRawStr(o, start, off - start);
                start = off + 1;
                off += 2; // skip '%'
                continue;
            }
            addRawStr(o, start, off - start);
            off = parseArgDefPerc(o, off);
            start = off;
            continue;
        }
        off++;
    }
    addRawStr(o, start, off - start);

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
            res.Append(Str(format.s + inst.rawOff, inst.sLen));
            continue;
        }

        const Arg& arg = *args[argNo];
        isOk = validArgTypes(inst.t, arg.t);
        ReportIf(!isOk);
        if (!isOk) {
            return false;
        }

        TempStr s;
        switch (arg.t) {
            case Type::Char:
                res.AppendChar(arg.c);
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
                str::BufFmt(buf, dimof(buf), Str(f), (int)arg.i);
                res.Append(buf);
            } break;
            case Type::Float:
                // Note: %G, unlike %f, avoid trailing '0'
                str::BufFmt(buf, dimof(buf), StrL("%G"), arg.f);
                res.Append(buf);
                break;
            case Type::Double:
                // Note: %G, unlike %f, avoid trailing '0'
                str::BufFmt(buf, dimof(buf), StrL("%G"), arg.d);
                res.Append(buf);
                break;
            case Type::Str:
                res.Append(arg.str);
                break;
            case Type::WStr:
                s = ToUtf8Temp(arg.wstr);
                res.Append(s);
                break;
            default:
                ReportIf(true);
                break;
        };
    }
    return true;
}

Str Format(Str s, const Arg& a1, const Arg& a2, const Arg& a3, const Arg& a4, const Arg& a5, const Arg& a6) {
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
        return {};
    }
    ok = fmt.Eval(args, nArgs);
    if (!ok) {
        return {};
    }
    return fmt.res.StealData();
}

TempStr FormatTemp(Str s, const Arg** args, int nArgs) {
    // arguments at the end could be empty
    while (nArgs >= 0 && args[nArgs - 1]->t == Type::None) {
        nArgs--;
    }

    if (nArgs == 0) {
        // TODO: verify that format has no references to args
        return Str(s);
    }

    Fmt fmt;
    bool ok = ParseFormat(fmt, s);
    if (!ok) {
        return {};
    }
    ok = fmt.Eval(args, nArgs);
    if (!ok) {
        return {};
    }
    return str::DupTemp(fmt.res.Get());
}

TempStr FormatTemp(Str s, const Arg& a1, const Arg& a2, const Arg& a3, const Arg& a4, const Arg& a5, const Arg& a6) {
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

TempStr FormatTemp(Str s, const Arg a1) {
    const Arg* args[3] = {&a1};
    return FormatTemp(s, args, 1);
}

TempStr FormatTemp(Str s, const Arg a1, const Arg a2) {
    const Arg* args[3] = {&a1, &a2};
    return FormatTemp(s, args, 2);
}

TempStr FormatTemp(Str s, const Arg a1, const Arg a2, const Arg a3) {
    const Arg* args[3] = {&a1, &a2, &a3};
    return FormatTemp(s, args, 3);
}

} // namespace strfmt
