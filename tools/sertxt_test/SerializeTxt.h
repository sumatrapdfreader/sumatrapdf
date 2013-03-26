/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef SerializeTxt_h
#define SerializeTxt_h

namespace sertxt {

struct FieldMetadata;

// TODO: to make the operations on elements easier, ListNode should be nt kernel style
// list i.e. double-linked, with root that points to start and end
#if 0
template <typename T>
struct ListNode {
    ListNode<T> *   next;
    ListNode<T> *   prev;
    void *          val;
}

template <typename T>
struct ListRoot {
    ListNode<T> *   first;
    ListNode<T> *   last;
};
#endif

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
    // a flag, if set, the value is not to be serialized
    TYPE_NO_STORE_MASK = 0x4000
} Type;

// information about a single field
struct FieldMetadata {
    uint16_t         nameOffset;
    // from the beginning of the struct
    uint16_t         offset;
    Type             type;
    // for TYP_ARRAY and TYPE_STRUCT_PTR, otherwise NULL
    StructMetadata * def;
};

void        FreeStruct(uint8_t *data, StructMetadata *def);
uint8_t*    Deserialize(char *data, size_t dataSize, StructMetadata *def, const char *fieldNamesSeq);
uint8_t *   Serialize(const uint8_t *data, StructMetadata *def, const char *fieldNamesSeq, size_t *sizeOut);

} // namespace sertxt

#endif
