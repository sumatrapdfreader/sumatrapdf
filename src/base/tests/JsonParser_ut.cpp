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
        {StrL("\"test\""), ExpectedValue(StrL(""), StrL("test"))},
        {StrL("\"\\\\\\n\\t\\u01234\""), ExpectedValue(StrL(""), StrL("\\\n\t\xC4\xA3"
                                                                      "4"))},
        // \u0000 is valid JSON
        {StrL("\"\\u0000\""), ExpectedValue(StrL(""), Str("\0", 1))},
        // numbers
        {StrL("123"), ExpectedValue(StrL(""), StrL("123"), json::Type::Number)},
        {StrL("-99.99"), ExpectedValue(StrL(""), StrL("-99.99"), json::Type::Number)},
        {StrL("1.2E+15"), ExpectedValue(StrL(""), StrL("1.2E+15"), json::Type::Number)},
        {StrL("0e-7"), ExpectedValue(StrL(""), StrL("0e-7"), json::Type::Number)},
        // keywords
        {StrL("true"), ExpectedValue(StrL(""), StrL("true"), json::Type::Bool)},
        {StrL("false"), ExpectedValue(StrL(""), StrL("false"), json::Type::Bool)},
        {StrL("null"), ExpectedValue(StrL(""), StrL("null"), json::Type::Null)},
        // dictionaries
        {StrL("{\"key\":\"test\"}"), ExpectedValue(StrL("/key"), StrL("test"))},
        {StrL("{ \"no\" : 123 }"), ExpectedValue(StrL("/no"), StrL("123"), json::Type::Number)},
        {StrL("{ \"bool\": true }"), ExpectedValue(StrL("/bool"), StrL("true"), json::Type::Bool)},
        {StrL("{}"), ExpectedValue()},
        // arrays
        {StrL("[\"test\"]"), ExpectedValue(StrL("[0]"), StrL("test"))},
        {StrL("[123]"), ExpectedValue(StrL("[0]"), StrL("123"), json::Type::Number)},
        {StrL("[ null ]"), ExpectedValue(StrL("[0]"), StrL("null"), json::Type::Null)},
        {StrL("[]"), ExpectedValue()},
        // combination
        {StrL("{\"key\":[{\"name\":-987}]}"), ExpectedValue(StrL("/key[0]/name"), StrL("-987"), json::Type::Number)},
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
        {StrL("{\"key\":\"test\""), ExpectedValue(StrL("/key"), StrL("test"))},
        {StrL("{ \"no\" : 123, }"), ExpectedValue(StrL("/no"), StrL("123"), json::Type::Number)},
        {StrL("{\"key\":\"test\"]"), ExpectedValue(StrL("/key"), StrL("test"))},
        // arrays
        {StrL("[\"test\""), ExpectedValue(StrL("[0]"), StrL("test"))},
        {StrL("[123,]"), ExpectedValue(StrL("[0]"), StrL("123"), json::Type::Number)},
        {StrL("[\"test\"}"), ExpectedValue(StrL("[0]"), StrL("test"))},
    };

    for (size_t i = 0; i < dimof(invalidJsonData); i++) {
        JsonVerifier verifier(&invalidJsonData[i].value, 1);
        utassert(!ParseVerify(invalidJsonData[i].json, &verifier));
    }

    static Str invalidJson[] = {StrL(""),
                                StrL("string"),
                                StrL("nada"),
                                StrL("\"open"),
                                StrL("\"\\xC4\""),
                                StrL("\"\\u123h\""),
                                StrL("'string'"),
                                StrL("01"),
                                StrL(".1"),
                                StrL("12."),
                                StrL("1e"),
                                StrL("1.e5"),
                                StrL("1.E+2"),
                                StrL("1e+"),
                                StrL("1e-"),
                                StrL("-"),
                                StrL("-01"),
                                StrL("{"),
                                StrL("{,}"),
                                StrL("{\"key\": }"),
                                StrL("{\"key: 123 }"),
                                StrL("{ 'key': 123 }"),
                                StrL("["),
                                StrL("[,]")};

    JsonVerifier verifyError(nullptr, 0);
    {
        Str s = invalidJson[10]; // this one caused buffer overflow
        utassert(!ParseVerify(s, &verifyError));
    }

    for (size_t i = 0; i < dimof(invalidJson); i++) {
        utassert(!ParseVerify(invalidJson[i], &verifyError));
    }

    const ExpectedValue testData[] = {
        ExpectedValue(StrL("/ComicBookInfo/1.0/title"), StrL("Meta data demo")),
        ExpectedValue(StrL("/ComicBookInfo/1.0/publicationMonth"), StrL("4"), json::Type::Number),
        ExpectedValue(StrL("/ComicBookInfo/1.0/publicationYear"), StrL("2010"), json::Type::Number),
        ExpectedValue(StrL("/ComicBookInfo/1.0/credits[0]/primary"), StrL("true"), json::Type::Bool),
        ExpectedValue(StrL("/ComicBookInfo/1.0/credits[0]/role"), StrL("Writer")),
        ExpectedValue(StrL("/ComicBookInfo/1.0/credits[1]/primary"), StrL("false"), json::Type::Bool),
        ExpectedValue(StrL("/ComicBookInfo/1.0/credits[1]/role"), StrL("Publisher")),
        ExpectedValue(StrL("/ComicBookInfo/1.0/credits[2]"), StrL("null"), json::Type::Null),
        ExpectedValue(StrL("/appID"), StrL("Test/123")),
    };
    Str jsonSample = StrL(
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
}");
    JsonVerifier sampleVerifier(testData, dimof(testData));
    utassert(ParseVerify(jsonSample, &sampleVerifier));

    // U+2028 / U+2029 must be escaped so the result is safe in a JS string literal
    {
        const char lineSep[] = {'\xE2', '\x80', '\xA8'};
        const char paraSep[] = {'\xE2', '\x80', '\xA9'};
        utassert(str::Eq(json::EscapeStrTemp(Str(lineSep, 3)), StrL("\\u2028")));
        utassert(str::Eq(json::EscapeStrTemp(Str(paraSep, 3)), StrL("\\u2029")));
        utassert(str::Eq(json::EscapeStrTemp(StrL("a\"b\\c\n")), StrL("a\\\"b\\\\c\\n")));
        utassert(str::Eq(json::EscapeStrTemp(StrL("\b\f\r\t")), StrL("\\b\\f\\r\\t")));
    }

    // PathMatch / PathBuildTemp / PathSeg helpers
    {
        StrNode* p = json::PathBuildTemp(StrL("/key"), StrL("i0"), StrL("/name"));
        utassert(str::Eq(json::PathFormatTemp(p), StrL("/key[0]/name")));
        utassert(json::PathMatch(p, StrL("/key"), StrL("i0"), StrL("/name")));
        utassert(json::PathMatch(p, StrL("/key"), StrL("*"), StrL("/name")));
        utassert(json::PathMatch(p, StrL("*"), StrL("*"), StrL("*")));
        utassert(!json::PathMatch(p, StrL("/key"), StrL("i1"), StrL("/name")));
        utassert(!json::PathMatch(p, StrL("/key"), StrL("i0")));                            // too short
        utassert(!json::PathMatch(p, StrL("/key"), StrL("i0"), StrL("/name"), StrL("/x"))); // too long
        utassert(json::PathSegIndex(json::PathNth(p, 1)) == 0);
        utassert(json::PathSegIndex(p) == -1); // key segment
        utassert(str::Eq(json::PathSegKey(p), StrL("key")));
        utassert(str::Eq(json::PathSegKey(json::PathNth(p, 2)), StrL("name")));
        utassert(!json::PathSegKey(json::PathNth(p, 1)).s); // index segment
        utassert(!json::PathNth(p, 3));
        utassert(json::PathMatch(nullptr));
        utassert(!json::PathMatch(nullptr, StrL("/id")));

        // key containing '/' is one segment, not nested keys
        StrNode* slashKey = json::PathBuildTemp(StrL("/ComicBookInfo/1.0"), StrL("/title"));
        utassert(str::Eq(json::PathFormatTemp(slashKey), StrL("/ComicBookInfo/1.0/title")));
        utassert(json::PathMatch(slashKey, StrL("/ComicBookInfo/1.0"), StrL("/title")));
        utassert(!json::PathMatch(slashKey, StrL("/ComicBookInfo"), StrL("/1.0"), StrL("/title")));
        utassert(str::Eq(json::PathSegKey(slashKey), StrL("ComicBookInfo/1.0")));
    }

    // object key with '/' is a single path segment when parsed
    {
        struct SlashKeyState {
            int n = 0;
        } st;
        auto onValue = [](SlashKeyState* st, json::Value* v) {
            st->n++;
            utassert(v->type == json::Type::String);
            utassert(str::Eq(v->value, StrL("x")));
            utassert(str::Eq(json::PathFormatTemp(v->path), StrL("/a/b/c")));
            utassert(json::PathMatch(v->path, StrL("/a/b"), StrL("/c")));
            utassert(!json::PathMatch(v->path, StrL("/a"), StrL("/b"), StrL("/c")));
            utassert(str::Eq(json::PathSegKey(v->path), StrL("a/b")));
            utassert(str::Eq(json::PathSegKey(json::PathNth(v->path, 1)), StrL("c")));
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
            utassert(str::Eq(v->value, StrL("1")));
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
            utassert(str::Eq(v->value, StrL("first")));
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
                b.Append(StrL("{\"k\":"));
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
            utassert(str::Eq(v->value, StrL("7")));
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
