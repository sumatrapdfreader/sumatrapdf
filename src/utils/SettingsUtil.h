/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/*
Since version 2.3, settings are serialized in a structure of the following form:

# comment linking to more information about the following values
SettingName = string value (might also be a number, etc.)
SubSettings [
    Boolean = true
    Rectangle = 0 0 40 50
    Escaped = $ (leading space), $r$n (newline), $$ (escape character) and trailing space: $
    ValueArray [
        [
            ItemNo = 1
        ]
        [
            ItemNo = 2
        ]
    ]
]

See SquareTreeParser.cpp for further details on variations allowed during
the deserialization of such a settings file.
*/

enum SettingType {
    Type_Struct,
    Type_Array,
    Type_Compact,
    Type_Bool,
    Type_Color,
    Type_Float,
    Type_Int,
    Type_String,
    Type_Utf8String,
    Type_ColorArray,
    Type_FloatArray,
    Type_IntArray,
    Type_StringArray,
    Type_Comment,
    // same as Type_Struct but won't be written out in release builds
    Type_Prerelease,
};

struct FieldInfo {
    // offset of the field in the struct
    size_t offset = 0;
    SettingType type = Type_Struct;
    // default value for primitive types and pointer to StructInfo for complex ones
    intptr_t value = 0;
};

struct StructInfo {
    uint16_t structSize = 0;
    uint16_t fieldCount = 0;
    const FieldInfo* fields = nullptr;
    // one string of fieldCount zero-terminated names of all fields
    // in the order of fields
    const char* fieldNames = nullptr;
};

char* SerializeStruct(const StructInfo* info, const void* strct, const char* prevData = nullptr,
                      size_t* sizeOut = nullptr);
void* DeserializeStruct(const StructInfo* info, const char* data, void* strct = nullptr);
void FreeStruct(const StructInfo* info, void* strct);
