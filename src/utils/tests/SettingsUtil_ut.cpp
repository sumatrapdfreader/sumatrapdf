/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/SettingsUtil.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

static const FieldInfo gSutPointIFields[] = {
    {offsetof(PointI, x), Type_Int, 111},
    {offsetof(PointI, y), Type_Int, 222},
};
static const StructInfo gSutPointIInfo = {sizeof(PointI), 2, gSutPointIFields, "X\0Y"};

struct SutStructNested {
    PointI point;
    Vec<COLORREF>* colorArray;
};

static const FieldInfo gSutStructNestedFields[] = {
    {offsetof(SutStructNested, point), Type_Struct, (intptr_t)&gSutPointIInfo},
    {offsetof(SutStructNested, colorArray), Type_ColorArray, (intptr_t) "#000000 #ffffff"},
};
static const StructInfo gSutStructNestedInfo = {sizeof(SutStructNested), 2, gSutStructNestedFields,
                                                "Point\0ColorArray"};

struct SutStructItem {
    Vec<float>* floatArray;
    PointI compactPoint;
    SutStructNested nested;
};

static const FieldInfo gSutStructItemFields[] = {
    {offsetof(SutStructItem, compactPoint), Type_Compact, (intptr_t)&gSutPointIInfo},
    {offsetof(SutStructItem, floatArray), Type_FloatArray, 0},
    {offsetof(SutStructItem, nested), Type_Struct, (intptr_t)&gSutStructNestedInfo},
};
static const StructInfo gSutStructItemInfo = {sizeof(SutStructItem), 3, gSutStructItemFields,
                                              "CompactPoint\0FloatArray\0Nested"};

struct SutStruct {
    int internal;
    bool boolean;
    COLORREF color;
    float floatingPoint;
    int integer;
    WCHAR* string;
    WCHAR* nullString;
    WCHAR* escapedString;
    char* utf8String;
    char* nullUtf8String;
    char* escapedUtf8String;
    Vec<int>* intArray;
    Vec<WCHAR*>* strArray;
    Vec<WCHAR*>* emptyStrArray;
    PointI point;
    Vec<SutStructItem*>* sutStructItems;
    char* internalString;
};

static const FieldInfo gSutStructFields[] = {
    {(size_t)-1, Type_Comment, (intptr_t) "This file will be overwritten - modify at your own risk!\r\n"},
    {offsetof(SutStruct, boolean), Type_Bool, (intptr_t) true},
    {offsetof(SutStruct, color), Type_Color, 0xffcc9933},
    {offsetof(SutStruct, floatingPoint), Type_Float, (intptr_t) "-3.14"},
    {offsetof(SutStruct, integer), Type_Int, 27},
    {offsetof(SutStruct, string), Type_String, (intptr_t)L"String"},
    {offsetof(SutStruct, nullString), Type_String, 0},
    {offsetof(SutStruct, escapedString), Type_String, (intptr_t)L"$\nstring "},
    {offsetof(SutStruct, utf8String), Type_Utf8String, (intptr_t) "Utf-8 String"},
    {offsetof(SutStruct, nullUtf8String), Type_Utf8String, 0},
    {offsetof(SutStruct, escapedUtf8String), Type_Utf8String, (intptr_t) "$\nstring "},
    {offsetof(SutStruct, intArray), Type_IntArray, (intptr_t) "1 2 -3"},
    {offsetof(SutStruct, strArray), Type_StringArray, (intptr_t) "one \"two three\" \"\""},
    {offsetof(SutStruct, emptyStrArray), Type_StringArray, 0},
    {offsetof(SutStruct, point), Type_Struct, (intptr_t)&gSutPointIInfo},
    {(size_t)-1, Type_Comment, 0},
    {offsetof(SutStruct, sutStructItems), Type_Array, (intptr_t)&gSutStructItemInfo},
};
static const StructInfo gSutStructInfo = {sizeof(SutStruct), 17, gSutStructFields,
                                          "\0Boolean\0Color\0FloatingPoint\0Integer\0String\0NullString\0EscapedString"
                                          "\0Utf8String\0NullUtf8String\0EscapedUtf8String\0IntArray\0StrArray\0EmptySt"
                                          "rArray\0Point\0\0SutStructItems"};

