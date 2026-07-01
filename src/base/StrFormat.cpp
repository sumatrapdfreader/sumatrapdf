/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Base.h"

/*
strfmt::Fmt is type-safe printf()-like system with support for both %-style formatting
directives and C#-like positional directives ({0}, {1} etc.).

Type safety is achieved by using strongly typed methods for adding arguments
(i(), c(), s() etc.). We also verify that the type of the argument matches
the type of formatting directive.

Positional directives are useful in translations with more than 1 argument
because in some languages translation is akward if you can't re-arrange
the order of arguments.

Idiomatic usage:
strfmt::Fmt fmt("%d = %s");
char *s = fmt.i(5).s("5").Get(); // returns "5 = 5"
// s is valid until fmt is valid
// use .GetDup() to get a copy that must be free()d
// you can re-use fmt as:
s = fmt.ParseFormat("{1} = {2} + {0}").i(3).s("3").s(L"

You can mix % and {} directives but beware, as the rule for assigning argument
number to % directive is simple (n-th argument position for n-th % directive)
but it's easy to mis-count when adding {} to the mix.

TODO: similar approach could be used for type-safe scanf() replacement.
*/

namespace strfmt {

// formatting instruction
struct Inst {
    Type t = Type::None;
    int argNo = 0;  // <0 for strings that come from formatting string
    int rawOff = 0; // offset into format for Type::RawStr / start of fwp for % spec
    int sLen = 0;   // length, for Type::RawStr

