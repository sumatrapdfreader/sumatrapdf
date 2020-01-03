/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "StrSlice.h"
#include "TxtParser.h"
#include <new> // for placement new

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

TxtNode::TxtNode(TxtNode::Type tp) {
    type = tp;
}

void TxtNode::AddChild(TxtNode* child) {
    if (firstChild == nullptr) {
        firstChild = child;
        return;
    }
    TxtNode** curr = &(firstChild->sibling);
    while (*curr != nullptr) {
        curr = &((*curr)->sibling);
    }
    *curr = child;
}

size_t TxtNode::KeyLen() const {
    return keyEnd - keyStart;
}

size_t TxtNode::ValLen() const {
    return valEnd - valStart;
}

bool TxtNode::IsArray() const {
    return Type::Array == type;
}

bool TxtNode::IsStruct() const {
    return Type::Struct == type;
}

bool TxtNode::IsStructWithName(const char* name, size_t nameLen) const {
    if (Type::Struct != type) {
        return false;
    }
    if (nameLen != KeyLen()) {
        return false;
    }
    return str::EqNI(keyStart, name, nameLen);
}

bool TxtNode::IsStructWithName(const char* name) const {
    return IsStructWithName(name, str::Len(name));
}

bool TxtNode::IsText() const {
    return Type::Text == type;
}

bool TxtNode::IsTextWithKey(const char* name) const {
    if (Type::Text != type) {
        return false;
    }
    if (!keyStart) {
        return false;
    }
    size_t nameLen = str::Len(name);
    if (nameLen != KeyLen()) {
        return false;
    }
    return str::EqNI(keyStart, name, nameLen);
}

char* TxtNode::KeyDup() const {
    if (!keyStart) {
        return nullptr;
    }
    return str::DupN(keyStart, KeyLen());
}

char* TxtNode::ValDup() const {
    if (!valStart) {
        return nullptr;
    }
    return str::DupN(valStart, ValLen());
}

TxtNode* TxtParser::AllocTxtNode(TxtNode::Type nodeType) {
    void* p = Allocator::Alloc<TxtNode>(&allocator);
    TxtNode* node = new (p) TxtNode(nodeType);
    CrashIf(!node);
    return node;
}

TxtNode* TxtParser::AllocTxtNodeFromToken(const Token& tok, TxtNode::Type nodeType) {
    AssertCrash((TxtNode::Type::Text == nodeType) || (TxtNode::Type::Struct == nodeType));
    TxtNode* node = AllocTxtNode(nodeType);
    node->lineStart = tok.lineStart;
    node->valStart = tok.valStart;
    node->valEnd = tok.valEnd;
    node->keyStart = tok.keyStart;
    node->keyEnd = tok.keyEnd;
    return node;
}

void TxtParser::SetToParse(const std::string_view& str) {
    data = strconv::UnknownToUtf8(str);
    char* d = (char*)data.get();
    size_t dLen = data.size();
    size_t n = str::NormalizeNewlinesInPlace(d, d + dLen);
    toParse.Set(d, n);

    // we create an implicit array node to hold the nodes we'll parse
    CrashIf(0 != nodes.size());
    TxtNode* node = AllocTxtNode(TxtNode::Type::Array);
    nodes.push_back(node);
}

static bool IsCommentStartChar(char c) {
    return (';' == c) || ('#' == c);
}

