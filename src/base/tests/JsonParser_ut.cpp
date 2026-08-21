/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/JsonParser.h"

// must be last due to assert() over-write
#include "base/UtAssert.h"

struct ExpectedValue {
    Str path;
    Str value;
    json::Type type{json::Type::String};

    ExpectedValue() = default;
    ExpectedValue(Str path, Str value, json::Type type = json::Type::String) : path(path), type(type), value(value) {}
};

struct JsonVerifier {
    const ExpectedValue* data = nullptr;
    size_t dataLen = 0;
    size_t idx = 0;

    JsonVerifier(const ExpectedValue* data, size_t dataLen) : data(data), dataLen(dataLen), idx(0) {}
    ~JsonVerifier() { utassert(dataLen == idx); }

    void OnValue(json::Value* v) {
        utassert(idx < dataLen);
        const ExpectedValue& d = data[idx];
        utassert(v->type == d.type);
        utassert(str::Eq(json::PathFormatTemp(v->path), d.path));
        utassert(str::Eq(v->value, d.value));
        idx++;
    }
};

static bool ParseVerify(Str json, JsonVerifier* v) {
    return json::Parse(json, MkMethod1<JsonVerifier, json::Value*, &JsonVerifier::OnValue>(v));
}

struct CountState {
    int n = 0;
};

static void CountOnValue(CountState* st, json::Value* /*v*/) {
    st->n++;
}

