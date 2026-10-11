/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace json {

enum class Type {
    String,
    Number,
    Bool,
    Null
};

// Path is a StrNode list (outermost segment first). Each node's s stores:
//   '/' + key   object key (rest is the key bytes, may contain '/' etc.)
//   'i' + digits  array index
//   '*'         match-only "any segment" (not produced by the parser)
// Path is only valid until the reader moves on; the parser reuses/mutates the list after.
constexpr char kSegKey = '/';
constexpr char kSegIdx = 'i';
constexpr char kSegAny = '*';

// One primitive value. path/value are only valid until the reader's next
// step (path is mutated when the parser pops; value is a temp-arena copy).
struct Value {
    StrNode* path = nullptr;
    Str value;
    Type type = Type::String;
};

// Pull parser: the primitive values of a JSON document, one per step, each
// with the path to it. data must be UTF-8.
//   json::Reader r(data);
//   for (json::Value& v : r) { ... }   // break to stop early
//   if (!r.Ok()) { invalid JSON }
// Ok() is only meaningful once the loop ran to its end: an early break leaves
// the rest of the input unvalidated.
struct Reader {
    Value value; // the current one, after Next() returned true

    explicit Reader(Str data);
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;

    bool Next(); // to the next value; false at the end or on invalid JSON
    bool Ok() const;

    struct iterator {
        Reader* r = nullptr;
        bool atEnd = true;
        Value& operator*() const;
        iterator& operator++(); // ++it
        friend bool operator==(const iterator& a, const iterator& b);
        friend bool operator!=(const iterator& a, const iterator& b);
    };
    iterator begin();
    iterator end();

  private:
    // a container being read: an array counts its elements
    struct Frame {
        bool isObject;
        int idx;
    };
    Str data;
    int off = 0;
    bool started = false;
    bool done = false;
    bool failed = false;
    Arena* arena = nullptr;
    StrNodeList path;
    Vec<Frame> stack;

    bool Fail();
    void PushKey(Str key);
    void PushIdx(int idx);
    bool ReadKey();
    bool StepValue();
    bool StepAfterValue();
};

TempStr EscapeStrTemp(Str s);

// path helpers (pattern segments use the same encoding as path nodes)
bool PathMatch(StrNode* path, Str a = {}, Str b = {}, Str c = {}, Str d = {}, Str e = {}, Str f = {});
StrNode* PathBuildTemp(Str a, Str b = {}, Str c = {}, Str d = {}, Str e = {}, Str f = {});
TempStr PathFormatTemp(StrNode* path);
StrNode* PathNth(StrNode* path, int n);
int PathSegIndex(StrNode* seg);
Str PathSegKey(StrNode* seg);

} // namespace json