// unescapes a string until a newline (\n)
// returns the end of unescaped string
static char* UnescapeLineInPlace(char*& sInOut, char* e, char escapeChar) {
    char* s = sInOut;
    char* dst = s;
    while ((s < e) && (*s != '\n')) {
        if (escapeChar != *s) {
            *dst++ = *s++;
            continue;
        }

        // ignore unexpected lone escape char
        if (s + 1 >= e) {
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
static bool ParseStructStart(TxtParser& parser) {
    // work on a copy in case we fail
    str::Slice slice = parser.toParse;

    char* keyStart = slice.curr;
    slice.SkipNonWs();
    // "foo:  ["
    //      ^

    char* keyEnd = slice.curr;
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

    Token& tok = parser.tok;
    tok.type = Token::Type::StructStart;
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
static bool ParseKey(TxtParser& parser) {
    str::Slice slice = parser.toParse;
    char* keyStart = slice.curr;
    slice.SkipNonWs();
    // "foo:  bar"
    //      ^

    char* keyEnd = slice.curr;
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
static void ParseNextToken(TxtParser& parser) {
    Token& tok = parser.tok;
    ZeroMemory(&tok, sizeof(Token));
    str::Slice& slice = parser.toParse;

Again:
    // "  foo:  bar  "
    //  ^
    tok.lineStart = slice.curr;
    slice.SkipWsUntilNewline();
    if (slice.Finished()) {
        tok.type = Token::Type::Finished;
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
        tok.type = ('[' == c) ? Token::Type::ArrayStart : Token::Type::Close;
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

    tok.type = Token::Type::String;
    ParseKey(parser);

    // "  foo:  bar"
    //          ^
    tok.valStart = slice.curr;
    char* origEnd = slice.curr;
    char* valEnd = UnescapeLineInPlace(origEnd, slice.end, parser.escapeChar);
    CrashIf((origEnd < slice.end) && (*origEnd != '\n'));
    if (valEnd < slice.end)
        *valEnd = 0;
    tok.valEnd = valEnd;

    slice.curr = origEnd;
    slice.ZeroCurr();
    slice.Skip(1);
    return;
}

static void ParseNodes(TxtParser& parser) {
    TxtNode* currNode = nullptr;
    for (;;) {
        ParseNextToken(parser);
        Token& tok = parser.tok;

        if (Token::Type::Finished == tok.type) {
            // we expect to end up with the implicit array node we created at start
            if (parser.nodes.size() != 1) {
                goto Failed;
            }
            return;
        }

        if (Token::Type::String == tok.type || Token::Type::KeyVal == tok.type) {
            currNode = parser.AllocTxtNodeFromToken(tok, TxtNode::Type::Text);
        } else if (Token::Type::ArrayStart == tok.type) {
            currNode = parser.AllocTxtNode(TxtNode::Type::Array);
        } else if (Token::Type::StructStart == tok.type) {
            currNode = parser.AllocTxtNodeFromToken(tok, TxtNode::Type::Struct);
        } else {
            CrashIf(Token::Type::Close != tok.type);
            // if the only node left is the implict array node we created,
            // this is an error
            if (1 == parser.nodes.size()) {
                goto Failed;
            }
            parser.nodes.pop_back();
            continue;
        }
        TxtNode* currParent = parser.nodes.at(parser.nodes.size() - 1);
        currParent->AddChild(currNode);
        if (TxtNode::Type::Text != currNode->type) {
            parser.nodes.push_back(currNode);
        }
    }
Failed:
    parser.failed = true;
}

bool ParseTxt(TxtParser& parser) {
    ParseNodes(parser);
    if (parser.failed) {
        return false;
    }
    return true;
}

static void AppendNest(str::Str& s, int nest) {
    while (nest > 0) {
        s.Append("    ");
        --nest;
    }
}

static void AppendWsTrimEnd(str::Str& res, char* s, char* e) {
    str::TrimWsEnd(s, e);
    res.Append(s, e - s);
}

static void PrettyPrintKeyVal(TxtNode* curr, int nest, str::Str& res) {
    AppendNest(res, nest);
    if (curr->keyStart) {
        AppendWsTrimEnd(res, curr->keyStart, curr->keyEnd);
        if (!curr->IsStruct()) {
            res.Append(": ");
        }
    }
    AppendWsTrimEnd(res, curr->valStart, curr->valEnd);
    if (!curr->IsStruct()) {
        res.Append("\n");
    }
}

static void PrettyPrintNode(TxtNode* curr, int nest, str::Str& res) {
    if (curr->IsText()) {
        PrettyPrintKeyVal(curr, nest, res);
        return;
    }

    if (curr->IsStruct()) {
        PrettyPrintKeyVal(curr, nest, res);
        res.Append(" [\n");
    } else if (nest >= 0) {
        CrashIf(!curr->IsArray());
        AppendNest(res, nest);
        res.Append("[\n");
    }

    for (TxtNode* child = curr->firstChild; child != nullptr; child = child->sibling) {
        PrettyPrintNode(child, nest + 1, res);
    }

    if (nest >= 0) {
        AppendNest(res, nest);
        res.Append("]\n");
        if (nest == 0) {
            res.Append("\n");
        }
    }
}

str::Str PrettyPrintTxt(const TxtParser& parser) {
    str::Str res;
    PrettyPrintNode(parser.nodes.at(0), -1, res);
    return res;
}
