#include "BaseUtil.h"
#include "SerializeTxtParser.h"

/*
TODO:
 - allow "foo = bar" in addition to "foo: bar"
*/

/*
This is a parser for a tree-like text format:

foo [
  key: val
  k2 [
    val
    mulit-line values are possible
  ]
]


This is not a very strict format. On purpose it doesn't try to strictly break
things into key/values, just to decode tree structure and *help* to interpret
a given line either as a simple string or key/value pair. It's
up to the caller to interpret the data.
*/

static bool IsCommentChar(char c)
{
    return (';' == c ) || ('#' == c);
}

// TODO: maybe also allow things like:
// foo: [1 3 4]
// i.e. a child on a single line
static void ParseNextToken(TxtParser& parser)
{
    TokenVal& tok = parser.tok;
    parser.prevToken = tok.type;
    ClearToken(parser.tok);

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
    bool wasUnescaped = false;
NextLine:
    slice.SkipUntil('\n');
    // "  foo:  bar  "
    //               ^
    if ('\\' == slice.PrevChar()) {
        wasUnescaped = true;
        slice.curr[-1] = 0;
        slice.Skip(1);
        goto NextLine;
    }
    if (wasUnescaped) {
        // we replaced '\' with 0, we need to remove those zeroes
        char *s = tok.valStart;
        char *end = slice.curr;
        char *dst = s;
        while (s < end) {
            if (*s)
                *dst++ = *s;
            s++;
        }
        *dst = 0;
        tok.valEnd = dst;
    } else {
        tok.valEnd = slice.curr;
    }
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
    TxtNode *firstNode = NULL;
    TxtNode *currNode = NULL;
    int arrayNest = 0;
    for (;;) {
        if (parser.encounteredError)
            return NULL;

        if (0 == parser.bracketNesting && parser.toParse.Finished())
            return firstNode;

        ParseNextToken(parser);
        TokenVal& tok = parser.tok;

        if (TokenError == tok.type) {
            parser.encounteredError = true;
            return NULL;
        }

        if (TokenString == tok.type || TokenKeyVal == tok.type) {
            TxtNode *tmp = TxtNodeFromToken(parser.allocator, tok);
            if (NULL == firstNode) {
                firstNode = tmp;
                CrashIf(currNode);
                currNode = tmp;
            } else {
                currNode->next = tmp;
                currNode = tmp;
            }
        } else if (TokenOpen == tok.type) {
            if ((TokenOpen == parser.prevToken) || (TokenClose == parser.prevToken)) {
                // array element
                ++arrayNest;
                TxtNode *tmp = TxtNodeFromToken(parser.allocator, tok);
                tmp->child = ParseNextNode(parser);
                if (tmp->child == NULL) {
                    // TODO: is it valid?
                    parser.encounteredError = true;
                }
                if (parser.encounteredError)
                    return NULL;
                if (NULL == firstNode) {
                    firstNode = tmp;
                    currNode = tmp;
                } else {
                    currNode->next = tmp;
                    currNode = tmp;
                }
            } else {
                ++parser.bracketNesting;
                currNode->child = ParseNextNode(parser);
                // note: it's valid for currNode->child to be NULL. It
                // corresponds to an empty structure i.e.:
                // foo [
                // ]
                if (parser.encounteredError)
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
                if (parser.bracketNesting < 0) {
                    // bad input!
                    parser.encounteredError = true;
                }
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
                while (curr) {
                    AppendNest(res, nest);
                    res.Append("[\n");
                    PrettyPrintNode(curr->child, nest + 1, res);
                    AppendNest(res, nest);
                    res.Append("]\n");
                    curr = curr->next;
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
