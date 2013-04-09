/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef SerializeTxtParser_h
#define SerializeTxtParser_h

#define SERIALIZE_ESCAPE_CHAR '$'

#include "StrSlice.h"

enum Token {
    TokenFinished = 0,
    TokenArrayStart,   // [
    TokenStructStart,  // foo [
    TokenClose,        // ]
    TokenKeyVal,       // foo: bar
    TokenString,       // foo
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
    TokenVal        tok;
    char            escapeChar;
    bool            failed;
    Vec<TxtNode*>   nodes;
    char *          toFree;

    TxtParser() {
        allocator = new PoolAllocator();
        escapeChar = SERIALIZE_ESCAPE_CHAR;
        failed = false;
        toFree = NULL;
    }
    ~TxtParser() {
        delete allocator;
        free(toFree);
    }
    void SetToParse(char *s, size_t sLen);
};

bool ParseTxt(TxtParser& parser);
char *PrettyPrintTxt(const TxtParser& parser);

#endif
