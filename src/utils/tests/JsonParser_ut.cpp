/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/JsonParser.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

struct JsonValue {
    const char* path;
    json::DataType type;
    const char* value;

    JsonValue() : path(nullptr), value(nullptr) {
    }
    JsonValue(const char* path, const char* value, json::DataType type = json::Type_String)
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

    virtual bool Visit(const char* path, const char* value, json::DataType type) {
        utassert(idx < dataLen);
        utassert(type == data[idx].type);
        utassert(str::Eq(path, data[idx].path));
        utassert(str::Eq(value, data[idx].value));

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
        {"123", JsonValue("", "123", json::Type_Number)},
        {"-99.99", JsonValue("", "-99.99", json::Type_Number)},
        {"1.2E+15", JsonValue("", "1.2E+15", json::Type_Number)},
        {"0e-7", JsonValue("", "0e-7", json::Type_Number)},
        // keywords
        {"true", JsonValue("", "true", json::Type_Bool)},
        {"false", JsonValue("", "false", json::Type_Bool)},
        {"null", JsonValue("", "null", json::Type_Null)},
        // dictionaries
        {"{\"key\":\"test\"}", JsonValue("/key", "test")},
        {"{ \"no\" : 123 }", JsonValue("/no", "123", json::Type_Number)},
        {"{ \"bool\": true }", JsonValue("/bool", "true", json::Type_Bool)},
        {"{}", JsonValue()},
        // arrays
        {"[\"test\"]", JsonValue("[0]", "test")},
        {"[123]", JsonValue("[0]", "123", json::Type_Number)},
        {"[ null ]", JsonValue("[0]", "null", json::Type_Null)},
        {"[]", JsonValue()},
        // combination
        {"{\"key\":[{\"name\":-987}]}", JsonValue("/key[0]/name", "-987", json::Type_Number)},
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
        {"{ \"no\" : 123, }", JsonValue("/no", "123", json::Type_Number)},
        {"{\"key\":\"test\"]", JsonValue("/key", "test")},
        // arrays
        {"[\"test\"", JsonValue("[0]", "test")},
        {"[123,]", JsonValue("[0]", "123", json::Type_Number)},
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
        JsonValue("/ComicBookInfo/1.0/publicationMonth", "4", json::Type_Number),
        JsonValue("/ComicBookInfo/1.0/publicationYear", "2010", json::Type_Number),
        JsonValue("/ComicBookInfo/1.0/credits[0]/primary", "true", json::Type_Bool),
        JsonValue("/ComicBookInfo/1.0/credits[0]/role", "Writer"),
        JsonValue("/ComicBookInfo/1.0/credits[1]/primary", "false", json::Type_Bool),
        JsonValue("/ComicBookInfo/1.0/credits[1]/role", "Publisher"),
        JsonValue("/ComicBookInfo/1.0/credits[2]", "null", json::Type_Null),
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
