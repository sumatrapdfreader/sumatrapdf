/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/JsonParser.h"

// must be last due to assert() over-write
#include "base/UtAssert.h"

struct JsonValue {
    Str path;
    Str value;
    json::Type type{json::Type::String};

    JsonValue() = default;
    JsonValue(Str path, Str value, json::Type type = json::Type::String) : path(path), type(type), value(value) {}
};

class JsonVerifier : public json::ValueVisitor {
    const JsonValue* data;
    size_t dataLen;
    size_t idx;

  public:
    JsonVerifier(const JsonValue* data, size_t dataLen) : data(data), dataLen(dataLen), idx(0) {}
    ~JsonVerifier() { utassert(dataLen == idx); }

    virtual bool Visit(StrNode* path, Str value, json::Type type) {
        utassert(idx < dataLen);
        const JsonValue& d = data[idx];
        utassert(type == d.type);
        utassert(str::Eq(json::PathFormatTemp(path), d.path));
        utassert(str::Eq(value, d.value));

        idx++;
        return true;
    }
};

void JsonTest() {
    static const struct {
        Str json;
        JsonValue value;
    } validJsonData[] = {
        // strings
        {"\"test\"", JsonValue("", "test")},
        {"\"\\\\\\n\\t\\u01234\"", JsonValue("",
                                             "\\\n\t\xC4\xA3"
                                             "4")},
        // \u0000 is valid JSON
        {"\"\\u0000\"", JsonValue("", Str("\0", 1))},
        // numbers
        {"123", JsonValue("", "123", json::Type::Number)},
        {"-99.99", JsonValue("", "-99.99", json::Type::Number)},
        {"1.2E+15", JsonValue("", "1.2E+15", json::Type::Number)},
        {"0e-7", JsonValue("", "0e-7", json::Type::Number)},
        // keywords
        {"true", JsonValue("", "true", json::Type::Bool)},
        {"false", JsonValue("", "false", json::Type::Bool)},
        {"null", JsonValue("", "null", json::Type::Null)},
        // dictionaries
        {"{\"key\":\"test\"}", JsonValue("/key", "test")},
        {"{ \"no\" : 123 }", JsonValue("/no", "123", json::Type::Number)},
        {"{ \"bool\": true }", JsonValue("/bool", "true", json::Type::Bool)},
        {"{}", JsonValue()},
        // arrays
        {"[\"test\"]", JsonValue("[0]", "test")},
        {"[123]", JsonValue("[0]", "123", json::Type::Number)},
        {"[ null ]", JsonValue("[0]", "null", json::Type::Null)},
        {"[]", JsonValue()},
        // combination
        {"{\"key\":[{\"name\":-987}]}", JsonValue("/key[0]/name", "-987", json::Type::Number)},
    };

    for (size_t i = 0; i < dimof(validJsonData); i++) {
        JsonVerifier verifier(&validJsonData[i].value, validJsonData[i].value.value ? 1 : 0);
        utassert(json::Parse(validJsonData[i].json, &verifier));
    }

    static const struct {
        Str json;
        JsonValue value;
    } invalidJsonData[] = {
        // dictionaries
        {"{\"key\":\"test\"", JsonValue("/key", "test")},
        {"{ \"no\" : 123, }", JsonValue("/no", "123", json::Type::Number)},
        {"{\"key\":\"test\"]", JsonValue("/key", "test")},
        // arrays
        {"[\"test\"", JsonValue("[0]", "test")},
        {"[123,]", JsonValue("[0]", "123", json::Type::Number)},
        {"[\"test\"}", JsonValue("[0]", "test")},
    };

    for (size_t i = 0; i < dimof(invalidJsonData); i++) {
        JsonVerifier verifier(&invalidJsonData[i].value, 1);
        utassert(!json::Parse(invalidJsonData[i].json, &verifier));
    }

    static Str invalidJson[] = {
        "",    "string",      "nada",          "\"open",         "\"\\xC4\"", "\"\\u123h\"", "'string'", "01",  ".1",
        "12.", "1e",          "1.e5",          "1.E+2",          "1e+",       "1e-",         "-",        "-01", "{",
        "{,}", "{\"key\": }", "{\"key: 123 }", "{ 'key': 123 }", "[",         "[,]"};

    JsonVerifier verifyError(nullptr, 0);
    {
        Str s = invalidJson[10]; // this one caused buffer overflow
        utassert(!json::Parse(s, &verifyError));
    }

    for (size_t i = 0; i < dimof(invalidJson); i++) {
        utassert(!json::Parse(invalidJson[i], &verifyError));
    }

    const JsonValue testData[] = {
        JsonValue("/ComicBookInfo/1.0/title", "Meta data demo"),
        JsonValue("/ComicBookInfo/1.0/publicationMonth", "4", json::Type::Number),
        JsonValue("/ComicBookInfo/1.0/publicationYear", "2010", json::Type::Number),
        JsonValue("/ComicBookInfo/1.0/credits[0]/primary", "true", json::Type::Bool),
        JsonValue("/ComicBookInfo/1.0/credits[0]/role", "Writer"),
        JsonValue("/ComicBookInfo/1.0/credits[1]/primary", "false", json::Type::Bool),
        JsonValue("/ComicBookInfo/1.0/credits[1]/role", "Publisher"),
        JsonValue("/ComicBookInfo/1.0/credits[2]", "null", json::Type::Null),
        JsonValue("/appID", "Test/123"),
    };
    Str jsonSample =
        "{\n\
    \"ComicBookInfo/1.0\": {\n\
        \"title\": \"Meta data demo\",\n\
        \"publicationMonth\": 4,\n\
        \"publicationYear\": 2010,\n\
        \"credits\": [\n\
            { \"primary\": true, \"role\": \"Writer\" },\n\
            { \"primary\": false, \"role\": \"Publisher\" },\n\
            null\n\
        ]\n\
    },\n\
    \"appID\": \"Test/123\"\n\
}";
    JsonVerifier sampleVerifier(testData, dimof(testData));
    utassert(json::Parse(jsonSample, &sampleVerifier));

    // U+2028 / U+2029 must be escaped so the result is safe in a JS string literal
    {
        const char lineSep[] = {'\xE2', '\x80', '\xA8'};
        const char paraSep[] = {'\xE2', '\x80', '\xA9'};
        utassert(str::Eq(json::EscapeStrTemp(Str(lineSep, 3)), "\\u2028"));
        utassert(str::Eq(json::EscapeStrTemp(Str(paraSep, 3)), "\\u2029"));
        utassert(str::Eq(json::EscapeStrTemp(StrL("a\"b\\c\n")), "a\\\"b\\\\c\\n"));
        utassert(str::Eq(json::EscapeStrTemp(StrL("\b\f\r\t")), "\\b\\f\\r\\t"));
    }

    // PathMatch / PathBuildTemp / PathSeg helpers
    {
        StrNode* p = json::PathBuildTemp(StrL("/key"), StrL("i0"), StrL("/name"));
        utassert(str::Eq(json::PathFormatTemp(p), "/key[0]/name"));
        utassert(json::PathMatch(p, StrL("/key"), StrL("i0"), StrL("/name")));
        utassert(json::PathMatch(p, StrL("/key"), StrL("*"), StrL("/name")));
        utassert(json::PathMatch(p, StrL("*"), StrL("*"), StrL("*")));
        utassert(!json::PathMatch(p, StrL("/key"), StrL("i1"), StrL("/name")));
        utassert(!json::PathMatch(p, StrL("/key"), StrL("i0")));                            // too short
        utassert(!json::PathMatch(p, StrL("/key"), StrL("i0"), StrL("/name"), StrL("/x"))); // too long
        utassert(json::PathSegIndex(json::PathNth(p, 1)) == 0);
        utassert(json::PathSegIndex(p) == -1); // key segment
        utassert(str::Eq(json::PathSegKey(p), "key"));
        utassert(str::Eq(json::PathSegKey(json::PathNth(p, 2)), "name"));
        utassert(!json::PathSegKey(json::PathNth(p, 1)).s); // index segment
        utassert(!json::PathNth(p, 3));
        utassert(json::PathMatch(nullptr));
        utassert(!json::PathMatch(nullptr, StrL("/id")));

        // key containing '/' is one segment, not nested keys
        StrNode* slashKey = json::PathBuildTemp(StrL("/ComicBookInfo/1.0"), StrL("/title"));
        utassert(str::Eq(json::PathFormatTemp(slashKey), "/ComicBookInfo/1.0/title"));
        utassert(json::PathMatch(slashKey, StrL("/ComicBookInfo/1.0"), StrL("/title")));
        utassert(!json::PathMatch(slashKey, StrL("/ComicBookInfo"), StrL("/1.0"), StrL("/title")));
        utassert(str::Eq(json::PathSegKey(slashKey), "ComicBookInfo/1.0"));
    }

    // object key with '/' is a single path segment when parsed
    {
        struct SlashKeyVisitor : json::ValueVisitor {
            int n = 0;
            bool Visit(StrNode* path, Str value, json::Type type) override {
                n++;
                utassert(type == json::Type::String);
                utassert(str::Eq(value, "x"));
                utassert(str::Eq(json::PathFormatTemp(path), "/a/b/c"));
                utassert(json::PathMatch(path, StrL("/a/b"), StrL("/c")));
                utassert(!json::PathMatch(path, StrL("/a"), StrL("/b"), StrL("/c")));
                utassert(str::Eq(json::PathSegKey(path), "a/b"));
                utassert(str::Eq(json::PathSegKey(json::PathNth(path, 1)), "c"));
                return true;
            }
        } v;
        utassert(json::Parse(StrL("{\"a/b\":{\"c\":\"x\"}}"), &v));
        utassert(v.n == 1);
    }

    // cancel mid-parse: Visit false => Parse returns true, remaining values skipped
    {
        struct CancelVisitor : json::ValueVisitor {
            int n = 0;
            bool Visit(StrNode* path, Str value, json::Type type) override {
                n++;
                utassert(json::PathMatch(path, StrL("/a")));
                utassert(type == json::Type::Number);
                utassert(str::Eq(value, "1"));
                return false;
            }
        } v;
        utassert(json::Parse(StrL("{\"a\":1,\"b\":2,\"c\":3}"), &v));
        utassert(v.n == 1);
    }

    // cancel on first array element still succeeds overall
    {
        struct CancelArr : json::ValueVisitor {
            int n = 0;
            bool Visit(StrNode* path, Str value, json::Type /*type*/) override {
                n++;
                utassert(json::PathSegIndex(path) == 0);
                utassert(!path->next);
                utassert(str::Eq(value, "first"));
                return false;
            }
        } v;
        utassert(json::Parse(StrL("[\"first\",\"second\"]"), &v));
        utassert(v.n == 1);
    }

    // nesting depth limit is 128 (depth >= 128 fails)
    {
        struct CountVisitor : json::ValueVisitor {
            int n = 0;
            bool Visit(StrNode* /*path*/, Str /*value*/, json::Type /*type*/) override {
                n++;
                return true;
            }
        };

        auto nestArray = [](int depth) -> TempStr {
            str::Builder b;
            for (int i = 0; i < depth; i++) {
                b.AppendChar('[');
            }
            b.AppendChar('1');
            for (int i = 0; i < depth; i++) {
                b.AppendChar(']');
            }
            return ToStrTemp(b);
        };

        CountVisitor ok;
        utassert(json::Parse(nestArray(127), &ok));
        utassert(ok.n == 1);

        CountVisitor deep;
        utassert(!json::Parse(nestArray(128), &deep));
        utassert(deep.n == 0);

        auto nestObject = [](int depth) -> TempStr {
            str::Builder b;
            for (int i = 0; i < depth; i++) {
                b.Append("{\"k\":");
            }
            b.AppendChar('1');
            for (int i = 0; i < depth; i++) {
                b.AppendChar('}');
            }
            return ToStrTemp(b);
        };

        CountVisitor okObj;
        utassert(json::Parse(nestObject(127), &okObj));
        utassert(okObj.n == 1);

        CountVisitor deepObj;
        utassert(!json::Parse(nestObject(128), &deepObj));
        utassert(deepObj.n == 0);
    }

    // UTF-8 BOM is skipped
    {
        struct BomVisitor : json::ValueVisitor {
            int n = 0;
            bool Visit(StrNode* path, Str value, json::Type type) override {
                n++;
                utassert(!path);
                utassert(type == json::Type::Number);
                utassert(str::Eq(value, "7"));
                return true;
            }
        } v;
        char bomJson[] = {'\xEF', '\xBB', '\xBF', '7'};
        utassert(json::Parse(Str(bomJson, 4), &v));
        utassert(v.n == 1);
    }

    // multi-element array path indices
    {
        struct ArrVisitor : json::ValueVisitor {
            int n = 0;
            bool Visit(StrNode* path, Str value, json::Type type) override {
                utassert(type == json::Type::Number);
                utassert(path && !path->next);
                utassert(json::PathSegIndex(path) == n);
                utassert(str::Eq(json::PathFormatTemp(path), fmt("[%d]", n)));
                utassert(str::Eq(value, fmt("%d", (n + 1) * 10)));
                n++;
                return true;
            }
        } v;
        utassert(json::Parse(StrL("[10,20,30]"), &v));
        utassert(v.n == 3);
    }
}
