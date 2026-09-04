/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/SettingsUtil.h"

// must be last due to assert() over-write
#include "base/UtAssert.h"

static const FieldInfo gSutPointIFields[] = {
    {offsetof(Point, x), SettingType::Int, 111},
    {offsetof(Point, y), SettingType::Int, 222},
};
static const StructInfo gSutPointIInfo = {sizeof(Point), 2, gSutPointIFields, "X\0Y"};

struct SutStructNested {
    Point point;
    Vec<Str>* colorArray;
};

static const FieldInfo gSutStructNestedFields[] = {
    {offsetof(SutStructNested, point), SettingType::Struct, (intptr_t)&gSutPointIInfo},
    {offsetof(SutStructNested, colorArray), SettingType::ColorArray, (intptr_t)"#000000 #ffffff"},
};
static const StructInfo gSutStructNestedInfo = {sizeof(SutStructNested), 2, gSutStructNestedFields,
                                                "Point\0ColorArray"};

struct SutStructItem {
    Vec<float>* floatArray;
    Point compactPoint;
    SutStructNested nested;
};

static const FieldInfo gSutStructItemFields[] = {
    {offsetof(SutStructItem, compactPoint), SettingType::Compact, (intptr_t)&gSutPointIInfo},
    {offsetof(SutStructItem, floatArray), SettingType::FloatArray, 0},
    {offsetof(SutStructItem, nested), SettingType::Struct, (intptr_t)&gSutStructNestedInfo},
};
static const StructInfo gSutStructItemInfo = {sizeof(SutStructItem), 3, gSutStructItemFields,
                                              "CompactPoint\0FloatArray\0Nested"};

struct SutStruct {
    int internal;
    bool boolean;
    Str color;
    float floatingPoint;
    int integer;
    Str string;
    Str nullString;
    Str escapedString;
    Str utf8String;
    Str nullUtf8String;
    Str escapedUtf8String;
    Vec<int>* intArray;
    Vec<Str>* strArray;
    Vec<Str>* emptyStrArray;
    Point point;
    Vec<SutStructItem*>* sutStructItems;
    Str internalString;
};

static const FieldInfo gSutStructFields[] = {
    {(size_t)-1, SettingType::Comment, (intptr_t)"This file will be overwritten - modify at your own risk!\r\n"},
    {offsetof(SutStruct, boolean), SettingType::Bool, (intptr_t)true},
    {offsetof(SutStruct, color), SettingType::Color, (intptr_t)"0xffcc9933"},
    {offsetof(SutStruct, floatingPoint), SettingType::Float, (intptr_t)"-3.14"},
    {offsetof(SutStruct, integer), SettingType::Int, 27},
    {offsetof(SutStruct, string), SettingType::String, (intptr_t)"String"},
    {offsetof(SutStruct, nullString), SettingType::String, 0},
    {offsetof(SutStruct, escapedString), SettingType::String, (intptr_t)"$\nstring "},
    {offsetof(SutStruct, utf8String), SettingType::String, (intptr_t)"Utf-8 String"},
    {offsetof(SutStruct, nullUtf8String), SettingType::String, 0},
    {offsetof(SutStruct, escapedUtf8String), SettingType::String, (intptr_t)"$\nstring "},
    {offsetof(SutStruct, intArray), SettingType::IntArray, (intptr_t)"1 2 -3"},
    {offsetof(SutStruct, strArray), SettingType::StringArray, (intptr_t)"one \"two three\" \"\""},
    {offsetof(SutStruct, emptyStrArray), SettingType::StringArray, 0},
    {offsetof(SutStruct, point), SettingType::Struct, (intptr_t)&gSutPointIInfo},
    {(size_t)-1, SettingType::Comment, 0},
    {offsetof(SutStruct, sutStructItems), SettingType::Array, (intptr_t)&gSutStructItemInfo},
};
static const StructInfo gSutStructInfo = {sizeof(SutStruct), 17, gSutStructFields,
                                          "\0Boolean\0Color\0FloatingPoint\0Integer\0String\0NullString\0EscapedString"
                                          "\0Utf8String\0NullUtf8String\0EscapedUtf8String\0IntArray\0StrArray\0EmptySt"
                                          "rArray\0Point\0\0SutStructItems"};

