/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
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

enum class SettingType {
    Struct,
    // an optional sub-struct, stored as a pointer that is null when unset.
    // it's only written out when set, so a struct with many instances (e.g.
    // FileState) doesn't carry an empty block per instance
    StructPtr,
    Array,
    Compact,
    Bool,
    Color,
    Float,
    Int,
    String,
    ColorArray,
    FloatArray,
    IntArray,
    StringArray,
    Comment,
};

struct FieldInfo {
    // offset of the field in the struct
    size_t offset = 0;
    SettingType type = SettingType::Struct;
    // default value for primitive types and pointer to StructInfo for complex ones
    intptr_t value = 0;
    // app-managed / deprecated setting: serialized like any other, but hidden
    // from the advanced settings dialog
    bool internal = false;
};

struct StructInfo {
    u16 structSize = 0;
    u16 fieldCount = 0;
    const FieldInfo* fields = nullptr;
    // one string of fieldCount zero-terminated names of all fields
    // in the order of fields
    const char* fieldNames = nullptr;
    // one string of fieldCount zero-terminated per-field doc comments, in the
    // same order as fieldNames (empty entry if a field has no comment)
    const char* fieldComments = nullptr;
    // true if this struct has a Bool field named IsTemporary (array elements
    // with that flag set are omitted when serializing)
    bool couldBeTemporary = false;
};

Str SerializeStruct(const StructInfo* info, const void* strct, Str prevData = {});
void* DeserializeStruct(const StructInfo* info, Str data, void* strct = nullptr);
void FreeStruct(const StructInfo* info, void* strct);
