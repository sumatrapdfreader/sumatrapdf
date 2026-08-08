/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace json {

enum class Type {
    String,
    Number,
    Bool,
    Null
};

struct ValueVisitor {
    virtual bool Visit(Str path, Str value, Type type) = 0;
    virtual ~ValueVisitor() = default;
};

bool Parse(Str data, ValueVisitor* visitor);
TempStr EscapeStrTemp(Str s);

} // namespace json
