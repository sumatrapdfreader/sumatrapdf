#ifndef SerializeTxtParser_h
#define SerializeTxtParser_h

namespace str {

// a class to help scanning through text
struct Slice {
    char *  begin;
    char *  end;
    char *  curr;

    Slice() : begin(NULL), end(NULL), curr(NULL) { }

    Slice(char *txt, size_t len) {
        Init(txt, len);
    }

    Slice(const Slice& other) {
        this->begin = other.begin;
        this->end = other.end;
        this->curr = other.curr;
    }

    void Init(char *txt, size_t len) {
        begin = txt;
        curr = txt;
        end = txt + len;
    }

    bool Finished() const { return curr >= end; }

    char PrevChar() const;
    char CurrChar() const;
    int SkipWsUntilNewline();
    int SkipUntil(char toFind);
    int SkipNonWs();
    int Skip(int n);
    void ZeroCurr();
};

} // namespace str

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

    TxtParser() {
        allocator = new PoolAllocator();
        firstNode = NULL;
        bracketNesting = 0;
        prevToken = TokenError;
        ClearToken(tok);
    }
    ~TxtParser() {
        delete allocator;
    }
};

bool ParseTxt(TxtParser& parser);
char *PrettyPrintTxt(TxtParser& parser);

#endif
