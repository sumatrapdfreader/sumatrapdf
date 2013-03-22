#include "BaseUtil.h"
#include "SerializeTxtParser.h"

namespace str {

// returns number of characters skipped
int Slice::SkipWsUntilNewline()
{
    char c;
    char *start = curr;
    while (!Finished()) {
        c = *curr++;
        if (!IsWs(c))
            break;
    }
    return curr - start;
}

inline bool IsWsOrNewline(char c)
{
    return ( ' ' == c) ||
           ('\r' == c) ||
           ('\t' == c) ||
           ('\n' == c);
}

// returns number of characters skipped
int Slice::SkipNonWs()
{
    char c;
    char *start = curr;
    while (!Finished()) {
        c = *curr++;
        if (IsWsOrNewline(c))
            break;
    }
    return curr - start;
}

// advances to a given character or end
int Slice::SkipUntil(char toFind)
{
    char c;
    char *start = curr;
    while (!Finished()) {
        c = *curr++;
        if (c == toFind)
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
int Slice::Skip(int n)
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

enum Token {
    TokenOpen, // '['
    TokenClose, // ']'
    TokenString,
    TokenKeyVal, // foo: bar
    TokenError
};

struct TokenVal {
    Token   type;
    char *  lineStart;
    char *  keyStart;
    // TODO: more data for other tokens ?
};

static void ParseNextToken(TxtParser& parser, TokenVal& tok)
{
    int n;
    tok.type = TokenError;
    str::Slice& slice = parser.s;
    if (slice.Finished())
        return;

    tok.lineStart = slice.curr;

    slice.SkipWsUntilNewline();
    if (slice.Finished())
        return;

    if ('[' == slice.CurrChar()) {
        slice.ZeroCurr();
        slice.Skip(1);
        tok.type = TokenOpen;
        return;
    }

    if (']' == slice.CurrChar()) {
        slice.ZeroCurr();
        slice.Skip(1);
        tok.type = TokenClose;
        return;
    }

    // TODO: maybe also allow things like:
    // foo: [1 3 4]
    // i.e. a child on a single line

    // TODO: must be a line
    // TODO: handle:
    // "foo ["
    // on a single line
    tok.type = TokenString;
    n = slice.SkipUntil('\n');
    slice.ZeroCurr();
}

static TxtNode *ParseNextNode(TxtParser& parser)
{
    TokenVal tok;
    //TxtNode *firstNode = NULL;
    //TxtNode *currNode = NULL;
    for (;;) {
        ParseNextToken(parser, tok);
        if (TokenError == tok.type)
            return NULL;
    }
}

/*
This is a parser for a tree-like text format:

foo [
  key: val
  k2 [
    another val
    and another
  ]
]

On purpose it doesn't try to always break things into key/values, just
to decode tree structure.
*/
bool ParseTxt(TxtParser& parser)
{
    // TODO: first, normalize newlines and remove empty lines
    parser.firstNode = ParseNextNode(parser);
    return false;
}
