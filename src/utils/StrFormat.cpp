/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "StrFormat.h"

namespace str {

Arg::Arg()
{
    tp = None;
}

Arg::Arg(int val)
{
    tp = Int;
    i = val;
}

Arg::Arg(const char* val)
{
    tp = Str;
    s = val;
}

Arg::Arg(const WCHAR* val)
{
    tp = WStr;
    ws = val;
}

Arg Arg::null;

enum { MAX_FMT_ARGS = 6 };
enum { MAX_FMT_PARTS = MAX_FMT_ARGS * 2 + 1 };

static int ArgsCount(const Arg** args)
{
    for (int i = 0; i < MAX_FMT_ARGS; i++) {
        if (args[i]->IsNull())
            return i;
    }
    return MAX_FMT_ARGS;
}

// a format strings consists of parts that need to be copied verbatim
// and args (parts that will be replaced with the argument)
struct FmtInfo {
    enum {
        Verbatim,
        Positional,
        Percent
    };
    int tp;
    // if tp is Verbatim
    const char *start;
    int len;

    // if tp is Positional
    int positionalArgNo;

    // if tp is Percent
    char percentArg;
};

static int AddFmtVerbatim(FmtInfo *fmt, const char *start, const char *end)
{
    // don't add empty strings
    if (start == end) {
        return 0;
    }
    fmt->tp =  FmtInfo::Verbatim;
    fmt->start = start;
    fmt->len = end - start;
    CrashIf(fmt->len <= 0);
    return 1;
}

static void AddFmtPositional(FmtInfo *fmt, int argNo)
{
    fmt->tp = FmtInfo::Positional;
    fmt->positionalArgNo = argNo;
}

// parse '{$n}' part. Returns true if string follows this pattern
// and false if not.
// If returns true, sInOut is repositioned after '}'
static bool ParsePositional(const char **sInOut, int *nOut)
{
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

bool PositionalArgExists(FmtInfo *fmtParts, int fmtPartsCount, int n)
{
    for (int i = 0; i < fmtPartsCount; i++) {
        if (fmtParts[i].tp != FmtInfo::Positional)
            continue;
        if (fmtParts[i].positionalArgNo == n)
            return true;
    }
    return false;
}

int FmtArgsCount(FmtInfo *fmtParts, int n)
{
    int nPositional = 0;
    int nPercent = 0;
    for (int i = 0; i < n; i++) {
        if (fmtParts[i].tp == FmtInfo::Positional) {
            nPositional++;
        } else if (fmtParts[i].tp == FmtInfo::Percent) {
            nPercent++;
        }
    }
    if (nPositional > 0) {
        // we don't allow mixing formatting styles
        CrashIf(nPercent != 0);
        return nPositional;
    }
    return nPercent;
}

int ParseFormatString(const char *fmt, FmtInfo *fmtParts, int maxArgsCount)
{
    int n;
    int currPartNo = 0;
    const char *start = fmt;
    const char *s = fmt;
    while (*s) {
        // TODO: add support for %-style formatting
        // TODO: do we need to support \{ escaping so that it's
        //       possible to write a verbatim "{0}"
        if (*s == '{') {
            const char *end = s;
            // we allow '{' that don't follow the pattern for positional args,
            // we treat them as verbatim strings
            if (ParsePositional(&s, &n)) {
                CrashIf(n >= maxArgsCount);
                CrashIf(PositionalArgExists(fmtParts, currPartNo, n));
                currPartNo += AddFmtVerbatim(&fmtParts[currPartNo], start, end);
                AddFmtPositional(&fmtParts[currPartNo], n);
                currPartNo++;
                start = s;
            } else {
                ++s;
            }
        } else {
            ++s;
        }
    }
    currPartNo += AddFmtVerbatim(&fmtParts[currPartNo], start, s);
    return currPartNo;
}

static void SerializeArg(Str<char>& s, const Arg *arg)
{
    if (arg->tp == Arg::Str) {
        s.Append(arg->s);
    } else if (arg->tp == Arg::Int) {
        s.AppendFmt("%d", arg->i);
    } else if (arg->tp == Arg::WStr) {
        // TODO: optimize by using a stack if possible
        char *sUtf8 = str::conv::ToUtf8(arg->ws);
        s.AppendAndFree(sUtf8);
    } else {
        CrashIf(true);
    }
}

// caller has to free()
char *Fmt(const char *fmt, const Arg& a0, const Arg& a1, const Arg& a2, const Arg& a3, const Arg& a4, const Arg& a5)
{
    FmtInfo fmtParts[MAX_FMT_PARTS];
    const Arg* args[MAX_FMT_ARGS];
    args[0] = &a0; args[1] = &a1; args[2] = &a2;
    args[3] = &a3; args[4] = &a4; args[5] = &a5;
    int argsCount = ArgsCount(args);
    CrashIf(0 == argsCount);
    int nFmtParts = ParseFormatString(fmt, &fmtParts[0], argsCount);
    CrashIf(argsCount != FmtArgsCount(fmtParts, nFmtParts));
    Str<char> res;
    int nPercentArg = 0;
    for (int i=0; i < nFmtParts; i++) {
        if (fmtParts[i].tp == FmtInfo::Verbatim) {
            // TODO: unescape \{ ?
            const char *s = fmtParts[i].start;
            int len = fmtParts[i].len;
            res.Append(s, len);
        } else if (fmtParts[i].tp == FmtInfo::Positional) {
            const Arg *arg = args[fmtParts[i].positionalArgNo];
            SerializeArg(res, arg);
        } else if (fmtParts[i].tp == FmtInfo::Percent) {
            const Arg *arg = args[nPercentArg++];
            // TODO: verify that fmtParts[i].percentArg agrees with arg->tp
            SerializeArg(res, arg);
        }
    }
    return res.StealData();
}

WCHAR *Fmt(const WCHAR *fmt, const Arg& a0, const Arg& a1, const Arg& a2, const Arg& a3, const Arg& a4, const Arg& a5)
{
    const Arg* args[MAX_FMT_ARGS];
    args[0] = &a0; args[1] = &a1; args[2] = &a2;
    args[3] = &a3; args[4] = &a4; args[5] = &a5;
    int argsCount = ArgsCount(args);
    CrashIf(0 == argsCount);
    return str::Format(L"%d", argsCount);
}

}