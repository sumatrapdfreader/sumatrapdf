/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#if OS_POSIX
#include <locale.h>
#endif

/*
str::Fmt is type-safe printf()-like system with support for both %-style formatting
directives and C#-like positional directives ({0}, {1} etc.).

Type safety is achieved by using strongly typed methods for adding arguments
(i(), c(), s() etc.). We also verify that the type of the argument matches
the type of formatting directive.

Positional directives are useful in translations with more than 1 argument
because in some languages translation is akward if you can't re-arrange
the order of arguments.

Idiomatic usage:
str::Fmt fmt("%d = %s");
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

namespace str {

// formatting instruction
struct Inst {
    FmtArg::Kind t = FmtArg::Kind::None;
    int argNo = 0;  // <0 for strings that come from formatting string
    int rawOff = 0; // offset into format for FmtArg::Kind::RawStr / start of fwp for % spec
    int sLen = 0;   // length, for FmtArg::Kind::RawStr

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

    bool Eval(const FmtArg** args, int nArgs);

    bool isOk = true; // true if mismatch between formatting instruction and args

    Str format;
    Inst instructions[32]{}; // 32 should be big enough for everybody
    int nInst = 0;

    int currArgNo = 0;
    int currPercArgNo = 0;
    str::Builder res;

    char buf[256] = {};
};

static void addRawStr(Fmt& fmt, int off, size_t n) {
    if (n == 0) {
        return;
    }
    ReportIf(fmt.nInst >= dimof(fmt.instructions));
    auto& i = fmt.instructions[fmt.nInst++];
    i.t = FmtArg::Kind::RawStr;
    i.rawOff = off;
    i.sLen = (int)n;
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
    i.t = FmtArg::Kind::Any;
    i.argNo = n;
    return off + 1;
}

static FmtArg::Kind typeFromConv(char c) {
    switch (c) {
        case 'c':
            return FmtArg::Kind::Char;
        case 'd':
        case 'i':
        case 'u':
        case 'o':
        case 'x':
        case 'X':
            return FmtArg::Kind::Int;
        case 'p':
            return FmtArg::Kind::Ptr;
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
        case 'a':
        case 'A':
            return FmtArg::Kind::Float;
        case 's':
        case 'S':
            return FmtArg::Kind::Str;
        case 'v':
            return FmtArg::Kind::Any;
    }
    return FmtArg::Kind::None;
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

static bool isIntLike(FmtArg::Kind t) {
    return t == FmtArg::Kind::Char || t == FmtArg::Kind::Int || t == FmtArg::Kind::Ptr;
}

static bool validArgTypes(FmtArg::Kind instType, FmtArg::Kind argType) {
    if (instType == FmtArg::Kind::Any || instType == FmtArg::Kind::RawStr) {
        return true;
    }
    // integer-family specs (%c %d %u %x %p ...) accept any integer-like arg
    // (char / int / pointer), matching printf's leniency -- e.g. an HWND with
    // %x, or an int with %c.
    if (instType == FmtArg::Kind::Char || instType == FmtArg::Kind::Int || instType == FmtArg::Kind::Ptr) {
        return isIntLike(argType);
    }
    if (instType == FmtArg::Kind::Float) {
        return argType == FmtArg::Kind::Float || argType == FmtArg::Kind::Double;
    }
    if (instType == FmtArg::Kind::Str) {
        return argType == FmtArg::Kind::Str || argType == FmtArg::Kind::WStr;
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
            // only "{N}" is an argument reference; a '{' not followed by a
            // digit is raw text (e.g. CSS / JS braces in html templates that
            // are run through fmt)
            if (off + 1 < fmtStr.len && str::IsDigit(fmtStr.s[off + 1])) {
                addRawStr(o, start, off - start);
                off = parseArgDefPositional(o, off);
                start = off;
                continue;
            }
            off++;
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
        if (o.instructions[i].t == FmtArg::Kind::RawStr) {
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
static void evalDefault(Fmt& fmt, const FmtArg& arg) {
    TempStr s;
    Str buf(fmt.buf, (int)dimof(fmt.buf));
    switch (arg.t) {
        case FmtArg::Kind::Char:
            fmt.res.AppendChar(arg.c);
            break;
        case FmtArg::Kind::Int:
            bufFmt(buf, "%lld", (long long)arg.i);
            fmt.res.Append(fmt.buf);
            break;
        case FmtArg::Kind::Ptr:
            bufFmt(buf, "%p", arg.ptr);
            fmt.res.Append(fmt.buf);
            break;
        case FmtArg::Kind::Float:
            // Note: %G, unlike %f, avoids trailing '0'
            bufFmt(buf, "%G", (double)arg.f);
            fmt.res.Append(fmt.buf);
            break;
        case FmtArg::Kind::Double:
            bufFmt(buf, "%G", arg.d);
            fmt.res.Append(fmt.buf);
            break;
        case FmtArg::Kind::Str:
            fmt.res.Append(arg.str);
            break;
        case FmtArg::Kind::WStr:
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
static i64 argToI64(const FmtArg& arg) {
    switch (arg.t) {
        case FmtArg::Kind::Char:
            return (i64)arg.c;
        case FmtArg::Kind::Ptr:
            return (i64)(intptr_t)arg.ptr;
        default:
            return arg.i;
    }
}

// format a typed % spec by reconstructing a single-conversion printf format and
// delegating to snprintf (bufFmt), normalizing the length modifier so the
// 32/64-bit value width matches printf. %s padding/truncation is done by hand to
// avoid relying on the Str being NUL-terminated.
static void evalPercInst(Fmt& fmt, const Inst& inst, const FmtArg& arg) {
    char* buf = fmt.buf;
    Str bufS(fmt.buf, (int)dimof(fmt.buf));

    if (inst.conv == 's' || inst.conv == 'S') {
        Str sv = (arg.t == FmtArg::Kind::WStr) ? ToUtf8Temp(arg.wstr) : arg.str;
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
            double dv = (arg.t == FmtArg::Kind::Double) ? arg.d : (double)arg.f;
            bufFmt(bufS, fbuf, dv);
            fmt.res.Append(buf);
        } break;
        case 'p': {
            // flags/width are uncommon (and platform-specific) for %p; emit plain
            const void* pv = (arg.t == FmtArg::Kind::Ptr) ? arg.ptr : (const void*)(intptr_t)ival;
            bufFmt(bufS, "%p", pv);
            fmt.res.Append(buf);
        } break;
        default:
            ReportIf(true);
            break;
    }
}

bool Fmt::Eval(const FmtArg** args, int nArgs) {
    if (!isOk) {
        // if failed parsing format
        return false;
    }

    for (int n = 0; n < nInst; n++) {
        ReportIf(n >= dimof(instructions));

        auto& inst = instructions[n];

        if (inst.t == FmtArg::Kind::RawStr) {
            res.Append(Str(format.s + inst.rawOff, inst.sLen));
            continue;
        }

        int argNo = inst.argNo;
        ReportIf(argNo >= nArgs);
        if (argNo >= nArgs) {
            isOk = false;
            return false;
        }

        const FmtArg& arg = *args[argNo];
        isOk = validArgTypes(inst.t, arg.t);
        ReportIf(!isOk);
        if (!isOk) {
            return false;
        }

        if (inst.t == FmtArg::Kind::Any) {
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
Str FormatArgs(Arena* a, const char* fmt, const FmtArg** args, int nArgs) {
    // trailing arguments could be empty (unused defaults from the variadic call)
    while (nArgs > 0 && args[nArgs - 1]->t == FmtArg::Kind::None) {
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
    // arena). TakeStr() then returns that arena buffer without a second copy.
    f.res.a = a;
    bool ok = ParseFormat(f, fmt);
    if (!ok) {
        return {};
    }
    ok = f.Eval(args, nArgs);
    if (!ok) {
        return {};
    }
    return f.res.TakeStr();
}

TempStr FormatTempArgs(const char* fmt, const FmtArg** args, int nArgs) {
    return FormatArgs(GetTempArena(), fmt, args, nArgs);
}

static int HexDigitVal(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static TempStr ExtractUntilTemp(Str str, int off, char c, int* endOffOut) {
    if (off < 0 || off > str.len) {
        return {};
    }
    Str slice = Str(str.s + off, str.len - off);
    int foundOff = IndexOfChar(slice, c);
    if (foundOff < 0) {
        return {};
    }
    int endOff = off + foundOff;
    *endOffOut = endOff;
    return str::DupTemp(Str(str.s + off, foundOff));
}

static int ParseLimitedNumber(Str str, int p, int formatOff, Str format, int* endOffOut, const ParseArg& valueOut) {
    unsigned int width;
    char f2[] = "% ";
    Str formatAt = Str(format.s + formatOff, format.len - formatOff);
    Str endF = Parse(formatAt, "%u%c", &width, &f2[1]);
    if (!str::IsNull(endF) && str::ContainsChar(StrL("udx"), f2[1]) && width <= (unsigned)(str.len - p)) {
        char limited[16]; // 32-bit integers are at most 11 characters long
        str::BufSet(Str(limited, std::min((int)width + 1, dimofi(limited))), Str(str.s + p, (int)width));
        Str end = ParseArgs(Str(limited), f2, &valueOut, 1);
        if (!str::IsNull(end) && !end.s[0]) {
            *endOffOut = p + (int)width;
            return (int)(endF.s - format.s) - 1;
        }
    }
    return -1;
}

static bool ParseULongAt(Str str, int off, int base, unsigned long* val, int* endOff) {
    if (off >= str.len) {
        return false;
    }
    unsigned long v = 0;
    int i = off;
    while (i < str.len && str::IsWs(str.s[i])) {
        i++;
    }
    if (base == 16 && i + 1 < str.len && str.s[i] == '0' && (str.s[i + 1] == 'x' || str.s[i + 1] == 'X')) {
        i += 2;
    }
    bool any = false;
    while (i < str.len) {
        char c = str.s[i];
        int digit = -1;
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (base == 16) {
            digit = HexDigitVal(c);
        }
        if (digit < 0 || (unsigned)digit >= (unsigned)base) {
            break;
        }
        any = true;
        v = v * (unsigned long)base + (unsigned long)digit;
        i++;
    }
    if (!any) {
        return false;
    }
    *val = v;
    *endOff = i;
    return true;
}

static bool ParseLongAt(Str str, int off, int base, long* val, int* endOff) {
    if (off >= str.len) {
        return false;
    }
    bool neg = false;
    int i = off;
    while (i < str.len && str::IsWs(str.s[i])) {
        i++;
    }
    if (i >= str.len) {
        return false;
    }
    if (str.s[i] == '-') {
        neg = true;
        i++;
    } else if (str.s[i] == '+') {
        i++;
    }
    unsigned long uv = 0;
    int end = i;
    if (!ParseULongAt(Str(str.s + i, str.len - i), 0, base, &uv, &end)) {
        return false;
    }
    *val = neg ? -(long)uv : (long)uv;
    *endOff = i + end;
    return true;
}

static bool ParseDoubleAt(Str str, int off, double* val, int* endOff) {
    if (off >= str.len) {
        return false;
    }
    char* sliceZ = CStrTemp(Str(str.s + off, str.len - off));
    ptrdiff_t consumed = 0;
    {
        char* endPtr = nullptr;
        *val = strtod(sliceZ, &endPtr);
        if (!endPtr || endPtr == sliceZ) {
            return false;
        }
        consumed = endPtr - sliceZ;
    }
    *endOff = off + (int)consumed;
    return true;
}

/* Parses a string into several variables sscanf-style (i.e. pass in pointers
   to where the parsed values are to be stored). Returns a pointer to the first
   character that's not been parsed when successful and nullptr otherwise.

   Supported formats:
     %u - parses an unsigned int
     %d - parses a signed int
     %x - parses an unsigned hex-int
     %f - parses a float
     %c - parses a single char
     %s - parses a string into an AutoFree (also on failure!)
     %S - parses a string into an AutoFree
     %? - makes the next single character optional (e.g. "x%?,y" parses both "xy" and "x,y")
     %$ - causes the parsing to fail if it's encountered when not at the end of the string
     %  - skips a single whitespace character
     %_ - skips one or multiple whitespace characters (or none at all)
     %% - matches a single '%'

   %u, %d and %x accept an optional width argument, indicating exactly how many
   characters must be read for parsing the number (e.g. "%4d" parses -123 out of "-12345"
   and doesn't parse "123" at all).
*/
Str ParseArgs(Str str, const char* fmt, const ParseArg* args, int nArgs) {
    if (str::IsNull(str) || !fmt) {
        return {};
    }
    Str format = fmt;
    int argIdx = 0;
    int p = 0;
    for (int fi = 0; fi < format.len; fi++) {
        char fc = format.s[fi];
        if (fc != '%') {
            if (p >= str.len || fc != str.s[p]) {
                return {};
            }
            p++;
            continue;
        }
        fi++;
        if (fi >= format.len) {
            return {};
        }
        char spec = format.s[fi];

        int end = -1;
        if ('u' == spec) {
            unsigned long v = 0;
            if (!ParseULongAt(str, p, 10, &v, &end)) {
                return {};
            }
            ReportIf(argIdx >= nArgs);
            *(unsigned int*)args[argIdx++].ptr = (unsigned int)v;
        } else if ('d' == spec) {
            long v = 0;
            if (!ParseLongAt(str, p, 10, &v, &end)) {
                return {};
            }
            ReportIf(argIdx >= nArgs);
            *(int*)args[argIdx++].ptr = (int)v;
        } else if ('x' == spec) {
            unsigned long v = 0;
            if (!ParseULongAt(str, p, 16, &v, &end)) {
                return {};
            }
            ReportIf(argIdx >= nArgs);
            *(unsigned int*)args[argIdx++].ptr = (unsigned int)v;
        } else if ('f' == spec) {
            double v = 0;
            if (!ParseDoubleAt(str, p, &v, &end)) {
                return {};
            }
            ReportIf(argIdx >= nArgs);
            *(float*)args[argIdx++].ptr = (float)v;
        } else if ('g' == spec) {
            double v = 0;
            if (!ParseDoubleAt(str, p, &v, &end)) {
                return {};
            }
            ReportIf(argIdx >= nArgs);
            *(float*)args[argIdx++].ptr = (float)v;
        } else if ('c' == spec) {
            if (p >= str.len) {
                return {};
            }
            ReportIf(argIdx >= nArgs);
            *(char*)args[argIdx++].ptr = str.s[p];
            end = p + 1;
        } else if ('s' == spec || 'S' == spec) {
            ReportIf(argIdx >= nArgs);
            const ParseArg& arg = args[argIdx++];
            TempStr val;
            if (fi + 1 < format.len) {
                val = ExtractUntilTemp(str, p, format.s[fi + 1], &end);
            } else {
                val = str::DupTemp(Str(str.s + p, str.len - p));
                end = str.len;
            }
            if (arg.kind == ParseArg::Kind::WStrOut) {
                *(WStr*)arg.ptr = ToWStrTemp(val);
            } else {
                *(Str*)arg.ptr = val;
            }
        } else if ('$' == spec && p >= str.len) {
            continue; // don't fail, if we're indeed at the end of the string
        } else if ('%' == spec) {
            if (p >= str.len || spec != str.s[p]) {
                return {};
            }
            end = p + 1;
        } else if (' ' == spec) {
            if (p >= str.len || !str::IsWs(str.s[p])) {
                return {};
            }
            end = p + 1;
        } else if ('_' == spec) {
            if (p >= str.len || !str::IsWs(str.s[p])) {
                continue; // don't fail, if there's no whitespace at all
            }
            for (end = p + 1; end < str.len && str::IsWs(str.s[end]); end++) {
                // do nothing
            }
        } else if ('?' == spec && fi + 1 < format.len) {
            // skip the next format character, advance the string,
            // if it the optional character is the next character to parse
            fi++;
            if (p >= str.len || str.s[p] != format.s[fi]) {
                continue;
            }
            end = p + 1;
        } else if (str::IsDigit(spec)) {
            ReportIf(argIdx >= nArgs);
            int formatIdx = ParseLimitedNumber(str, p, fi, format, &end, args[argIdx++]);
            if (formatIdx < 0) {
                return {};
            }
            fi = formatIdx;
        }
        if (end < 0 || end == p) {
            return {};
        }
        p = end;
    }
    return Str(str.s + p, str.len - p);
}