void JsonTest() {
    static const struct {
        Str json;
        ExpectedValue value;
    } validJsonData[] = {
        // strings
        {"\"test\"", ExpectedValue("", "test")},
        {"\"\\\\\\n\\t\\u01234\"", ExpectedValue("",
                                                 "\\\n\t\xC4\xA3"
                                                 "4")},
        // \u0000 is valid JSON
        {"\"\\u0000\"", ExpectedValue("", Str("\0", 1))},
        // numbers
        {"123", ExpectedValue("", "123", json::Type::Number)},
        {"-99.99", ExpectedValue("", "-99.99", json::Type::Number)},
        {"1.2E+15", ExpectedValue("", "1.2E+15", json::Type::Number)},
        {"0e-7", ExpectedValue("", "0e-7", json::Type::Number)},
        // keywords
        {"true", ExpectedValue("", "true", json::Type::Bool)},
        {"false", ExpectedValue("", "false", json::Type::Bool)},
        {"null", ExpectedValue("", "null", json::Type::Null)},
        // dictionaries
        {"{\"key\":\"test\"}", ExpectedValue("/key", "test")},
        {"{ \"no\" : 123 }", ExpectedValue("/no", "123", json::Type::Number)},
        {"{ \"bool\": true }", ExpectedValue("/bool", "true", json::Type::Bool)},
        {"{}", ExpectedValue()},
        // arrays
        {"[\"test\"]", ExpectedValue("[0]", "test")},
        {"[123]", ExpectedValue("[0]", "123", json::Type::Number)},
        {"[ null ]", ExpectedValue("[0]", "null", json::Type::Null)},
        {"[]", ExpectedValue()},
        // combination
        {"{\"key\":[{\"name\":-987}]}", ExpectedValue("/key[0]/name", "-987", json::Type::Number)},
    };

    for (size_t i = 0; i < dimof(validJsonData); i++) {
        JsonVerifier verifier(&validJsonData[i].value, validJsonData[i].value.value ? 1 : 0);
        utassert(ParseVerify(validJsonData[i].json, &verifier));
    }

    static const struct {
        Str json;
        ExpectedValue value;
    } invalidJsonData[] = {
        // dictionaries
        {"{\"key\":\"test\"", ExpectedValue("/key", "test")},
        {"{ \"no\" : 123, }", ExpectedValue("/no", "123", json::Type::Number)},
        {"{\"key\":\"test\"]", ExpectedValue("/key", "test")},
        // arrays
        {"[\"test\"", ExpectedValue("[0]", "test")},
        {"[123,]", ExpectedValue("[0]", "123", json::Type::Number)},
        {"[\"test\"}", ExpectedValue("[0]", "test")},
    };

    for (size_t i = 0; i < dimof(invalidJsonData); i++) {
        JsonVerifier verifier(&invalidJsonData[i].value, 1);
        utassert(!ParseVerify(invalidJsonData[i].json, &verifier));
    }

    static Str invalidJson[] = {
        "",    "string",      "nada",          "\"open",         "\"\\xC4\"", "\"\\u123h\"", "'string'", "01",  ".1",
        "12.", "1e",          "1.e5",          "1.E+2",          "1e+",       "1e-",         "-",        "-01", "{",
        "{,}", "{\"key\": }", "{\"key: 123 }", "{ 'key': 123 }", "[",         "[,]"};

    JsonVerifier verifyError(nullptr, 0);
    {
        Str s = invalidJson[10]; // this one caused buffer overflow
        utassert(!ParseVerify(s, &verifyError));
    }

    for (size_t i = 0; i < dimof(invalidJson); i++) {
        utassert(!ParseVerify(invalidJson[i], &verifyError));
    }

    const ExpectedValue testData[] = {
        ExpectedValue("/ComicBookInfo/1.0/title", "Meta data demo"),
        ExpectedValue("/ComicBookInfo/1.0/publicationMonth", "4", json::Type::Number),
        ExpectedValue("/ComicBookInfo/1.0/publicationYear", "2010", json::Type::Number),
        ExpectedValue("/ComicBookInfo/1.0/credits[0]/primary", "true", json::Type::Bool),
        ExpectedValue("/ComicBookInfo/1.0/credits[0]/role", "Writer"),
        ExpectedValue("/ComicBookInfo/1.0/credits[1]/primary", "false", json::Type::Bool),
        ExpectedValue("/ComicBookInfo/1.0/credits[1]/role", "Publisher"),
        ExpectedValue("/ComicBookInfo/1.0/credits[2]", "null", json::Type::Null),
        ExpectedValue("/appID", "Test/123"),
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
    utassert(ParseVerify(jsonSample, &sampleVerifier));

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
        struct SlashKeyState {
            int n = 0;
        } st;
        auto onValue = [](SlashKeyState* st, json::Value* v) {
            st->n++;
            utassert(v->type == json::Type::String);
            utassert(str::Eq(v->value, "x"));
            utassert(str::Eq(json::PathFormatTemp(v->path), "/a/b/c"));
            utassert(json::PathMatch(v->path, StrL("/a/b"), StrL("/c")));
            utassert(!json::PathMatch(v->path, StrL("/a"), StrL("/b"), StrL("/c")));
            utassert(str::Eq(json::PathSegKey(v->path), "a/b"));
            utassert(str::Eq(json::PathSegKey(json::PathNth(v->path, 1)), "c"));
        };
        utassert(json::Parse(StrL("{\"a/b\":{\"c\":\"x\"}}"), MkFunc1<SlashKeyState, json::Value*>(onValue, &st)));
        utassert(st.n == 1);
    }

    // cancel mid-parse: Value::stop => Parse returns true, remaining values skipped
    {
        struct CancelState {
            int n = 0;
        } st;
        auto onValue = [](CancelState* st, json::Value* v) {
            st->n++;
            utassert(json::PathMatch(v->path, StrL("/a")));
            utassert(v->type == json::Type::Number);
            utassert(str::Eq(v->value, "1"));
            v->stop = true;
        };
        utassert(json::Parse(StrL("{\"a\":1,\"b\":2,\"c\":3}"), MkFunc1<CancelState, json::Value*>(onValue, &st)));
        utassert(st.n == 1);
    }

    // cancel on first array element still succeeds overall
    {
        struct CancelState {
            int n = 0;
        } st;
        auto onValue = [](CancelState* st, json::Value* v) {
            st->n++;
            utassert(json::PathSegIndex(v->path) == 0);
            utassert(!v->path->next);
            utassert(str::Eq(v->value, "first"));
            v->stop = true;
        };
        utassert(json::Parse(StrL("[\"first\",\"second\"]"), MkFunc1<CancelState, json::Value*>(onValue, &st)));
        utassert(st.n == 1);
    }

    // nesting depth limit is 128 (depth >= 128 fails)
    {
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

        CountState ok;
        utassert(json::Parse(nestArray(127), MkFunc1(CountOnValue, &ok)));
        utassert(ok.n == 1);

        CountState deep;
        utassert(!json::Parse(nestArray(128), MkFunc1(CountOnValue, &deep)));
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

        CountState okObj;
        utassert(json::Parse(nestObject(127), MkFunc1(CountOnValue, &okObj)));
        utassert(okObj.n == 1);

        CountState deepObj;
        utassert(!json::Parse(nestObject(128), MkFunc1(CountOnValue, &deepObj)));
        utassert(deepObj.n == 0);
    }

    // UTF-8 BOM is skipped
    {
        struct BomState {
            int n = 0;
        } st;
        auto onValue = [](BomState* st, json::Value* v) {
            st->n++;
            utassert(!v->path);
            utassert(v->type == json::Type::Number);
            utassert(str::Eq(v->value, "7"));
        };
        char bomJson[] = {'\xEF', '\xBB', '\xBF', '7'};
        utassert(json::Parse(Str(bomJson, 4), MkFunc1<BomState, json::Value*>(onValue, &st)));
        utassert(st.n == 1);
    }

    // multi-element array path indices
    {
        struct ArrState {
            int n = 0;
        } st;
        auto onValue = [](ArrState* st, json::Value* v) {
            utassert(v->type == json::Type::Number);
            utassert(v->path && !v->path->next);
            utassert(json::PathSegIndex(v->path) == st->n);
            utassert(str::Eq(json::PathFormatTemp(v->path), fmt("[%d]", st->n)));
            utassert(str::Eq(v->value, fmt("%d", (st->n + 1) * 10)));
            st->n++;
        };
        utassert(json::Parse(StrL("[10,20,30]"), MkFunc1<ArrState, json::Value*>(onValue, &st)));
        utassert(st.n == 3);
    }
}
