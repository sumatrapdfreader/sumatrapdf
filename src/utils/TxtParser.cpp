/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "StrSlice.h"
#include "TxtParser.h"
#include <new>  // for placement new

// unbreak placement new introduced by defining new as DEBUG_NEW
#ifdef new
#undef new
#endif

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
    CrashIf(!node);
    if (TextNode != nodeType) {
        p = Allocator::AllocZero(allocator, sizeof(Vec<TxtNode*>));
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

// unescapes a string until a newline (\n)
// returns the end of unescaped string
static char *UnescapeLineInPlace(char *&sInOut, char *e, char escapeChar)
{
    char *s = sInOut;
    char *dst = s;
    while ((s < e) && (*s != '\n')) {
        if (escapeChar != *s) {
            *dst++ = *s++;
            continue;
        }

        // ignore unexpected lone escape char
        if (s+1 >= e) {
            *dst++ = *s++;
            continue;
        }

        ++s;
        char c = *s++;
        if (c == escapeChar) {
            *dst++ = escapeChar;
            continue;
        }

        switch (c) {
        case '[':
        case ']':
            *dst++ = c;
            break;
        case 'r':
            *dst++ = '\r';
            break;
        case 'n':
            *dst++ = '\n';
            break;
        default:
            // invalid escaping sequence. we preserve it
            *dst++ = escapeChar;
            *dst++ = c;
            break;
        }
    }
    sInOut = s;
    return dst;
}

// parses "foo[: | (WS*=]WS*[WS*\n" (WS == whitespace
static bool ParseStructStart(TxtParser& parser)
{
    // work on a copy in case we fail
    str::Slice slice = parser.toParse;

    char *keyStart = slice.curr;
    slice.SkipNonWs();
    // "foo:  ["
    //      ^

    char *keyEnd = slice.curr;
    if (':' == slice.PrevChar()) {
        // "foo:  [  "
        //     ^ <- keyEnd
        keyEnd--;
    } else {
        slice.SkipWsUntilNewline();
        if ('=' == slice.CurrChar()) {
            // "foo  =  ["
            //       ^
            slice.Skip(1);
            // "foo  =  ["
            //        ^
        }
    }
    slice.SkipWsUntilNewline();
    // "foo  =  [  "
    //          ^
    if ('[' != slice.CurrChar())
        return false;
    slice.Skip(1);
    slice.SkipWsUntilNewline();
    // "foo  =  [  "
    //             ^
    if (!(slice.Finished() || ('\n' == slice.CurrChar())))
        return false;

    TokenVal& tok = parser.tok;
    tok.type = TokenStructStart;
    tok.keyStart = keyStart;
    tok.keyEnd = keyEnd;
    *keyEnd = 0;
    if ('\n' == slice.CurrChar()) {
        slice.ZeroCurr();
        slice.Skip(1);
    }

    parser.toParse = slice;
    return true;
}

// parses "foo:WS${rest}" or "fooWS=WS${rest}"
// if finds this pattern, sets parser.tok.keyStart and parser.tok.keyEnd
// and positions slice at the beginning of ${rest}
static bool ParseKey(TxtParser& parser)
{
    str::Slice slice = parser.toParse;
    char *keyStart = slice.curr;
    slice.SkipNonWs();
    // "foo:  bar"
    //      ^

    char *keyEnd = slice.curr;
    if (':' == slice.PrevChar()) {
        // "foo:  bar"
        //     ^ <- keyEnd
        keyEnd--;
    } else {
        slice.SkipWsUntilNewline();
        if ('=' != slice.CurrChar())
            return false;
        // "foo  =  bar"
        //       ^
        slice.Skip(1);
        // "foo  =  bar"
        //        ^
    }
    slice.SkipWsUntilNewline();
    // "foo  =   bar "
    //           ^

    parser.tok.keyStart = keyStart;
    parser.tok.keyEnd = keyEnd;

    parser.toParse = slice;
    return true;
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
    // "  foo:  bar  "
    //  ^
    tok.lineStart = slice.curr;
    slice.SkipWsUntilNewline();
    if (slice.Finished()) {
        tok.type = TokenFinished;
        return;
    }

    // "  foo:  bar  "
    //    ^

    // skip comments
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

    if (ParseStructStart(parser))
        return;

    tok.type = TokenString;
    ParseKey(parser);

    // "  foo:  bar"
    //          ^
    tok.valStart = slice.curr;
    char *origEnd = slice.curr;
    char *valEnd = UnescapeLineInPlace(origEnd, slice.end, parser.escapeChar);
    CrashIf((origEnd < slice.end) && (*origEnd != '\n'));
    if (valEnd < slice.end)
        *valEnd = 0;
    tok.valEnd = valEnd;

    slice.curr = origEnd;
    slice.ZeroCurr();
    slice.Skip(1);
    return;
}

static void ParseNodes(TxtParser& parser)
{
    TxtNode *currNode = nullptr;
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
        TxtNode *currParent = parser.nodes.at(parser.nodes.Count() - 1);
        currParent->children->Append(currNode);
        if (TextNode != currNode->type)
            parser.nodes.Append(currNode);
    }
Failed:
    parser.failed = true;
}

static void SkipUtf8Bom(char *& s, size_t& sLen)
{
    if (sLen >= 3 && str::EqN(s, UTF8_BOM, 3)) {
        s += 3;
        sLen -= 3;
    }
}

// we will modify s in-place
void TxtParser::SetToParse(char *s, size_t sLen)
{
    char *tmp = str::conv::UnknownToUtf8(s, sLen);
    if (tmp != s) {
        toFree = tmp;
        s = tmp;
        sLen = str::Len(s);
    }
    SkipUtf8Bom(s, sLen);
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
            res.Append(" + ");
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
        res.Append(" + [\n");
    } else if (nest >= 0) {
        CrashIf(ArrayNode != curr->type);
        AppendNest(res, nest);
        res.Append("[\n");
    }

    TxtNode *child;
    for (size_t i = 0; i < curr->children->Count(); i++) {
        child = curr->children->at(i);
        PrettyPrintNode(child, nest + 1, res);
    }

    if (nest >= 0) {
        AppendNest(res, nest);
        res.Append("]\n");
    }
}

char *PrettyPrintTxt(const TxtParser& parser)
{
    str::Str<char> res;
    PrettyPrintNode(parser.nodes.at(0), -1, res);
    return res.StealData();
}
