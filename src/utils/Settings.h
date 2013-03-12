/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Settings_h
#define Settings_h

namespace settings {

struct FieldMetadata;

typedef struct {
    uint16_t        size;
    uint16_t        nFields;
    FieldMetadata * fields;
} StructMetadata;

typedef enum : uint16_t {
    TYPE_BOOL         = 0,
    TYPE_I16          = 1,
    TYPE_U16          = 2,
    TYPE_I32          = 3,
    TYPE_U32          = 4,
    TYPE_U64          = 5,
    TYPE_STR          = 6,
    TYPE_STRUCT_PTR   = 7,
} Typ;

// information about a single field
struct FieldMetadata {
    Typ              type; // TYPE_*
    // from the beginning of the struct
    uint16_t         offset;
    // type is TYPE_STRUCT_PTR, otherwise NULL
    StructMetadata * def;
};

void FreeStruct(uint8_t *data, StructMetadata *def);
uint8_t* Deserialize(const uint8_t *data, int dataSize, const char *version, StructMetadata *def);
uint8_t *Serialize(const uint8_t *data, const char *version, StructMetadata *def, int *sizeOut);

} // namespace Settings

#endif