// format a number with a given thousand separator e.g. it turns 1234 into "1,234"
// Caller needs to free() the result.
TempStr FormatNumWithThousandSepTemp(i64 num, LCID locale) {
#if OS_WIN
    WCHAR thousandSepW[4]{};
    if (!GetLocaleInfoW(locale, LOCALE_STHOUSAND, thousandSepW, dimof(thousandSepW))) {
        str::BufSet(thousandSepW, dimof(thousandSepW), ",");
    }
    TempStr thousandSep = ToUtf8Temp(thousandSepW);
#else
    (void)locale;
    const lconv* lc = localeconv();
    TempStr thousandSep = Str(lc && lc->thousands_sep && lc->thousands_sep[0] ? lc->thousands_sep : ",");
#endif
    TempStr buf = str::FormatTemp("%d", num);

    str::Builder res;
    int i = 3 - (buf.len % 3);
    for (int src = 0; src < buf.len; src++) {
        res.AppendChar(buf.s[src]);
        if (src + 1 < buf.len && i == 2) {
            res.Append(thousandSep);
        }
        i = (i + 1) % 3;
    }

    return ToStrTemp(res);
}

// Format a floating point number with at most two decimal after the point
// Caller needs to free the result.
TempStr FormatFloatWithThousandSepTemp(double number, LCID locale, bool stripTrailingZero) {
    i64 num = (i64)(number * 100 + 0.5);

    TempStr tmp = FormatNumWithThousandSepTemp(num / 100, locale);
#if OS_WIN
    WCHAR decimalW[4] = {};
    if (!GetLocaleInfoW(locale, LOCALE_SDECIMAL, decimalW, dimof(decimalW))) {
        decimalW[0] = '.';
        decimalW[1] = 0;
    }
    char decimal[4];
    int i = 0;
    for (WCHAR c : decimalW) {
        decimal[i++] = (char)c;
    }
#else
    const lconv* lc = localeconv();
    const char* decimal = lc && lc->decimal_point && lc->decimal_point[0] ? lc->decimal_point : ".";
#endif

    // add between one and two decimals after the point
    TempStr buf = str::FormatTemp("%s%s%02d", tmp, Str(decimal), num % 100);
    if (stripTrailingZero && str::EndsWith(buf, StrL("0"))) {
        buf.s[buf.len - 1] = '\0';
        buf.len--;
    }

    return buf;
}

