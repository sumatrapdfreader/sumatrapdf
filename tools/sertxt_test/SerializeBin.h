/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef SerializeBin_h
#define SerializeBin_h

namespace serbin {

struct FieldMetadata;

template <typename T>
struct ListNode {
    ListNode<T> *   next;
    T *             val;
};

typedef struct {
    uint16_t        size;
    uint16_t        nFields;
    FieldMetadata * fields;
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
    TYPE_NO_FLAGS_MASK = 0xFF,
    // a flag, if set the value is not to be serialized
    TYPE_NO_STORE_MASK = 0x4000,
} Type;

// information about a single field
struct FieldMetadata {
    // from the beginning of the struct
    uint16_t         offset;
    Type             type; // TYPE_*
    // type is TYPE_STRUCT_PTR, otherwise NULL
    StructMetadata * def;
};

void        FreeStruct(uint8_t *data, StructMetadata *def);
uint8_t*    Deserialize(const uint8_t *data, int dataSize, const char *version, StructMetadata *def);
uint8_t *   Serialize(const uint8_t *data, const char *version, StructMetadata *def, int *sizeOut);

} // namespace serbin

#endif
