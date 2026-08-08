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
// Path is only valid during the VisitFn call; the parser reuses/mutates the list after.
constexpr char kSegKey = '/';
constexpr char kSegIdx = 'i';
constexpr char kSegAny = '*';

// One primitive value from Parse. path/value are only valid for the duration of
// the VisitFn call (path may be mutated when the parser pops; value is a
// temp-arena copy). Set stop to true to cancel further visits (Parse still
// returns true).
struct Value {
    StrNode* path = nullptr;
    Str value;
    Type type = Type::String;
    bool stop = false;
};

using VisitFn = Func1<Value*>;

bool Parse(Str data, const VisitFn& onValue);
TempStr EscapeStrTemp(Str s);

// path helpers (pattern segments use the same encoding as path nodes)
bool PathMatch(StrNode* path, Str a = {}, Str b = {}, Str c = {}, Str d = {}, Str e = {}, Str f = {});
StrNode* PathBuildTemp(Str a, Str b = {}, Str c = {}, Str d = {}, Str e = {}, Str f = {});
TempStr PathFormatTemp(StrNode* path);
StrNode* PathNth(StrNode* path, int n);
int PathSegIndex(StrNode* seg);
Str PathSegKey(StrNode* seg);

} // namespace json
