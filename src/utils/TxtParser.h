/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#define SERIALIZE_ESCAPE_CHAR '$'

struct TxtNode {
    enum class Type {
        Unknown,
        Struct,
        Array,
        Text,
    };

    Type type{Type::Unknown};

    // for storing children, first goes into firstChild and the
    // rest are linked as sibling
    TxtNode* firstChild{nullptr};
    TxtNode* sibling{nullptr};

    char* lineStart{nullptr};
    char* valStart{nullptr};
    char* valEnd{nullptr};
    char* keyStart{nullptr};
    char* keyEnd{nullptr};

    explicit TxtNode(TxtNode::Type tp);
    TxtNode(const TxtNode& other) = delete;
    TxtNode& operator=(const TxtNode& other) = delete;

    void AddChild(TxtNode*);

    size_t KeyLen() const;
    size_t ValLen() const;
    bool IsArray() const;
    bool IsStruct() const;
    bool IsStructWithName(const char* name, size_t nameLen) const;
    bool IsStructWithName(const char* name) const;
    bool IsText() const;
    bool IsTextWithKey(const char* name) const;
    char* KeyDup() const;
    char* ValDup() const;
};

struct Token {
    enum class Type {
        Finished = 0,
        ArrayStart,  // [
        StructStart, // foo [
        Close,       // ]
        KeyVal,      // foo: bar
        String,      // foo
    };

    Type type{Type::Finished};

    // TokenString, TokenKeyVal
    char* lineStart{nullptr};
    char* valStart{nullptr};
    char* valEnd{nullptr};

    // TokenKeyVal
    char* keyStart{nullptr};
    char* keyEnd{nullptr};
};

struct TxtParser {
    PoolAllocator allocator;
    AutoFree data;

    str::Slice toParse;
    Token tok;
    char escapeChar{SERIALIZE_ESCAPE_CHAR};
    bool failed{false};
    Vec<TxtNode*> nodes;

    TxtNode* AllocTxtNode(TxtNode::Type);
    TxtNode* AllocTxtNodeFromToken(const Token&, TxtNode::Type);

    void SetToParse(const std::string_view&);
};

bool ParseTxt(TxtParser& parser);
str::Str PrettyPrintTxt(const TxtParser& parser);
