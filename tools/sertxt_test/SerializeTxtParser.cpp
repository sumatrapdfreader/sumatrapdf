#include "BaseUtil.h"
#include "SerializeTxtParser.h"

/*
TODO:
 - comments (; and/or #)
 - allow "foo = bar" in addition to "foo: bar"
*/

/*
This is a parser for a tree-like text format:

foo [
  key: val
  k2 [
    val
    another val
  ]
]

This is not a very strict format. On purpose it doesn't try to break
things into key/values, just to decode tree structure and *help* to interpret
a given line either as a simple string or key/value pair. It's
up to the caller to interpret the data.
*/

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
int Slice::SkipWsUntilNewline()
{
    char *start = curr;
    for (; !Finished(); ++curr) {
        if (!IsWsNoNewline(*curr))
            break;
    }
    return curr - start;
}

// returns number of characters skipped
int Slice::SkipNonWs()
{
    char *start = curr;
    for (; !Finished(); ++curr) {
        if (IsWsOrNewline(*curr))
            break;
    }
    return curr - start;
}

// advances to a given character or end
int Slice::SkipUntil(char toFind)
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

    // TokenString, TokenKeyVal
    char *  lineStart;
    char *  valStart;
    char *  valEnd;

    // TokenKeyVal
    char *  keyStart;
    char *  keyEnd;
};

static bool IsCommentChar(char c)
{
    return (';' == c ) || ('#' == c);
}

// TODO: maybe also allow things like:
// foo: [1 3 4]
// i.e. a child on a single line
static void ParseNextToken(TxtParser& parser, TokenVal& tok)
{
    ZeroMemory(&tok, sizeof(TokenVal));
    tok.type = TokenError;

    str::Slice& slice = parser.toParse;

Again:
    if (slice.Finished())
        return;

    // "  foo:  bar  "
    //  ^

    tok.lineStart = slice.curr;

    slice.SkipWsUntilNewline();
    // "  foo:  bar  "
    //    ^

    if (slice.Finished())
        return;

    char c = slice.CurrChar();
    if (IsCommentChar(c)) {
        slice.SkipUntil('\n');
        slice.Skip(1);
        goto Again;
    }

    if ('[' == c || ']' == c) {
        tok.type = ('[' == c) ? TokenOpen : TokenClose;
        slice.ZeroCurr();
        slice.Skip(1);
        slice.SkipWsUntilNewline();
        if (slice.CurrChar() == '\n') {
            slice.ZeroCurr();
            slice.Skip(1);
        }
        return;
    }

    tok.type = TokenString;
    tok.valStart = slice.curr;
    slice.SkipNonWs();
    // "  foo:  bar  "
    //        ^

    bool isKeyVal = (slice.PrevChar() == ':');
    if (isKeyVal) {
        tok.keyStart = tok.valStart;
        tok.keyEnd = slice.curr - 1;
        CrashIf(*tok.keyEnd != ':');
    }

    slice.SkipWsUntilNewline();
    // "  foo:  bar  "
    //          ^

    // this is "foo [", '[' will be returned in subsequent call
    // it'll also be zero'ed, properly terminating this token's valStart
    if ('[' == slice.CurrChar()) {
        // TODO: should this be returned as key instead?
        tok.valEnd = slice.curr;
        // interpret "foo: [" as "foo ["
        if (tok.keyEnd) {
            *tok.keyEnd = 0;
            tok.valEnd = tok.keyEnd;
        }
        tok.keyStart = NULL;
        tok.keyEnd = NULL;
        CrashIf(TokenString != tok.type);
        return;
    }

    // "  foo:  bar  "
    //          ^
    if (isKeyVal) {
        tok.type = TokenKeyVal;
        tok.valStart = slice.curr;
    } else {
        CrashIf(tok.keyStart || tok.keyEnd);
    }
    slice.SkipUntil('\n');
    // "  foo:  bar  "
    //               ^

    tok.valEnd = slice.curr;
    slice.ZeroCurr();
    slice.Skip(1);
}

static TxtNode *TxtNodeFromToken(Allocator *allocator, TokenVal& tok)
{
    TxtNode *node = Allocator::Alloc<TxtNode>(allocator);
    node->lineStart = tok.lineStart;
    node->valStart = tok.valStart;
    node->valEnd = tok.valEnd;
    node->keyStart = tok.keyStart;
    node->keyEnd = tok.keyEnd;
    return node;
}

