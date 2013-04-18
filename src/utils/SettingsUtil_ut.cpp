/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "SettingsUtil.h"

static const FieldInfo gSutPointIFields[] = {
    { offsetof(PointI, x), Type_Int, 111 },
    { offsetof(PointI, y), Type_Int, 222 },
};
static const StructInfo gSutPointIInfo = { sizeof(PointI), 2, gSutPointIFields, "X\0Y" };

struct SutStructNested {
    PointI point;
    Vec<COLORREF> *colorArray;
};

static const FieldInfo gSutStructNestedFields[] = {
    { offsetof(SutStructNested, point), Type_Struct, (intptr_t)&gSutPointIInfo },
    { offsetof(SutStructNested, colorArray), Type_ColorArray, (intptr_t)"#000000 #ffffff" },
};
static const StructInfo gSutStructNestedInfo = { sizeof(SutStructNested), 2, gSutStructNestedFields, "Point\0ColorArray" };

struct SutStructItem {
    Vec<float> *floatArray;
    PointI compactPoint;
    SutStructNested nested;
};

static const FieldInfo gSutStructItemFields[] = {
    { offsetof(SutStructItem, compactPoint), Type_Compact, (intptr_t)&gSutPointIInfo },
    { offsetof(SutStructItem, floatArray), Type_FloatArray, NULL },
    { offsetof(SutStructItem, nested), Type_Struct, (intptr_t)&gSutStructNestedInfo },
};
static const StructInfo gSutStructItemInfo = { sizeof(SutStructItem), 3, gSutStructItemFields, "CompactPoint\0FloatArray\0Nested" };

struct SutStruct {
    int internal;
    bool boolean;
    COLORREF color;
    float floatingPoint;
    int integer;
    WCHAR *string;
    WCHAR *nullString;
    WCHAR *escapedString;
    char *utf8String;
    char *nullUtf8String;
    char *escapedUtf8String;
    Vec<int> *intArray;
    PointI point;
    Vec<SutStructItem *> *sutStructItems;
    char *internalString;
};

static const FieldInfo gSutStructFields[] = {
    { offsetof(SutStruct, boolean), Type_Bool, (intptr_t)true },
    { offsetof(SutStruct, color), Type_Color, 0xffcc9933 },
    { offsetof(SutStruct, floatingPoint), Type_Float, (intptr_t)"-3.14" },
    { offsetof(SutStruct, integer), Type_Int, 27 },
    { offsetof(SutStruct, string), Type_String, (intptr_t)L"String" },
    { offsetof(SutStruct, nullString), Type_String, NULL },
    { offsetof(SutStruct, escapedString), Type_String, (intptr_t)L"$\nstring " },
    { offsetof(SutStruct, utf8String), Type_Utf8String, (intptr_t)"Utf-8 String" },
    { offsetof(SutStruct, nullUtf8String), Type_Utf8String, NULL },
    { offsetof(SutStruct, escapedUtf8String), Type_Utf8String, (intptr_t)"$\nstring " },
    { offsetof(SutStruct, intArray), Type_IntArray, (intptr_t)"1 2 -3" },
    { offsetof(SutStruct, point), Type_Struct, (intptr_t)&gSutPointIInfo },
    { offsetof(SutStruct, sutStructItems), Type_Array, (intptr_t)&gSutStructItemInfo },
};
static const StructInfo gSutStructInfo = { sizeof(SutStruct), 13, gSutStructFields, "Boolean\0Color\0FloatingPoint\0Integer\0String\0NullString\0EscapedString\0Utf8String\0NullUtf8String\0EscapedUtf8String\0IntArray\0Point\0SutStructItems" };

static void SettingsUtilTest()
{
    static const char *serialized = UTF8_BOM "# This file will be overwritten - modify at your own risk!\r\n\r\n\
Boolean = true\r\n\
Color = #abcdef\r\n\
FloatingPoint = 2.7182\r\n\
Integer = -1234567890\r\n\
String = Might\\be\\a\\path\r\n\
EscapedString = $\t$r$n$$ $\r\n\
Utf8String = another string\r\n\
EscapedUtf8String = $r$n[]\t$\r\n\
IntArray = 3 1\r\n\
Point [\r\n\
\tX = -17\r\n\
\tY = -18\r\n\
\tZ = -19\r\n\
]\r\n\
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

    static const char *unknownOnly = UTF8_BOM "\
UnknownString: Forget-me-not\r\n\
[Point]\r\n\
Z: -19\r\n\
[UnknownNode]\r\n\
AnotherPoint: 7 8\r\n\
Nested [\r\n\
Key = Value";

    SutStruct *data = NULL;
    for (int i = 0; i < 3; i++) {
        data = (SutStruct *)DeserializeStruct(&gSutStructInfo, serialized, data);
        assert(data->internal == i);
        ScopedMem<char> reserialized(SerializeStruct(&gSutStructInfo, data, i < 2 ? unknownOnly : serialized));
        assert(str::Eq(serialized, reserialized));
        data->internal++;
    }
    assert(RGB(0xab, 0xcd, 0xef) == data->color);
    assert(str::Eq(data->escapedString, L"\t\r\n$ "));
    assert(str::Eq(data->escapedUtf8String, "\r\n[]\t"));
    assert(2 == data->intArray->Count() && 3 == data->intArray->At(0));
    assert(2 == data->sutStructItems->Count());
    assert(PointI(-1, 5) == data->sutStructItems->At(0)->compactPoint);
    assert(2 == data->sutStructItems->At(0)->floatArray->Count());
    assert(0 == data->sutStructItems->At(0)->nested.colorArray->Count());
    assert(0 == data->sutStructItems->At(1)->floatArray->Count());
    assert(2 == data->sutStructItems->At(1)->nested.colorArray->Count());
    assert(0x12785634 == data->sutStructItems->At(1)->nested.colorArray->At(0));
    assert(!data->internalString);
    assert(!str::Eq(serialized, ScopedMem<char>(SerializeStruct(&gSutStructInfo, data))));
    data->sutStructItems->At(0)->nested.point.x++;
    assert(!str::Eq(serialized, ScopedMem<char>(SerializeStruct(&gSutStructInfo, data, unknownOnly))));
    FreeStruct(&gSutStructInfo, data);

    data = (SutStruct *)DeserializeStruct(&gSutStructInfo, NULL);
    assert(data && data->boolean && 0xffcc9933 == data->color);
    assert(-3.14f == data->floatingPoint && 27 == data->integer);
    assert(str::Eq(data->string, L"String") && !data->nullString && str::Eq(data->escapedString, L"$\nstring "));
    assert(str::Eq(data->utf8String, "Utf-8 String") && !data->nullUtf8String && str::Eq(data->escapedUtf8String, "$\nstring "));
    assert(data->intArray && 3 == data->intArray->Count() && 1 == data->intArray->At(0));
    assert(2 == data->intArray->At(1) && -3 == data->intArray->At(2));
    assert(PointI(111, 222) == data->point);
    assert(data->sutStructItems && 0 == data->sutStructItems->Count());
    FreeStruct(&gSutStructInfo, data);
}
