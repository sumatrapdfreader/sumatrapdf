/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: FreeBSD (see COPYING) */

#ifndef CmdLineParser_h
#define CmdLineParser_h

#include "Vec.h"

class CmdLineParser : public StrVec {
public:
    /* 'cmdLine' contains one or several arguments can be:
        - escaped, in which case it starts with '"', ends with '"' and
          each '"' that is part of the name is escaped with '\\'
        - unescaped, in which case it start with != '"' and ends with ' ' or '\0' */
    CmdLineParser(const TCHAR *cmdLine)
    {
        while (cmdLine) {
            // skip whitespace
            while (_istspace(*cmdLine))
                cmdLine++;
            if ('"' == *cmdLine)
                cmdLine = ParseQuoted(cmdLine);
            else if ('\0' != *cmdLine)
                cmdLine = ParseUnquoted(cmdLine);
            else
                cmdLine = NULL;
        }
    }

private:
    /* returns the next character in '*txt' that isn't a backslash */
    const TCHAR SkipBackslashs(const TCHAR *txt)
    {
        assert(txt && '\\' == *txt);
        while ('\\' == *++txt);
        return *txt;
    }

    /* appends the next quoted argument and returns the position after it */
    const TCHAR *ParseQuoted(const TCHAR *arg)
    {
        assert(arg && '"' == *arg);
        arg++;

        str::Str<TCHAR> txt(str::Len(arg) / 2);
        const TCHAR *next;
        for (next = arg; *next && *next != '"'; next++) {
            // skip escaped quotation marks according to
            // http://msdn.microsoft.com/en-us/library/17w5ykft.aspx
            if ('\\' == *next && '"' == SkipBackslashs(next))
                next++;
            txt.Append(*next);
        }
        this->Append(txt.StealData());

        if ('"' == *next)
            next++;
        return next;
    }

    /* appends the next unquoted argument and returns the position after it */
    const TCHAR *ParseUnquoted(const TCHAR *arg)
    {
        assert(arg && *arg && '"' != *arg && !_istspace(*arg));

        const TCHAR *next;
        // contrary to http://msdn.microsoft.com/en-us/library/17w5ykft.aspx
        // we don't treat quotation marks or backslashes in non-quoted
        // arguments in any special way
        for (next = arg; *next && !_istspace(*next); next++);
        this->Append(str::DupN(arg, next - arg));

        return next;
    }
};

#endif

