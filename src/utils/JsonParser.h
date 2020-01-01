/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// simple push parser for JSON files (cf. http://www.json.org/ )

namespace json {

enum DataType { Type_String, Type_Number, Type_Bool, Type_Null };

// parsing JSON data will call the ValueVisitor for every
// primitive data value with a string representation of that
// value and a path to it

// e.g. the following JSON data will lead to two calls:
// { "key": [false, { "name": "valu\u0065" }] }
// 1. "/key[0]", "false", Type_Bool
// 2. "/key[1]/name", "value", Type_String

class ValueVisitor {
  public:
    // return false to stop parsing
    virtual bool Visit(const char* path, const char* value, DataType type) = 0;
    virtual ~ValueVisitor() {
    }
};

// data must be UTF-8 encoded and nullptr-terminated
// returns false on error
bool Parse(const char* data, ValueVisitor* visitor);

} // namespace json
