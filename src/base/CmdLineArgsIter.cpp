/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "base/CmdLineArgsIter.h"

#define REMOVE_FIRST_ARG

// Quote a single argument for a Windows CreateProcessW command line.
// Matches the rules used by CommandLineToArgvW / the MSVC CRT (see
// https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments
// and "Everyone quotes command line arguments the wrong way").
//
// Always wraps the result in double quotes so untrusted content (quotes and
// backslashes) cannot break out of the argument boundary. A naive
// " -> \" replace is not enough: an input ending in \" becomes \\" after that
// replace, which closes the quoted argument early and injects new tokens.
//
// Returns {} if arg.s is null. Empty (len 0) arg becomes "".
// Note: Str's operator bool is false for empty strings (len==0), so check .s.
// Quote for CreateProcessW command lines (Windows argv rules; always quoted).
TempStr QuoteCmdLineArgTemp(Str arg) {
    if (!arg.s) {
        return {};
    }

    // Paths/args usually fit; worst case ~2x arg for backslash doubling + quotes.
    char resScratch[1024]{};
    str::Builder res(Str(resScratch, sizeofi(resScratch)));
    res.AppendChar('"');

    int n = arg.len;
    int i = 0;
    while (i < n) {
        int nBackslashes = 0;
        while (i < n && arg.s[i] == '\\') {
            nBackslashes++;
            i++;
        }
        if (i >= n) {
            // Trailing backslashes before the closing quote must be doubled so
            // they are treated as literal, not as escapes of that quote.
            for (int k = 0; k < nBackslashes * 2; k++) {
                res.AppendChar('\\');
            }
            break;
        }
        if (arg.s[i] == '"') {
            // Backslashes before a quote are doubled, then the quote is escaped.
            for (int k = 0; k < (nBackslashes * 2) + 1; k++) {
                res.AppendChar('\\');
            }
            res.AppendChar('"');
            i++;
        } else {
            for (int k = 0; k < nBackslashes; k++) {
                res.AppendChar('\\');
            }
            res.AppendChar(arg.s[i]);
            i++;
        }
    }

    res.AppendChar('"');
    return ToStrTemp(res);
}

bool CouldBeArg(Str s) {
    if (!s) {
        return false;
    }
    char c = *s.s;
    return (c == '-') || (c == '/');
}

void BuildCmdLineArgs(int argc, char** argv, StrVec& argsOut) {
    for (int i = 0; i < argc; i++) {
        Str arg(argv[i]);
        if (len(arg) == 0) {
            continue;
        }
        argsOut.Append(arg);
    }
}

CmdLineArgsIter::CmdLineArgsIter(int argc, char** argv) {
    BuildCmdLineArgs(argc, argv, args);
    nArgs = len(args);
#if defined(REMOVE_FIRST_ARG)
    curr = 1;
#endif
}

Str CmdLineArgsIter::NextArg() {
    if (curr >= nArgs) {
        return {};
    }
    currArg = args[curr++];
    return currArg;
}

Str CmdLineArgsIter::EatParam() {
    // doesn't change currArg
    if (curr >= nArgs) {
        return {};
    }
    return args[curr++];
}

void CmdLineArgsIter::RewindParam() {
    // undo EatParam()
    --curr;
    ReportIf(curr < 1);
}

// additional param is one in addition to the default first param
// they start at 1
// returns nullptr if no additional param
Str CmdLineArgsIter::AdditionalParam(int n) const {
    ReportIf(n < 1);
    if (curr + n - 1 >= nArgs) {
        return {};
    }

    // we assume that param cannot be args (i.e. start with - or /
    for (int i = 0; i < n; i++) {
        Str s = args[curr + i];
        if (CouldBeArg(s)) {
            return {};
        }
    }
    return args[curr + n - 1];
}

Str CmdLineArgsIter::at(int n) const {
    return args[n];
}

// returns just the params i.e. everything but the first
// arg (which is the name of the command)
// returns nullptr if no args
TempStr CmdLineArgsIter::ParamsTemp() {
    if (nArgs < 2) {
        return {};
    }
    if (nArgs == 2) {
        return Str(args[1]);
    }
    // must concat all the
    TempStr s = Str(args[1]);
    for (int i = 2; i < nArgs; i++) {
        s = str::JoinTemp(s, StrL(" "), args[i]);
    }
    return s;
}
