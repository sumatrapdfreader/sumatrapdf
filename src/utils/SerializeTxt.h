/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct TxtNode;

namespace sertxt {

struct FieldMetadata;

typedef struct {
    u16 size;
    u16 nFields;
    const char* fieldNames;
    const FieldMetadata* fields;
} StructMetadata;

typedef enum : u16 {
    TYPE_BOOL = 0,
    TYPE_I16 = 1,
    TYPE_U16 = 2,
    TYPE_I32 = 3,
    TYPE_U32 = 4,
    TYPE_U64 = 5,
    TYPE_FLOAT = 6,
    TYPE_COLOR = 7,
    TYPE_STR = 8,
    TYPE_WSTR = 9,
    TYPE_STRUCT_PTR = 10,
    TYPE_ARRAY = 11,
    // do && with TYPE_MASK to get just the type, no flags
    TYPE_MASK = 0xFF,
    // a flag, if set the value is not to be serialized
    TYPE_NO_STORE_MASK = 0x4000,
    // a flag, if set the value is serialized in a compact form
    TYPE_STORE_COMPACT_MASK = 0x8000,
} Type;

// TODO: re-arrange fields for max compactness
// information about a single field
struct FieldMetadata {
    // offset of the value from the beginning of the struct
    u16 offset;
    Type type;
    // StructMetadata * for TYP_ARRAY and TYPE_STRUCT_PT
    // otherwise default value for this field
    uintptr_t defValOrDefinition;
};

std::string_view Serialize(const u8* data, const StructMetadata* def);
u8* Deserialize(struct TxtNode* root, const StructMetadata* def);
u8* Deserialize(const std::string_view str, const StructMetadata* def);
void FreeStruct(u8* data, const StructMetadata* def);

} // namespace sertxt