constexpr double KB = 1024;
constexpr double MB = (double)1024 * (double)1024;
constexpr double GB = (double)1024 * (double)1024 * (double)1024;

static Str sizeUnitsEnglish[3] = {StrL("GB"), StrL("MB"), StrL("KB")};

// Format the file size in a short form that rounds to the largest size unit
// e.g. "3.48 GB", "12.38 MB", "23 KB"
// To be used in a context where translations are not yet available
TempStr FormatSizeShortTemp(i64 size) {
    return FormatSizeShortTemp(size, sizeUnitsEnglish);
}

TempStr FormatSizeShortTemp(i64 size, Str const* sizeUnits) {
    Str unit{};
    double s = (double)size;
    if (!sizeUnits) {
        sizeUnits = sizeUnitsEnglish;
    }
    if (s > GB) {
        s = s / GB;
        unit = sizeUnits[0];
    } else if (s > MB) {
        s = s / MB;
        unit = sizeUnits[1];
    } else {
        s = s / KB;
        unit = sizeUnits[2];
    }

    TempStr sizestr = str::FormatFloatWithThousandSepTemp(s, LOCALE_USER_DEFAULT, false);
    if (!unit) {
        return sizestr;
    }
    return str::FormatTemp("%s %s", sizestr, unit);
}

