/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef TxtParser_h
#define TxtParser_h

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

    explicit TxtNode(TxtNodeType tp) {
        type = tp;
    }
    ~TxtNode() { }

    size_t KeyLen() const {
        return keyEnd - keyStart;
    }

    size_t ValLen() const {
        return valEnd - valStart;
    }

    bool IsArray() const {
        return ArrayNode == type;
    }

    bool IsStruct() const {
        return StructNode == type;
    }

    // TODO: move to TxtParser.cpp
    bool IsStructWithName(const char *name, size_t nameLen) const {
        if (StructNode != type)
            return false;
        if (nameLen != KeyLen())
            return false;
        return str::EqNI(keyStart, name, nameLen);
    }

    bool IsStructWithName(const char *name) const {
        return IsStructWithName(name, str::Len(name));
    }

    bool IsText() const {
        return TextNode == type;
    }

    // TODO: move to TxtParser.cpp
    bool IsTextWithKey(const char *name) const {
        if (!keyStart)
            return false;
        size_t nameLen = str::Len(name);
        if (nameLen != KeyLen())
            return false;
        return str::EqNI(keyStart, name, nameLen);
    }

    // TODO: move to TxtParser.cpp
    char *KeyDup() const {
        if (!keyStart)
            return NULL;
        return str::DupN(keyStart, KeyLen());
    }

    // TODO: move to TxtParser.cpp
    char *ValDup() const {
        if (!valStart)
            return NULL;
        return str::DupN(valStart, ValLen());
    }
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