    // for a % spec: the conversion char and the flags+width+precision range
    // (everything between '%' and the length-modifier/conversion). We delegate
    // the actual formatting to snprintf, only normalizing the length modifier so
    // 32/64-bit semantics match printf exactly.
    char conv = 0;
    int intBits = 0; // 32 or 64 for integer-family conversions
    int fwpOff = 0;  // offset into format of flags+width+precision
    int fwpLen = 0;
    int width = 0; // parsed width (for manual %s padding)
    int prec = -1; // parsed precision, -1 if none (for manual %s)
    bool leftJust = false;
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
    str::Builder res;

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
    return off + 1;
}

static Type typeFromConv(char c) {
    switch (c) {
        case 'c':
            return Type::Char;
        case 'd':
        case 'i':
        case 'u':
        case 'o':
        case 'x':
        case 'X':
            return Type::Int;
        case 'p':
            return Type::Ptr;
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
        case 'a':
        case 'A':
            return Type::Float;
        case 's':
        case 'S':
            return Type::Str;
        case 'v':
            return Type::Any;
    }
    return Type::None;
}

static bool startsWith(Str s, int off, const char* prefix) {
    int i = 0;
    while (prefix[i]) {
        if (off + i >= s.len || s.s[off + i] != prefix[i]) {
            return false;
        }
        i++;
    }
    return true;
}

// parse: %[flags][width][.prec][length]<conv>
// We capture flags+width+precision verbatim (fed to snprintf) and normalize the
// length modifier into an explicit 32/64-bit width so output matches printf.
static int parseArgDefPerc(Fmt& fmt, int off) {
    Str f = fmt.format;
    ReportIf(f.s[off] != '%');
    off++; // past '%'
    int fwpStart = off;
    bool leftJust = false;
    // flags
    while (off < f.len &&
           (f.s[off] == '-' || f.s[off] == '+' || f.s[off] == ' ' || f.s[off] == '0' || f.s[off] == '#')) {
        if (f.s[off] == '-') {
            leftJust = true;
        }
        off++;
    }
    // width
    int width = 0;
    while (off < f.len && str::IsDigit(f.s[off])) {
        width = width * 10 + (f.s[off] - '0');
        off++;
    }
    // precision
    int prec = -1;
    if (off < f.len && f.s[off] == '.') {
        off++;
        prec = 0;
        while (off < f.len && str::IsDigit(f.s[off])) {
            prec = prec * 10 + (f.s[off] - '0');
            off++;
        }
    }
    int fwpEnd = off;
    // length modifier; determine integer width (32/64 on LLP64 / win64)
    int bits = 32;
    if (startsWith(f, off, "I64")) {
        bits = 64;
        off += 3;
    } else if (startsWith(f, off, "I32")) {
        off += 3;
    } else if (startsWith(f, off, "ll")) {
        bits = 64;
        off += 2;
    } else if (startsWith(f, off, "hh")) {
        off += 2;
    } else if (off < f.len && f.s[off] == 'l') {
        off++; // long is 32-bit on win64
    } else if (off < f.len && f.s[off] == 'h') {
        off++;
    } else if (off < f.len && f.s[off] == 'L') {
        off++;
    } else if (off < f.len && (f.s[off] == 'z' || f.s[off] == 'j' || f.s[off] == 't' || f.s[off] == 'I')) {
        bits = 64; // size_t / intmax_t / ptrdiff_t / MS size_t
        off++;
    } else if (off < f.len && f.s[off] == 'w') {
        off++;
    }
    char conv = (off < f.len) ? f.s[off] : 0;
    off++;

    auto& i = fmt.instructions[fmt.nInst++];
    i.t = typeFromConv(conv);
    i.argNo = fmt.currPercArgNo++;
    i.conv = conv;
    i.intBits = bits;
    i.fwpOff = fwpStart;
    i.fwpLen = fwpEnd - fwpStart;
    i.width = width;
    i.prec = prec;
    i.leftJust = leftJust;
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

static bool isIntLike(Type t) {
    return t == Type::Char || t == Type::Int || t == Type::Ptr;
}

static bool validArgTypes(Type instType, Type argType) {
    if (instType == Type::Any || instType == Type::RawStr) {
        return true;
    }
    // integer-family specs (%c %d %u %x %p ...) accept any integer-like arg
    // (char / int / pointer), matching printf's leniency -- e.g. an HWND with
    // %x, or an int with %c.
    if (instType == Type::Char || instType == Type::Int || instType == Type::Ptr) {
        return isIntLike(argType);
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

    int maxArgNo = -1; // -1 so an escape/literal-only format requires no args
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

// format a single value into a caller-provided buffer via snprintf, NUL-terminating
// even on truncation. Avoids allocating (assuming vsnprintf doesn't allocate).
static void bufFmt(Str buf, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    str::VsnprintfUtf8(buf, fmt, args);
    va_end(args);
    buf.s[buf.len - 1] = 0;
}

// default formatting for {n} positional and %v: format by the arg's runtime type
static void evalDefault(Fmt& fmt, const Arg& arg) {
    TempStr s;
    Str buf(fmt.buf, (int)dimof(fmt.buf));
    switch (arg.t) {
        case Type::Char:
            fmt.res.AppendChar(arg.c);
            break;
        case Type::Int:
            bufFmt(buf, "%lld", (long long)arg.i);
            fmt.res.Append(fmt.buf);
            break;
        case Type::Ptr:
            bufFmt(buf, "%p", arg.ptr);
            fmt.res.Append(fmt.buf);
            break;
        case Type::Float:
            // Note: %G, unlike %f, avoids trailing '0'
            bufFmt(buf, "%G", (double)arg.f);
            fmt.res.Append(fmt.buf);
            break;
        case Type::Double:
            bufFmt(buf, "%G", arg.d);
            fmt.res.Append(fmt.buf);
            break;
        case Type::Str:
            fmt.res.Append(arg.str);
            break;
        case Type::WStr:
            s = ToUtf8Temp(arg.wstr);
            fmt.res.Append(s);
            break;
        default:
            ReportIf(true);
            break;
    }
}

// extract an integer value from any integer-like arg (char / int / pointer) so
// %d/%x/%c/%p work with any of them, like printf.
static i64 argToI64(const Arg& arg) {
    switch (arg.t) {
        case Type::Char:
            return (i64)arg.c;
        case Type::Ptr:
            return (i64)(intptr_t)arg.ptr;
        default:
            return arg.i;
    }
}

// format a typed % spec by reconstructing a single-conversion printf format and
// delegating to snprintf (bufFmt), normalizing the length modifier so the
// 32/64-bit value width matches printf. %s padding/truncation is done by hand to
// avoid relying on the Str being NUL-terminated.
static void evalPercInst(Fmt& fmt, const Inst& inst, const Arg& arg) {
    char* buf = fmt.buf;
    Str bufS(fmt.buf, (int)dimof(fmt.buf));

    if (inst.conv == 's' || inst.conv == 'S') {
        Str sv = (arg.t == Type::WStr) ? ToUtf8Temp(arg.wstr) : arg.str;
        int slen = sv.len;
        if (inst.prec >= 0 && inst.prec < slen) {
            slen = inst.prec;
        }
        int pad = inst.width - slen;
        if (pad < 0) {
            pad = 0;
        }
        if (!inst.leftJust) {
            for (int j = 0; j < pad; j++) {
                fmt.res.AppendChar(' ');
            }
        }
        fmt.res.Append(Str(sv.s, slen));
        if (inst.leftJust) {
            for (int j = 0; j < pad; j++) {
                fmt.res.AppendChar(' ');
            }
        }
        return;
    }

    // build "%" + flags+width+precision into fbuf
    char fbuf[64];
    int k = 0;
    fbuf[k++] = '%';
    for (int j = 0; j < inst.fwpLen && k < (int)dimof(fbuf) - 5; j++) {
        fbuf[k++] = fmt.format.s[inst.fwpOff + j];
    }
    char conv = inst.conv;
    i64 ival = argToI64(arg);
    switch (conv) {
        case 'd':
        case 'i':
            if (inst.intBits == 64) {
                fbuf[k++] = 'l';
                fbuf[k++] = 'l';
                fbuf[k++] = 'd';
                fbuf[k] = 0;
                bufFmt(bufS, fbuf, (long long)ival);
            } else {
                fbuf[k++] = 'd';
                fbuf[k] = 0;
                bufFmt(bufS, fbuf, (int)ival);
            }
            fmt.res.Append(buf);
            break;
        case 'u':
        case 'o':
        case 'x':
        case 'X':
            if (inst.intBits == 64) {
                fbuf[k++] = 'l';
                fbuf[k++] = 'l';
                fbuf[k++] = conv;
                fbuf[k] = 0;
                bufFmt(bufS, fbuf, (unsigned long long)ival);
            } else {
                fbuf[k++] = conv;
                fbuf[k] = 0;
                bufFmt(bufS, fbuf, (unsigned int)(unsigned long long)ival);
            }
            fmt.res.Append(buf);
            break;
        case 'c':
            fbuf[k++] = 'c';
            fbuf[k] = 0;
            bufFmt(bufS, fbuf, (int)ival);
            fmt.res.Append(buf);
            break;
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
        case 'a':
        case 'A': {
            fbuf[k++] = conv;
            fbuf[k] = 0;
            double dv = (arg.t == Type::Double) ? arg.d : (double)arg.f;
            bufFmt(bufS, fbuf, dv);
            fmt.res.Append(buf);
        } break;
        case 'p': {
            // flags/width are uncommon (and platform-specific) for %p; emit plain
            const void* pv = (arg.t == Type::Ptr) ? arg.ptr : (const void*)(intptr_t)ival;
            bufFmt(bufS, "%p", pv);
            fmt.res.Append(buf);
        } break;
        default:
            ReportIf(true);
            break;
    }
}

bool Fmt::Eval(const Arg** args, int nArgs) {
    if (!isOk) {
        // if failed parsing format
        return false;
    }

    for (int n = 0; n < nInst; n++) {
        ReportIf(n >= dimof(instructions));

        auto& inst = instructions[n];

        if (inst.t == Type::RawStr) {
            res.Append(Str(format.s + inst.rawOff, inst.sLen));
            continue;
        }

        int argNo = inst.argNo;
        ReportIf(argNo >= nArgs);
        if (argNo >= nArgs) {
            isOk = false;
            return false;
        }

        const Arg& arg = *args[argNo];
        isOk = validArgTypes(inst.t, arg.t);
        ReportIf(!isOk);
        if (!isOk) {
            return false;
        }

        if (inst.t == Type::Any) {
            evalDefault(*this, arg);
        } else {
            evalPercInst(*this, inst, arg);
        }
    }
    return true;
}

// Format into an explicit arena; the returned Str lives in `a`. Use this
// instead of fmt()/FormatTemp when the result must outlive the temp allocator's
// scope, or on paths that must not touch the temp allocator / heap at all (e.g.
// the crash handler, which pre-allocates its arena). FormatTempArgs() is just
// this with GetTempArena().
Str FormatArgs(Arena* a, const char* fmt, const Arg** args, int nArgs) {
    // trailing arguments could be empty (unused defaults from the variadic call)
    while (nArgs > 0 && args[nArgs - 1]->t == Type::None) {
        nArgs--;
    }

    if (nArgs == 0) {
        // no args: if the format has no directives/escapes, return it verbatim
        // (fast path); otherwise still run it through so %% and \{ are handled.
        bool hasDirective = false;
        for (const char* p = fmt; p && *p; p++) {
            if (*p == '%' || *p == '{' || *p == '\\') {
                hasDirective = true;
                break;
            }
        }
        if (!hasDirective) {
            return str::Dup(a, Str(fmt));
        }
    }

    Fmt f;
    // format directly into the caller's arena so there are no temp-allocator /
    // heap allocations at all (matters for the crash handler's pre-allocated
    // arena). StealData() then returns that arena buffer without a second copy.
    f.res.allocator = a;
    bool ok = ParseFormat(f, fmt);
    if (!ok) {
        return {};
    }
    ok = f.Eval(args, nArgs);
    if (!ok) {
        return {};
    }
    return f.res.StealData();
}

TempStr FormatTempArgs(const char* fmt, const Arg** args, int nArgs) {
    return FormatArgs(GetTempArena(), fmt, args, nArgs);
}

} // namespace strfmt
