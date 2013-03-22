/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef SerializeTxt_h
#define SerializeTxt_h

namespace sertxt {

struct FieldMetadata;

typedef struct {
    uint16_t        size;
    uint16_t        nFields;
    FieldMetadata * fields;
} StructMetadata;

typedef enum : uint16_t {
    TYPE_BOOL,
    TYPE_I16,
    TYPE_U16,
    TYPE_I32,
    TYPE_U32,
    TYPE_U64,
    TYPE_FLOAT,
    TYPE_COLOR,
    TYPE_STR,
    TYPE_WSTR,
    TYPE_STRUCT_PTR,
    TYP_ARRAY,
} Typ;

// information about a single field
struct FieldMetadata {
    const char *     name;
    Typ              type; // TYPE_*
    // from the beginning of the struct
    uint16_t         offset;
    // type is TYPE_STRUCT_PTR, otherwise NULL
    StructMetadata * def;
};

void        FreeStruct(uint8_t *data, StructMetadata *def);
uint8_t*    Deserialize(const uint8_t *data, int dataSize, const char *version, StructMetadata *def);
uint8_t *   Serialize(const uint8_t *data, const char *version, StructMetadata *def, int *sizeOut);

} // namespace sertxt

#endif
