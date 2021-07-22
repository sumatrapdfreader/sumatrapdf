/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/CmdLineParser.h"

void ParseCmdLine(const char* cmdLine, WStrVec& out, int maxParts) {
    if (!cmdLine) {
        return;
    }
    auto s = ToWstrTemp(cmdLine);
    return ParseCmdLine(s.Get(), out, maxParts);
}

// Parses a command line according to the specification at
// http://msdn.microsoft.com/en-us/library/17w5ykft.aspx :
// * arguments are delimited by spaces or tabs
// * whitespace in between two quotation marks are part of a single argument
// * a single backslash in front of a quotation mark prevents this special treatment
// * an even number of backslashes followed by either a backslash and a quotation
//   mark or just a quotation mark is collapsed into half as many backslashes
void ParseCmdLine(const WCHAR* cmdLine, WStrVec& out, int maxParts) {
    if (!cmdLine) {
        return;
    }

    str::WStr arg(MAX_PATH / 2);
    const WCHAR* s;

    while (--maxParts != 0) {
        while (str::IsWs(*cmdLine)) {
            cmdLine++;
        }
        if (!*cmdLine) {
            break;
        }

        bool insideQuotes = false;
        for (; *cmdLine; cmdLine++) {
            if ('"' == *cmdLine) {
                insideQuotes = !insideQuotes;
                continue;
            }
            if (!insideQuotes && str::IsWs(*cmdLine)) {
                break;
            }
            if ('\\' == *cmdLine) {
                for (s = cmdLine + 1; '\\' == *s; s++) {
                    ;
                }
                // backslashes escape only when followed by a quotation mark
                if ('"' == *s) {
                    cmdLine++;
                }
            }
            arg.Append(*cmdLine);
        }
        out.Append(arg.StealData());
    }

    if (*cmdLine) {
        while (str::IsWs(*cmdLine)) {
            cmdLine++;
        }
        if (*cmdLine) {
            out.Append(str::Dup(cmdLine));
        }
    }
}

bool CouldBeArg(const WCHAR* s) {
    WCHAR c = *s;
    return (c == L'-') || (c == L'/');
}

ArgsIter::~ArgsIter() {
    LocalFree(args);
}

ArgsIter::ArgsIter(const WCHAR* cmdLine) {
    args = CommandLineToArgvW(cmdLine, &nArgs);
}

const WCHAR* ArgsIter::NextArg() {
    if (curr >= nArgs) {
        return nullptr;
    }
    currArg = args[curr++];
    return currArg;
}

const WCHAR* ArgsIter::EatParam() {
    // doesn't change currArg
    if (curr >= nArgs) {
        return nullptr;
    }
    return args[curr++];
}

void ArgsIter::RewindParam() {
    // undo EatParam()
    --curr;
    ReportIf(curr < 1);
}

// additional param is one in addition to the default first param
// they start at 1
// returns nullptr if no additional param
const WCHAR* ArgsIter::AdditionalParam(int n) const {
    ReportIf(n < 1);
    if (curr + n - 1 >= nArgs) {
        return nullptr;
    }

    // we assume that param cannot be args (i.e. start with - or /
    for (int i = 0; i < n; i++) {
        const WCHAR* s = args[curr + i];
        if (CouldBeArg(s)) {
            return nullptr;
        }
    }
    return args[curr + n - 1];
}