void SettingsUtilTest() {
    static const char* serialized = UTF8_BOM
        "# This file will be overwritten - modify at your own risk!\r\n\r\n\
Boolean = true\r\n\
Color = #abcdef\r\n\
FloatingPoint = 2.7182\r\n\
Integer = -1234567890\r\n\
String = Might\\be\\a\\path\r\n\
EscapedString = $\t$r$n$$ $\r\n\
Utf8String = another string\r\n\
EscapedUtf8String = $r$n[]\t$\r\n\
IntArray = 3 1\r\n\
StrArray = \"with space\" plain \"quote:\"\"\"\r\n\
Point [\r\n\
\tX = -17\r\n\
\tY = -18\r\n\
\tZ = -19\r\n\
]\r\n\
\r\n\
SutStructItems [\r\n\
\t[\r\n\
\t\tCompactPoint = -1 5\r\n\
\t\tFloatArray = -1.5 1.5\r\n\
\t\tNested [\r\n\
\t\t\tPoint [\r\n\
\t\t\t\tX = 1\r\n\
\t\t\t\tY = 2\r\n\
\t\t\t]\r\n\
\t\t\tColorArray = \r\n\
\t\t]\r\n\
\t]\r\n\
\t[\r\n\
\t\tCompactPoint = 3 -4\r\n\
\t\tNested [\r\n\
\t\t\tPoint [\r\n\
\t\t\t\tX = 5\r\n\
\t\t\t\tY = 6\r\n\
\t\t\t]\r\n\
\t\t\tColorArray = #12345678 #987654\r\n\
\t\t]\r\n\
\t]\r\n\
]\r\n\
UnknownString = Forget-me-not\r\n\
UnknownNode [\r\n\
\tAnotherPoint = 7 8\r\n\
\tNested [\r\n\
\t\tKey = Value\r\n\
\t]\r\n\
]\r\n";

    static const char* unknownOnly = UTF8_BOM
        "\
UnknownString: Forget-me-not\r\n\
[Point]\r\n\
Z: -19\r\n\
[UnknownNode]\r\n\
AnotherPoint: 7 8\r\n\
Nested [\r\n\
Key = Value";

    SutStruct* data = nullptr;
    for (int i = 0; i < 3; i++) {
        data = (SutStruct*)DeserializeStruct(&gSutStructInfo, serialized, data);
        utassert(data->internal == i);
        const char* s = serialized;
        if (i < 2) {
            s = unknownOnly;
        }
        char* reserialized = SerializeStruct(&gSutStructInfo, data, s);
        utassert(str::Eq(serialized, reserialized));
        free(reserialized);
        data->internal++;
    }
    utassert(RGB(0xab, 0xcd, 0xef) == data->color);
    utassert(str::Eq(data->escapedString, L"\t\r\n$ "));
    utassert(str::Eq(data->escapedUtf8String, "\r\n[]\t"));
    utassert(2 == data->intArray->size() && 3 == data->intArray->at(0));
    utassert(3 == data->strArray->size() && 0 == data->emptyStrArray->size());
    utassert(str::Eq(data->strArray->at(0), L"with space") && str::Eq(data->strArray->at(1), L"plain") &&
             str::Eq(data->strArray->at(2), L"quote:\""));
    utassert(2 == data->sutStructItems->size());
    utassert(PointI(-1, 5) == data->sutStructItems->at(0)->compactPoint);
    utassert(2 == data->sutStructItems->at(0)->floatArray->size());
    utassert(0 == data->sutStructItems->at(0)->nested.colorArray->size());
    utassert(0 == data->sutStructItems->at(1)->floatArray->size());
    utassert(2 == data->sutStructItems->at(1)->nested.colorArray->size());
    utassert(0x12785634 == data->sutStructItems->at(1)->nested.colorArray->at(0));
    utassert(!data->internalString);
    utassert(!str::Eq(serialized, AutoFree(SerializeStruct(&gSutStructInfo, data))));
    data->sutStructItems->at(0)->nested.point.x++;
    utassert(!str::Eq(serialized, AutoFree(SerializeStruct(&gSutStructInfo, data, unknownOnly))));
    FreeStruct(&gSutStructInfo, data);

    data = (SutStruct*)DeserializeStruct(&gSutStructInfo, nullptr);
    utassert(data && data->boolean && 0xffcc9933 == data->color);
    utassert(-3.14f == data->floatingPoint && 27 == data->integer);
    utassert(str::Eq(data->string, L"String") && !data->nullString && str::Eq(data->escapedString, L"$\nstring "));
    utassert(str::Eq(data->utf8String, "Utf-8 String") && !data->nullUtf8String &&
             str::Eq(data->escapedUtf8String, "$\nstring "));
    utassert(data->intArray && 3 == data->intArray->size() && 1 == data->intArray->at(0));
    utassert(2 == data->intArray->at(1) && -3 == data->intArray->at(2));
    utassert(3 == data->strArray->size() && 0 == data->emptyStrArray->size());
    utassert(str::Eq(data->strArray->at(0), L"one") && str::Eq(data->strArray->at(1), L"two three") &&
             str::Eq(data->strArray->at(2), L""));
    utassert(PointI(111, 222) == data->point);
    utassert(data->sutStructItems && 0 == data->sutStructItems->size());
    FreeStruct(&gSutStructInfo, data);

    static const char* boolData[] = {
        "Boolean = true", "Boolean = false", "Boolean = TRUE", "Boolean = FALSE", "Boolean = yes",
        "Boolean = no",   "Boolean = Yes",   "Boolean = No",   "Boolean = 1",     "Boolean = 0",
    };
    for (int i = 0; i < dimof(boolData); i++) {
        data = (SutStruct*)DeserializeStruct(&gSutStructInfo, boolData[i]);
        utassert(data->boolean == ((i % 2) == 0));
        FreeStruct(&gSutStructInfo, data);
    }
}
