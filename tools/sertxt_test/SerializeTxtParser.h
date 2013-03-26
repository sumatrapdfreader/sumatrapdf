#ifndef SerializeTxtParser_h
#define SerializeTxtParser_h

#include "StrSlice.h"

enum Token {
    TokenArrayStart,   // [
    TokenStructStart,  // foo [
    TokenClose,        // ]
    TokenKeyVal,       // foo: bar
    TokenString,       // foo
    TokenFinished,
};

enum TxtNodeType {
    StructNode,
    ArrayNode,
    TextNode,
};

struct TxtNode {
    TxtNodeType     type;
    Vec<TxtNode*>*  children;

    char *          lineStart;
    char *          valStart;
    char *          valEnd;
    char *          keyStart;
    char *          keyEnd;

    TxtNode(TxtNodeType tp) {
        type = tp;
    }
    ~TxtNode() { }
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

struct TxtParser {
    Allocator *     allocator;
    str::Slice      toParse;
    Token           prevToken;
    TokenVal        tok;
    char            escapeChar;
    bool            failed;
    Vec<TxtNode*>   nodes;

    TxtParser() {
        allocator = new PoolAllocator();
        escapeChar = 0;
        prevToken = TokenFinished;
        failed = false;
    }
    ~TxtParser() {
        delete allocator;
    }
};

bool ParseTxt(TxtParser& parser);
char *PrettyPrintTxt(TxtParser& parser);

#endif