void SettingsUtilTest() {
    static const char* serialized = kUtf8Bom
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

    static const char* unknownOnly = kUtf8Bom
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
        data = (SutStruct*)DeserializeStruct(&gSutStructInfo, Str(serialized), data);
        utassert(data->internal == i);
        Str s = Str(serialized);
        if (i < 2) {
            s = Str(unknownOnly);
        }
        Str reserializedBs = SerializeStruct(&gSutStructInfo, data, s);
        utassert(str::Eq(Str(serialized), reserializedBs));
        str::Free(reserializedBs);
        data->internal++;
    }
    utassert(str::Eq(data->color, StrL("#abcdef")));
    utassert(str::Eq(data->escapedString, StrL("\t\r\n$ ")));
    utassert(str::Eq(data->escapedUtf8String, StrL("\r\n[]\t")));
    utassert(2 == len(*data->intArray) && 3 == (*data->intArray)[0]);
    utassert(3 == len(*data->strArray) && 0 == len(*data->emptyStrArray));
    utassert(str::Eq((*data->strArray)[0], StrL("with space")) && str::Eq((*data->strArray)[1], StrL("plain")) &&
             str::Eq((*data->strArray)[2], StrL("quote:\"")));
    utassert(2 == len(*data->sutStructItems));
    utassert(Point(-1, 5) == (*data->sutStructItems)[0]->compactPoint);
    utassert(2 == len(*(*data->sutStructItems)[0]->floatArray));
    utassert(0 == len(*(*data->sutStructItems)[0]->nested.colorArray));
    utassert(0 == len(*(*data->sutStructItems)[1]->floatArray));
    utassert(2 == len(*(*data->sutStructItems)[1]->nested.colorArray));
    utassert(str::Eq(StrL("#12345678"), (*(*data->sutStructItems)[1]->nested.colorArray)[0]));
    utassert(str::Eq(StrL("#987654"), (*(*data->sutStructItems)[1]->nested.colorArray)[1]));
    utassert(len(data->internalString) == 0);
    {
        Str res = SerializeStruct(&gSutStructInfo, data);
        utassert(!str::Eq(Str(serialized), res));
        str::Free(res);
    }
    (*data->sutStructItems)[0]->nested.point.x++;
    {
        Str res = SerializeStruct(&gSutStructInfo, data, Str(unknownOnly));
        utassert(!str::Eq(Str(serialized), res));
        str::Free(res);
    }
    FreeStruct(&gSutStructInfo, data);

    data = (SutStruct*)DeserializeStruct(&gSutStructInfo, Str());
    utassert(data);
    if (!data) {
        return;
    }
    utassert(data->boolean && str::Eq(StrL("0xffcc9933"), data->color));
    utassert(-3.14f == data->floatingPoint && 27 == data->integer);
    utassert(str::Eq(data->string, StrL("String")) && str::IsNull(data->nullString) &&
             str::Eq(data->escapedString, StrL("$\nstring ")));
    utassert(str::Eq(data->utf8String, StrL("Utf-8 String")) && str::IsNull(data->nullUtf8String) &&
             str::Eq(data->escapedUtf8String, StrL("$\nstring ")));
    utassert(data->intArray);
    utassert(3 == len(*data->intArray) && 1 == (*data->intArray)[0]);
    utassert(2 == (*data->intArray)[1] && -3 == (*data->intArray)[2]);
    utassert(data->strArray);
    utassert(data->emptyStrArray);
    utassert(3 == len(*data->strArray));
    utassert(0 == len(*data->emptyStrArray));
    Vec<Str>* sa = data->strArray;
    utassert(str::Eq((*sa)[0], StrL("one")));
    utassert(str::Eq((*sa)[1], StrL("two three")));
    utassert(str::Eq((*sa)[2], StrL("")));

    utassert(Point(111, 222) == data->point);
    utassert(data->sutStructItems && 0 == len(*data->sutStructItems));
    FreeStruct(&gSutStructInfo, data);

    static const char* boolData[] = {
        "Boolean = true", "Boolean = false", "Boolean = TRUE", "Boolean = FALSE", "Boolean = yes",
        "Boolean = no",   "Boolean = Yes",   "Boolean = No",   "Boolean = 1",     "Boolean = 0",
    };
    for (int i = 0; i < dimof(boolData); i++) {
        data = (SutStruct*)DeserializeStruct(&gSutStructInfo, Str(boolData[i]));
        utassert(data->boolean == ((i % 2) == 0));
        FreeStruct(&gSutStructInfo, data);
    }

    // Array elements with Bool field IsTemporary=true are omitted on serialize
    // (session-only favorites, issue #5862). The IsTemporary field itself is
    // never written.
    struct SutTempItem {
        Str name;
        int pageNo;
        bool isTemporary;
    };
    static const FieldInfo gSutTempItemFields[] = {
        {offsetof(SutTempItem, name), SettingType::String, 0},
        {offsetof(SutTempItem, pageNo), SettingType::Int, 0},
        {offsetof(SutTempItem, isTemporary), SettingType::Bool, (intptr_t)false},
    };
    static const StructInfo gSutTempItemInfo = {sizeof(SutTempItem),         3,       gSutTempItemFields,
                                                "Name\0PageNo\0IsTemporary", nullptr, true};

    struct SutTempRoot {
        Vec<SutTempItem*>* items;
    };
    static const FieldInfo gSutTempRootFields[] = {
        {offsetof(SutTempRoot, items), SettingType::Array, (intptr_t)&gSutTempItemInfo},
    };
    static const StructInfo gSutTempRootInfo = {sizeof(SutTempRoot), 1, gSutTempRootFields, "Items"};

    {
        auto* root = (SutTempRoot*)DeserializeStruct(&gSutTempRootInfo, Str());
        utassert(root && root->items);

        auto* keep = (SutTempItem*)DeserializeStruct(&gSutTempItemInfo, Str());
        str::ReplaceWithCopy(&keep->name, StrL("keep"));
        keep->pageNo = 3;
        keep->isTemporary = false;

        auto* drop = (SutTempItem*)DeserializeStruct(&gSutTempItemInfo, Str());
        str::ReplaceWithCopy(&drop->name, StrL("/"));
        drop->pageNo = 7;
        drop->isTemporary = true;

        auto* keep2 = (SutTempItem*)DeserializeStruct(&gSutTempItemInfo, Str());
        str::ReplaceWithCopy(&keep2->name, StrL("also"));
        keep2->pageNo = 9;
        keep2->isTemporary = false;

        VecAppend(*root->items, keep);
        VecAppend(*root->items, drop);
        VecAppend(*root->items, keep2);

        Str out = SerializeStruct(&gSutTempRootInfo, root);
        utassert(str::Contains(out, StrL("Name = keep")));
        utassert(str::Contains(out, StrL("PageNo = 3")));
        utassert(str::Contains(out, StrL("Name = also")));
        utassert(str::Contains(out, StrL("PageNo = 9")));
        utassert(!str::Contains(out, StrL("Name = /")));
        utassert(!str::Contains(out, StrL("PageNo = 7")));
        utassert(!str::Contains(out, StrL("IsTemporary")));
        // still three elements in memory after serialize
        utassert(3 == len(*root->items));

        // only the two non-temporary elements round-trip
        auto* loaded = (SutTempRoot*)DeserializeStruct(&gSutTempRootInfo, out);
        str::Free(out);
        utassert(loaded && loaded->items && 2 == len(*loaded->items));
        utassert(str::Eq((*loaded->items)[0]->name, StrL("keep")));
        utassert(3 == (*loaded->items)[0]->pageNo);
        utassert(!(*loaded->items)[0]->isTemporary);
        utassert(str::Eq((*loaded->items)[1]->name, StrL("also")));
        utassert(9 == (*loaded->items)[1]->pageNo);
        FreeStruct(&gSutTempRootInfo, loaded);
        FreeStruct(&gSutTempRootInfo, root);
    }

    // SettingType::StructPtr is an optional sub-struct: null by default and not
    // written at all until it's set (per-document EBookUI overrides, #4600)
    struct SutOptSub {
        Str name;
        int size;
    };
    static const FieldInfo gSutOptSubFields[] = {
        {offsetof(SutOptSub, name), SettingType::String, 0},
        {offsetof(SutOptSub, size), SettingType::Int, 0},
    };
    static const StructInfo gSutOptSubInfo = {sizeof(SutOptSub), 2, gSutOptSubFields, "Name\0Size"};

    struct SutOptRoot {
        int other;
        SutOptSub* sub;
    };
    static const FieldInfo gSutOptRootFields[] = {
        {offsetof(SutOptRoot, other), SettingType::Int, 7},
        {offsetof(SutOptRoot, sub), SettingType::StructPtr, (intptr_t)&gSutOptSubInfo},
    };
    static const StructInfo gSutOptRootInfo = {sizeof(SutOptRoot), 2, gSutOptRootFields, "Other\0Sub"};

    {
        // default: not set, and nothing about it in the output
        auto* root = (SutOptRoot*)DeserializeStruct(&gSutOptRootInfo, Str());
        utassert(root && !root->sub);
        Str out = SerializeStruct(&gSutOptRootInfo, root);
        utassert(str::Contains(out, StrL("Other = 7")));
        utassert(!str::Contains(out, StrL("Sub")));
        str::Free(out);

        // once set, it round-trips
        root->sub = (SutOptSub*)DeserializeStruct(&gSutOptSubInfo, Str());
        str::ReplaceWithCopy(&root->sub->name, StrL("Segoe UI"));
        root->sub->size = 14;
        out = SerializeStruct(&gSutOptRootInfo, root);
        utassert(str::Contains(out, StrL("Sub [")));
        utassert(str::Contains(out, StrL("Name = Segoe UI")));

        auto* loaded = (SutOptRoot*)DeserializeStruct(&gSutOptRootInfo, out);
        str::Free(out);
        utassert(loaded && loaded->sub);
        utassert(str::Eq(loaded->sub->name, StrL("Segoe UI")));
        utassert(14 == loaded->sub->size);
        FreeStruct(&gSutOptRootInfo, loaded);
        FreeStruct(&gSutOptRootInfo, root);

        // a block in the data with none of its fields set still means "set"
        auto* empty = (SutOptRoot*)DeserializeStruct(&gSutOptRootInfo, StrL(kUtf8Bom "Sub [\r\n]\r\n"));
        utassert(empty && empty->sub && len(empty->sub->name) == 0 && 0 == empty->sub->size);

        // deserializing onto an existing struct merges, like the other types:
        // fields not in the data keep their value, present ones win
        str::ReplaceWithCopy(&empty->sub->name, StrL("old"));
        empty->sub->size = 3;
        DeserializeStruct(&gSutOptRootInfo, StrL(kUtf8Bom "Other = 9\r\nSub [\r\n\tSize = 5\r\n]\r\n"), empty);
        utassert(9 == empty->other && empty->sub);
        utassert(str::Eq(empty->sub->name, StrL("old")) && 5 == empty->sub->size);
        FreeStruct(&gSutOptRootInfo, empty);
    }
}
