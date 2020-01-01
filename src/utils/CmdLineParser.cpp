/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/CmdLineParser.h"

// Parses a command line according to the specification at
// http://msdn.microsoft.com/en-us/library/17w5ykft.aspx :
// * arguments are delimited by spaces or tabs
// * whitespace in between two quotation marks are part of a single argument
// * a single backslash in front of a quotation mark prevents this special treatment
// * an even number of backslashes followed by either a backslash and a quotation
//   mark or just a quotation mark is collapsed into half as many backslashes
void ParseCmdLine(const WCHAR* cmdLine, WStrVec& out, int maxParts) {
    if (!cmdLine)
        return;

    str::WStr arg(MAX_PATH / 2);
    const WCHAR* s;

    while (--maxParts != 0) {
        while (str::IsWs(*cmdLine))
            cmdLine++;
        if (!*cmdLine)
            break;

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
                for (s = cmdLine + 1; '\\' == *s; s++)
                    ;
                // backslashes escape only when followed by a quotation mark
                if ('"' == *s)
                    cmdLine++;
            }
            arg.Append(*cmdLine);
        }
        out.Append(arg.StealData());
    }

    if (*cmdLine) {
        while (str::IsWs(*cmdLine))
            cmdLine++;
        if (*cmdLine)
            out.Append(str::Dup(cmdLine));
    }
}
