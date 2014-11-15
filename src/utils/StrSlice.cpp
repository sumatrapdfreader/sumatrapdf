/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "StrSlice.h"

namespace str {

inline bool IsWsOrNewline(char c)
{
    return ( ' ' == c) ||
           ('\r' == c) ||
           ('\t' == c) ||
           ('\n' == c);
}

inline bool IsWsNoNewline(char c)
{
    return ( ' ' == c) ||
           ('\r' == c) ||
           ('\t' == c);
}

// returns number of characters skipped
ptrdiff_t Slice::SkipWsUntilNewline()
{
    char *start = curr;
    for (; !Finished(); ++curr) {
        if (!IsWsNoNewline(*curr))
            break;
    }
    return curr - start;
}

// returns number of characters skipped
ptrdiff_t Slice::SkipNonWs()
{
    char *start = curr;
    for (; !Finished(); ++curr) {
        if (IsWsOrNewline(*curr))
            break;
    }
    return curr - start;
}

// advances to a given character or end
ptrdiff_t Slice::SkipUntil(char toFind)
{
    char *start = curr;
    for (; !Finished(); ++curr) {
        if (*curr == toFind)
            break;
    }
    return curr - start;
}

char Slice::PrevChar() const
{
    if (curr > begin)
        return curr[-1];
    return 0;
}

char Slice::CurrChar() const
{
    if (curr < end)
        return *curr;
    return 0;
}

// skip up to n characters
// returns the number of characters skipped
ptrdiff_t Slice::Skip(int n)
{
    char *start = curr;
    while ((curr < end) && (n > 0)) {
        ++curr;
        --n;
    }
    return curr - start;
}

void Slice::ZeroCurr()
{
    if (curr < end)
        *curr = 0;
}

} // namespace str
