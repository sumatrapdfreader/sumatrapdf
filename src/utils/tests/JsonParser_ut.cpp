/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/JsonParser.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

struct JsonValue {
    const char* path{nullptr};
    const char* value{nullptr};
    json::Type type{json::Type::String};

    JsonValue() = default;
    JsonValue(const char* path, const char* value, json::Type type = json::Type::String)
        : path(path), type(type), value(value) {
    }
};

class JsonVerifier : public json::ValueVisitor {
    const JsonValue* data;
    size_t dataLen;
    size_t idx;

  public:
    JsonVerifier(const JsonValue* data, size_t dataLen) : data(data), dataLen(dataLen), idx(0) {
    }
    ~JsonVerifier() {
        utassert(dataLen == idx);
    }

    virtual bool Visit(const char* path, const char* value, json::Type type) {
        utassert(idx < dataLen);
        const JsonValue& d = data[idx];
        utassert(type == d.type);
        utassert(str::Eq(path, d.path));
        utassert(str::Eq(value, d.value));

        idx++;
        return true;
    }
};

void JsonTest() {
    static const struct {
        const char* json;
        JsonValue value;
    } validJsonData[] = {
        // strings
        {"\"test\"", JsonValue("", "test")},
        {"\"\\\\\\n\\t\\u01234\"", JsonValue("",
                                             "\\\n\t\xC4\xA3"
                                             "4")},
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
        const char* json;
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

    static const char* invalidJson[] = {
        "",  "string", "nada", "\"open", "\"\\xC4\"",   "\"\\u123h\"",   "'string'",       "01", ".1", "12.", "1e",
        "-", "-01",    "{",    "{,}",    "{\"key\": }", "{\"key: 123 }", "{ 'key': 123 }", "[",  "[,]"};

    JsonVerifier verifyError(nullptr, 0);
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
    const char* jsonSample =
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
}
