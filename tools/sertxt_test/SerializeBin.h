/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef SerializeBin_h
#define SerializeBin_h

namespace serbin {

struct FieldMetadata;

typedef struct {
    uint16_t        size;
    uint16_t        nFields;
    const FieldMetadata * fields;
} StructMetadata;

typedef enum {
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
    TYPE_ARRAY,
    // do && with TYPE_MASK to get just the type, no flags
    TYPE_MASK = 0xFF,
    // a flag, if set the value is not to be serialized
    TYPE_NO_STORE_MASK = 0x4000,
} Type;

// information about a single field
struct FieldMetadata {
    // from the beginning of the struct
    uint16_t         offset;
    Type             type; // TYPE_*
    // type is TYPE_STRUCT_PTR, otherwise NULL
    const StructMetadata * def;
};

void        FreeStruct(uint8_t *data, const StructMetadata *def);
uint8_t*    Deserialize(const uint8_t *data, int dataSize, const char *version, const StructMetadata *def);
uint8_t *   Serialize(const uint8_t *data, const char *version, const StructMetadata *def, int *sizeOut);

} // namespace serbin

#endif
