#include "BaseUtil.h"
#include "SerializeTxtParser.h"
#include <new>

// unbreak placement new introduced by defining new as DEBUG_NEW
#ifdef new
#undef new
#endif

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

static TxtNode *AllocTxtNode(Allocator *allocator, TxtNodeType nodeType)
{
    void *p = Allocator::AllocZero(allocator, sizeof(TxtNode));
    TxtNode *node = new (p) TxtNode(nodeType);
    if (TextNode != nodeType) {
        p = allocator->Alloc(sizeof(Vec<TxtNode*>));
        node->children = new (p) Vec<TxtNode*>(0, allocator);
    }
    return node;
}

static TxtNode *TxtNodeFromToken(Allocator *allocator, TokenVal& tok, TxtNodeType nodeType)
{
    AssertCrash((TextNode == nodeType) || (StructNode == nodeType));
    TxtNode *node = AllocTxtNode(allocator, nodeType);
    node->lineStart = tok.lineStart;
    node->valStart = tok.valStart;
    node->valEnd = tok.valEnd;
    node->keyStart = tok.keyStart;
    node->keyEnd = tok.keyEnd;
    return node;
}

static bool IsCommentStartChar(char c)
{
    return (';' == c ) || ('#' == c);
}

// TODO: maybe also allow things like:
// foo: [1 3 4]
// i.e. a child on a single line
static void ParseNextToken(TxtParser& parser)
{
    TokenVal& tok = parser.tok;
    ZeroMemory(&tok, sizeof(TokenVal));

    str::Slice& slice = parser.toParse;

Again:
    if (slice.Finished())
        goto Finished;

    // "  foo:  bar  "
    //  ^

    tok.lineStart = slice.curr;

    slice.SkipWsUntilNewline();
    // "  foo:  bar  "
    //    ^

    if (slice.Finished())
        goto Finished;

    char c = slice.CurrChar();
    if (IsCommentStartChar(c)) {
        slice.SkipUntil('\n');
        slice.Skip(1);
        goto Again;
    }

    if ('[' == c || ']' == c) {
        tok.type = ('[' == c) ? TokenArrayStart : TokenClose;
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
        tok.valStart = NULL;
        tok.valEnd = NULL;
    }

    slice.SkipWsUntilNewline();
    // "  foo:  bar  "
    //          ^

    // this is "foo ["
    if ('[' == slice.CurrChar()) {
        tok.type = TokenStructStart;
        if (isKeyVal)
            *tok.keyEnd = 0;
        else {
            CrashIf(NULL == tok.valStart);
            tok.keyStart = tok.valStart;
            tok.keyEnd = slice.curr - 1;
            str::TrimWsEnd(tok.keyStart, tok.keyEnd);
            *tok.keyEnd = 0;
            tok.valStart = NULL;
            tok.valEnd = NULL;
        }
        slice.ZeroCurr();
        slice.Skip(1);
        slice.SkipWsUntilNewline();
        if ('\n' == slice.CurrChar()) {
            slice.ZeroCurr();
            slice.Skip(1);
        }
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
    return;
Finished:
    tok.type = TokenFinished;
}

static void ParseNodes(TxtParser& parser)
{
    TxtNode *currNode = NULL;
    for (;;) {
        ParseNextToken(parser);
        TokenVal& tok = parser.tok;

        if (TokenFinished == tok.type) {
            // we expect to end up with the implicit array node we created at start
            if (parser.nodes.Count() != 1)
                goto Failed;
            return;
        }

        if (TokenString == tok.type || TokenKeyVal == tok.type) {
            currNode = TxtNodeFromToken(parser.allocator, tok, TextNode);
        } else if (TokenArrayStart == tok.type) {
            currNode = AllocTxtNode(parser.allocator, ArrayNode);
        } else if (TokenStructStart == tok.type) {
            currNode = TxtNodeFromToken(parser.allocator, tok, StructNode);
        } else {
            CrashIf(TokenClose != tok.type);
            // if the only node left is the implict array node we created,
            // this is an error
            if (1 == parser.nodes.Count())
                goto Failed;
            parser.nodes.Pop();
            continue;
        }
        TxtNode *currParent = parser.nodes.At(parser.nodes.Count() - 1);
        currParent->children->Append(currNode);
        if (TextNode != currNode->type)
            parser.nodes.Append(currNode);
    }
Failed:
    parser.failed = true;
}

void TxtParser::SetToParse(char *s, size_t sLen)
{
    size_t n = str::NormalizeNewlinesInPlace(s, s + sLen);
    toParse.Init(s, n);

    // we create an implicit array node to hold the nodes we'll parse
    CrashIf(0 != nodes.Count());
    nodes.Append(AllocTxtNode(allocator, ArrayNode));
}

bool ParseTxt(TxtParser& parser)
{
    ParseNodes(parser);
    if (parser.failed)
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

static void PrettyPrintKeyVal(TxtNode *curr, int nest, str::Str<char>& res)
{
    AppendNest(res, nest);
    if (curr->keyStart) {
        AppendWsTrimEnd(res, curr->keyStart, curr->keyEnd);
        if (StructNode != curr->type)
            res.Append(" = ");
    }
    AppendWsTrimEnd(res, curr->valStart, curr->valEnd);
    if (StructNode != curr->type)
        res.Append("\n");
}

static void PrettyPrintNode(TxtNode *curr, int nest, str::Str<char>& res)
{
    if (TextNode == curr->type) {
        PrettyPrintKeyVal(curr, nest, res);
        return;
    }

    if (StructNode == curr->type) {
        PrettyPrintKeyVal(curr, nest, res);
        res.Append(" [\n");
    } else if (nest >= 0) {
        CrashIf(ArrayNode != curr->type);
        AppendNest(res, nest);
        res.Append("[\n");
    }

    TxtNode *child;
    for (size_t i = 0; i < curr->children->Count(); i++) {
        child = curr->children->At(i);
        PrettyPrintNode(child, nest + 1, res);
    }

    if (nest >= 0) {
        AppendNest(res, nest);
        res.Append("]\n");
    }
}

char *PrettyPrintTxt(TxtParser& parser)
{
    str::Str<char> res;
    PrettyPrintNode(parser.nodes.At(0), -1, res);
    return res.StealData();
}