// format file size in a readable way e.g. 1348258 is shown
// as "1.29 MB (1,348,258 Bytes)"
TempStr FormatFileSizeTemp(i64 size) {
    if (size <= 0) {
        return str::FormatTemp("%d", (int)size);
    }
    TempStr n1 = str::FormatSizeShortTemp(size);
    TempStr n2 = str::FormatNumWithThousandSepTemp(size);
    return str::FormatTemp("%s (%s %s)", n1, n2, StrL("Bytes"));
}

// http://rosettacode.org/wiki/Roman_numerals/Encode#C.2B.2B
TempStr FormatRomanNumeralTemp(int n) {
    if (n < 1) {
        return {};
    }

    static struct {
        int value;
        Str numeral;
    } romandata[] = {{1000, "M"}, {900, "CM"}, {500, "D"}, {400, "CD"}, {100, "C"}, {90, "XC"}, {50, "L"},
                     {40, "XL"},  {10, "X"},   {9, "IX"},  {5, "V"},    {4, "IV"},  {1, "I"}};

    str::Builder roman;
    for (int i = 0; i < dimof(romandata); i++) {
        auto&& el = romandata[i];
        for (; n >= el.value; n -= el.value) {
            roman.Append(el.numeral);
        }
    }
    return ToStrTemp(roman);
}

} // namespace str
