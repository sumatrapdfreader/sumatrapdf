/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "StrFormat.h"

StrFormatArg::StrFormatArg()
{
    tp = TpNone;
}

StrFormatArg::StrFormatArg(int val)
{
    tp = TpInt;
    i = val;
}

StrFormatArg::StrFormatArg(const char* val)
{
    tp = TpStr;
    s = val;
}

StrFormatArg::StrFormatArg(const WCHAR* val)
{
    tp = TpWStr;
    ws = val;
}

StrFormatArg StrFormatArg::null;

namespace str {

static int ArgsCount(const StrFormatArg& a1, const StrFormatArg& a2, const StrFormatArg& a3,
    const StrFormatArg& a4, const StrFormatArg& a5, const StrFormatArg& a6)
{
    int n = 0;
    if (a1.IsNull()) return n;
    n++;
    if (a2.IsNull()) return n;
    n++;
    if (a3.IsNull()) return n;
    n++;
    if (a4.IsNull()) return n;
    n++;
    if (a5.IsNull()) return n;
    n++;
    if (a6.IsNull()) return n;
    n++;
    return n;
}

// a format strings consists of parts that need to be copied verbatim
// and args (parts that will be replaced with the argument)
struct FmtPartInfo {
    enum {
        TpVerbatim,
        TpArg,
    };
    int tp;
    int start;
    int end;
    int argNo; // if tp is TpArg
};

enum { MAX_FMT_ARGS = 6 };
enum { MAX_FMT_PARTS = MAX_FMT_ARGS * 2 };

static void FillFmtPartInfo(FmtPartInfo *fmtPart, int tp, int start, int end, int argNo)
{
    if (start == end) return;
}

int ParseFormatString(const char *fmt, FmtPartInfo *fmtParts)
{
    return 0;
#if 0
    int currPartNo = 0;
    int start = 0;
    const char *s = fmt;
    while (*s) {
        if (*s == '{') {
            FillFmtPartInfo(&fmtParts[currPartNo], FmtPartInfo::TpVerbatim, start, s - fmt - 1, 0);
        }
    }
#endif
}

char *Fmt(const char *fmt, const StrFormatArg& a1, const StrFormatArg& a2, const StrFormatArg& a3,
           const StrFormatArg& a4, const StrFormatArg& a5, const StrFormatArg& a6)
{
    int argsCount = ArgsCount(a1, a2, a3, a4, a5, a6);
    CrashIf(0 == argsCount);
    return str::Format("%d", argsCount);
}

WCHAR *Fmt(const WCHAR *fmt, const StrFormatArg& a1, const StrFormatArg& a2, const StrFormatArg& a3,
            const StrFormatArg& a4, const StrFormatArg& a5, const StrFormatArg& a6)
{
    int argsCount = ArgsCount(a1, a2, a3, a4, a5, a6);
    CrashIf(0 == argsCount);
    return str::Format(L"%d", argsCount);
}

}