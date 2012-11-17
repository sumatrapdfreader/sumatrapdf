/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "JsonParser.h"

struct JsonValue {
    const char *path;
    json::DataType type;
    const char *value;

    JsonValue(const char *path, const char *value, json::DataType type=json::Type_String) :
        path(path), type(type), value(value) { }
};

class JsonVerifier : public json::ValueVisitor {
    JsonValue *data;
    size_t dataLen;
    size_t idx;

public:
    JsonVerifier(JsonValue *data, size_t dataLen=1) :
        data(data), dataLen(dataLen), idx(0) { }

    virtual bool Visit(const char *path, const char *value, json::DataType type) {
        assert(idx < dataLen);
        assert(type == data[idx].type);
        assert(str::Eq(path, data[idx].path));
        assert(str::Eq(value, data[idx].value));

        idx++;
        return true;
    }
};

static void JsonTest()
{
    JsonVerifier verifyError(NULL, 0);

    assert(!json::Parse("", &verifyError));

    assert(json::Parse("\"test\"", &JsonVerifier(&JsonValue("", "test"))));
    assert(json::Parse("\"\\\\\\n\\t\\u01234\"", &JsonVerifier(&JsonValue("", "\\\n\t\xC4\xA3""4"))));
    assert(!json::Parse("\"open", &verifyError));
    assert(!json::Parse("\"\\xC4\"", &verifyError));
    assert(!json::Parse("\"\\u123h\"", &verifyError));

    assert(json::Parse("123", &JsonVerifier(&JsonValue("", "123", json::Type_Number))));
    assert(json::Parse("-99.99", &JsonVerifier(&JsonValue("", "-99.99", json::Type_Number))));
    assert(json::Parse("1.2E+15", &JsonVerifier(&JsonValue("", "1.2E+15", json::Type_Number))));
    assert(json::Parse("0e-7", &JsonVerifier(&JsonValue("", "0e-7", json::Type_Number))));
    assert(!json::Parse("01", &verifyError));
    assert(!json::Parse(".1", &verifyError));
    assert(!json::Parse("12.", &verifyError));
    assert(!json::Parse("1e", &verifyError));

    assert(json::Parse("{\"key\":\"test\"}", &JsonVerifier(&JsonValue("/key", "test"))));
    assert(json::Parse("{ \"no\" : 123 }", &JsonVerifier(&JsonValue("/no", "123", json::Type_Number))));
    assert(json::Parse("{}", &verifyError));
    assert(!json::Parse("{,}", &verifyError));
    assert(!json::Parse("{\"key\":\"test\"", &JsonVerifier(&JsonValue("/key", "test"))));
    assert(!json::Parse("{ \"no\" : 123, }", &JsonVerifier(&JsonValue("/no", "123", json::Type_Number))));
    assert(!json::Parse("{\"key\":\"test\"]", &JsonVerifier(&JsonValue("/key", "test"))));
    assert(!json::Parse("{\"key\": }", &verifyError));
    assert(!json::Parse("{\"key: 123 }", &verifyError));

    assert(json::Parse("[\"test\"]", &JsonVerifier(&JsonValue("[0]", "test"))));
    assert(json::Parse("[123]", &JsonVerifier(&JsonValue("[0]", "123", json::Type_Number))));
    assert(json::Parse("[]", &verifyError));
    assert(!json::Parse("[,]", &verifyError));
    assert(!json::Parse("[\"test\"", &JsonVerifier(&JsonValue("[0]", "test"))));
    assert(!json::Parse("[123,]", &JsonVerifier(&JsonValue("[0]", "123", json::Type_Number))));
    assert(!json::Parse("[\"test\"}", &JsonVerifier(&JsonValue("[0]", "test"))));

    assert(json::Parse("true", &JsonVerifier(&JsonValue("", "true", json::Type_Bool))));
    assert(json::Parse("false", &JsonVerifier(&JsonValue("", "false", json::Type_Bool))));
    assert(json::Parse("null", &JsonVerifier(&JsonValue("", "null", json::Type_Null))));
    assert(!json::Parse("string", &verifyError));
    assert(!json::Parse("nada", &verifyError));

    assert(json::Parse("{\"key\":[{\"name\":-987}]}",
        &JsonVerifier(&JsonValue("/key[0]/name", "-987", json::Type_Number))));

    JsonValue testData[] = {
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
    const char *jsonSample = "{\
    \"ComicBookInfo/1.0\": {\
        \"title\": \"Meta data demo\",\
        \"publicationMonth\": 4,\
        \"publicationYear\": 2010,\
        \"credits\": [\
            { \"primary\": true, \"role\": \"Writer\" },\
            { \"primary\": false, \"role\": \"Publisher\" },\
            null\
        ]\
    },\
    \"appID\": \"Test/123\"\
}";
    assert(json::Parse(jsonSample, &JsonVerifier(testData, dimof(testData))));
}
