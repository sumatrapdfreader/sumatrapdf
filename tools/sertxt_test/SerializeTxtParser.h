#ifndef SerializeTxtParser_h
#define SerializeTxtParser_h

#include "StrSlice.h"

enum Token {
    TokenOpen, // '['
    TokenClose, // ']'
    TokenString,
    TokenKeyVal, // foo: bar
    TokenError,
};

struct TxtNode {
    char *      lineStart;
    char *      valStart;
    char *      valEnd;
    char *      keyStart;
    char *      keyEnd;

    TxtNode *   next;
    TxtNode *   child;
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

inline void ClearToken(TokenVal& tok)
{
    ZeroMemory(&tok, sizeof(TokenVal));
    tok.type = TokenError;
}

struct TxtParser {
    Allocator *     allocator;
    TxtNode *       firstNode;
    str::Slice      toParse;
    int             bracketNesting; // nesting level of '[', ']'
    Token           prevToken;
    TokenVal        tok;
    char            escapeChar;
    bool            encounteredError;

    TxtParser() {
        allocator = new PoolAllocator();
        firstNode = NULL;
        bracketNesting = 0;
        escapeChar = 0;
        prevToken = TokenError;
        encounteredError = false;
        ClearToken(tok);
    }
    ~TxtParser() {
        delete allocator;
    }
};

bool ParseTxt(TxtParser& parser);
char *PrettyPrintTxt(TxtParser& parser);

#endif