static TxtNode *ParseNextNode(TxtParser& parser)
{
    TokenVal tok;
    TxtNode *firstNode = NULL;
    TxtNode *currNode = NULL;
    int arrayNest = 0;
    for (;;) {
        if (0 == parser.bracketNesting && parser.toParse.Finished())
            return firstNode;

        ParseNextToken(parser, tok);
        if (TokenError == tok.type)
            return NULL;
        if (TokenString == tok.type || TokenKeyVal == tok.type) {
            TxtNode *tmp = TxtNodeFromToken(parser.allocator, tok);
            if (firstNode == NULL) {
                firstNode = tmp;
                CrashIf(currNode);
                currNode = tmp;
            } else {
                currNode->next = tmp;
                currNode = tmp;
            }
        } else if (TokenOpen == tok.type) {
            if (NULL == currNode) {
                CrashIf(firstNode);
                // '[' that starts an array
                CrashIf(arrayNest != 0);
                arrayNest = 1;
                firstNode = TxtNodeFromToken(parser.allocator, tok);
                currNode = firstNode;
                currNode->child = ParseNextNode(parser);
                if (!currNode->child) // propagate errors
                    return NULL;
            } else if (arrayNest > 0) {
                ++arrayNest;
                TxtNode *tmp = ParseNextNode(parser);
                if (!tmp) // propagate errors
                    return NULL;
                currNode->next = tmp;
                currNode = tmp;
            } else {
                ++parser.bracketNesting;
                currNode->child = ParseNextNode(parser);
                if (!currNode->child) // propagate errors
                    return NULL;
            }
        } else {
            CrashIf(TokenClose != tok.type);
            if (arrayNest > 0) {
                --arrayNest;
                if (0 == arrayNest)
                    return firstNode;
            } else {
                --parser.bracketNesting;
                if (parser.bracketNesting < 0) // bad input!
                    return NULL;
                return firstNode;
            }
        }
    }
}

bool ParseTxt(TxtParser& parser)
{
    CrashIf(!parser.allocator);
    str::Slice& slice = parser.toParse;
    size_t n = str::NormalizeNewlinesInPlace(slice.begin, slice.end);
    slice.end = slice.begin + n;
    parser.firstNode = ParseNextNode(parser);
    if (parser.firstNode == NULL)
        return false;
    return true;
}

static void AppendNest(str::Str<char>& s, int nest)
{
    while (nest > 0) {
        s.Append("  ");
        --nest;
    }
}

static void AppendWsTrimEnd(str::Str<char>& res, char *s, char *e)
{
    str::TrimWsEnd(s, e);
    res.Append(s, e - s);
}

static void PrettyPrintVal(TxtNode *curr, int nest, str::Str<char>& res)
{
    AppendNest(res, nest);
    if (curr->keyStart) {
        AppendWsTrimEnd(res, curr->keyStart, curr->keyEnd);
        res.Append(" = ");
    }
    AppendWsTrimEnd(res, curr->valStart, curr->valEnd);
}

static void PrettyPrintNode(TxtNode *curr, int nest, str::Str<char>& res)
{
    while (curr) {
        if (curr->child) {
            if (curr->valStart != NULL) {
                // dict
                PrettyPrintVal(curr, nest, res);
                res.Append(" [\n");
                PrettyPrintNode(curr->child, nest + 1, res);
                AppendNest(res, nest);
                res.Append("]\n");
            } else {
                // array
                TxtNode *arrEl = curr;
                AppendNest(res, nest);
                res.Append("[\n");
                PrettyPrintNode(arrEl->child, nest + 1, res);
                AppendNest(res, nest);
                res.Append("]\n");
                arrEl = arrEl->next;
                while (arrEl) {
                    AppendNest(res, nest);
                    res.Append("[\n");
                    PrettyPrintNode(arrEl, nest + 1, res);
                    AppendNest(res, nest);
                    res.Append("]\n");
                    arrEl = arrEl->next;
                }
                return;
            }
        } else {
            PrettyPrintVal(curr, nest, res);
            res.Append("\n");
        }
        curr = curr->next;
    }
}

char *PrettyPrintTxt(TxtParser& parser)
{
    str::Str<char> res;
    PrettyPrintNode(parser.firstNode, 0, res);
    return res.StealData();
}
