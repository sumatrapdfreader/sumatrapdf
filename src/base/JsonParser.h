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
// Path is only valid during Visit; the parser reuses/mutates the list after.
constexpr char kSegKey = '/';
constexpr char kSegIdx = 'i';
constexpr char kSegAny = '*';

struct ValueVisitor {
    virtual bool Visit(StrNode* path, Str value, Type type) = 0;
    virtual ~ValueVisitor() = default;
};

bool Parse(Str data, ValueVisitor* visitor);
TempStr EscapeStrTemp(Str s);

// path helpers (pattern segments use the same encoding as path nodes)
bool PathMatch(StrNode* path, Str a = {}, Str b = {}, Str c = {}, Str d = {}, Str e = {}, Str f = {});
StrNode* PathBuildTemp(Str a, Str b = {}, Str c = {}, Str d = {}, Str e = {}, Str f = {});
TempStr PathFormatTemp(StrNode* path);
StrNode* PathNth(StrNode* path, int n);
int PathSegIndex(StrNode* seg);
Str PathSegKey(StrNode* seg);

} // namespace json
