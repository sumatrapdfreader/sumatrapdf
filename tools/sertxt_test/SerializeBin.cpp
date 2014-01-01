/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "SerializeBin.h"

#include "VarintGob.h"

namespace serbin {

#define MAGIC_ID 0x53657454  // 'SetT' for 'Settings'

#define SERIALIZED_HEADER_LEN 12

typedef struct {
    uint32_t   magicId;
    uint32_t   version;
    uint32_t   topLevelStructOffset;
} SerializedHeader;

// these assertions don't always hold without #pragma pack
// STATIC_ASSERT(sizeof(SerializedHeader) == SERIALIZED_HEADER_LEN, SerializedHeader_is_12_bytes);
// STATIC_ASSERT(sizeof(FieldMetadata) == 4 + sizeof(StructMetadata*), valid_FieldMetadata_size);

#if 0
// returns -1 on error
static int NextIntVal(const char **sInOut)
{
    // cf. ExtractNextNumber in AppTools.cpp
    int val = 0;
    const char *end = str::Parse(*sInOut, "%d%?.", &val);
    if (!end || val < 0 || val > 255)
        return -1;
    *sInOut = end;
    return val;
}

// parses a vrsion string in the format "x.y.z", of up to 4 parts
// return 0 on parsing error
static uint32_t VersionFromStr(const char *s)
{
    uint32_t ver = 0;
    int left = 4;
    int n;
    while (left > 0) {
        if (0 == *s)
            goto Exit;
        n = NextIntVal(&s);
        if (-1 == n)
            return 0;
        --left;
        ver = (ver << 8) + n;
    }
Exit:
    ver = ver << (left * 8);
    return ver;
}
#endif

// the assumption here is that the data was either built by Deserialize()
// or was created by application code in a way that observes our rule: each
// struct and string was separately allocated with malloc()
void FreeStruct(uint8_t *structDataStart, const StructMetadata *def)
{
    if (!structDataStart)
        return;
    const FieldMetadata *fieldDef = NULL;
    Type type;
    uint8_t *data;
    for (int i = 0; i < def->nFields; i++) {
        fieldDef = def->fields + i;
        type = (Type)(fieldDef->type & TYPE_MASK);
        data = structDataStart + fieldDef->offset;
        if (TYPE_STRUCT_PTR ==  type) {
            uint8_t **p = (uint8_t**)(data);
            FreeStruct(*p, fieldDef->def);
        } else if (TYPE_ARRAY == type) {
            Vec<uint8_t*> **vecPtr = (Vec<uint8_t*> **)data;
            Vec<uint8_t*> *vec = *vecPtr;
            for (size_t i = 0; i < vec->Count(); i++) {
                FreeStruct(vec->At(i), fieldDef->def);
            }
            delete vec;
        } else if ((TYPE_STR == type) || (TYPE_WSTR == type)) {
            char **sp = (char**)(data);
            char *s = *sp;
            free(s);
        }
    }
    free(structDataStart);
}

static bool IsSignedIntType(Type type)
{
    return ((TYPE_I16 == type) ||
            (TYPE_I32 == type));
}

static bool IsUnsignedIntType(Type type)
{
    return ((TYPE_BOOL == type) ||
            (TYPE_U16 == type) ||
            (TYPE_U32 == type) ||
            (TYPE_COLOR == type) ||
            (TYPE_U64 == type));
}

static bool WriteStructInt(uint8_t *p, uint16_t type, int64_t val)
{
    if (TYPE_I16 == type) {
        if (val > 0xffff)
            return false;
        int16_t v = (int16_t)val;
        int16_t *vp = (int16_t*)p;
        *vp = v;
        return true;
    }

    if (TYPE_I32 == type) {
        if (val > 0xffffffff)
            return false;
        int32_t v = (int32_t)val;
        int32_t *vp = (int32_t*)p;
        *vp = v;
        return true;
    }
    CrashIf(true);
    return false;
}

// TODO: optimize
static COLORREF Uint32ToCOLORREF(uint32_t u)
{
    int a = ((u >> 24) & 0xff);
    int r = ((u >> 16) & 0xff);
    int g = ((u >> 8) & 0xff);
    int b = u & 0xff;
    COLORREF col =  RGB(r, g, b);
    COLORREF alpha = (COLORREF)a;
    alpha = alpha << 24;
    col = col | alpha;
    return col;
}

// TODO: optimize
#if 0
static uint32_t COLORREFToUint32(COLORREF c)
{
    uint32_t r = (uint32_t)(c & 0xff);
    uint32_t g = (uint32_t)((c >> 8) & 0xff);
    uint32_t b = (uint32_t)((c >> 16) & 0xff);
    uint32_t a = (uint32_t)(c & 0xffffff);
    uint32_t col = a;
    col = col | (r >> 16);
    col = col | (g >> 8);
    col = col | b;
    return col;
}
#endif

static bool WriteStructUInt(uint8_t *p, Type type, uint64_t val)
{
    if (TYPE_BOOL == type) {
        bool *bp = (bool*)p;
        if (1 == val) {
            *bp = true;
            return true;
        }
        if (0 == val) {
            *bp = false;
            return true;
        }
        return false;
    }

    if (TYPE_U16 == type) {
        if (val > 0xffff)
            return false;
        uint16_t v = (uint16_t)val;
        uint16_t *vp = (uint16_t*)p;
        *vp = v;
        return true;
    }

    if ((TYPE_U32 == type) || (TYPE_COLOR == type)) {
        if (val > 0xffffffff)
            return false;
        uint32_t v = (uint32_t)val;
        uint32_t *vp = (uint32_t*)p;
        if (TYPE_COLOR == type)
            *vp = Uint32ToCOLORREF(v);
        else
            *vp = v;
        return true;
    }

    if (TYPE_U64 == type) {
        uint64_t *vp = (uint64_t*)p;
        *vp = val;
        return true;
    }

    CrashIf(true);
    return false;
}

static void WriteStructPtrVal(uint8_t *p, void *val)
{
    void **pp = (void**)p;
    *pp = val;
}

static void WriteStructStr(uint8_t *p, char *s)
{
    char **sp = (char **)p;
    *sp = s;
}

static void WriteStructWStr(uint8_t *p, WCHAR *s)
{
    WCHAR **sp = (WCHAR **)p;
    *sp = s;
}

static void WriteStructFloat(uint8_t *p, float f)
{
    float *fp = (float*)p;
    *fp = f;
}

typedef struct {
    // data being decoded
    const uint8_t *   data;
    int               dataSize;

    // current offset within the data
    int               off;

    // number of fields
    int               nFields;

    // last decoded value
    uint64_t          u;
    int64_t           i;
    int               decodedOffset;
    float             f;
    char *            s;
    int               sLen;
} DecodeState;

static bool DecodeInt(DecodeState *ds)
{
    int n = VarintGobDecode(ds->data + ds->off, ds->dataSize - ds->off, &ds->i);
    ds->off += n;
    return 0 != n;
}

static bool DecodeUInt(DecodeState *ds)
{
    int n = UVarintGobDecode(ds->data + ds->off, ds->dataSize - ds->off, &ds->u);
    ds->off += n;
    return 0 != n;
}

// TODO: make it return 0 on failure, since real offsets are never 0
static bool DecodeOffset(DecodeState *ds)
{
    bool ok = DecodeUInt(ds);
    if (!ok)
        return false;
    ds->decodedOffset = (int)ds->u;
    // validate the offset
    if (ds->decodedOffset < SERIALIZED_HEADER_LEN || ds->decodedOffset >= ds->dataSize)
        return false;
    return true;
}

static bool DecodeString(DecodeState *ds)
{
    bool ok = DecodeUInt(ds);
    if (!ok)
        return false;
    int strLen = (int)ds->u; // including terminating 0
    // sanity check to avoid integer overflow issues
    if (strLen < 0)
        return false;
    if (ds->off + strLen > ds->dataSize)
        return false;
    ds->s = (char*)(ds->data + ds->off);
    ds->off += strLen;
    ds->sLen = strLen;
    // validate that non-empty strings end with 0
    if (strLen > 0 && (0 != ds->s[strLen-1]))
        return false;
    return true;
}

static bool DecodeFloat(DecodeState *ds)
{
    bool ok = DecodeString(ds);
    if (!ok)
        return false;
    char *end;
    ds->f = (float)strtod(ds->s, &end);
    return true;
}

// Struct in encoded form starts with:
// - 4 bytes, magic id constant, for detecting corrupt data
// - uvarint, number of fields in the struct
static bool DecodeStructHeader(DecodeState *ds)
{
    if (ds->off + 5 > ds->dataSize)
        return false;
    // TODO: could be an issue with non-aligned access, but not on x86
    uint32_t *magicIdPtr = (uint32_t*)(ds->data + ds->off);
    if (MAGIC_ID != *magicIdPtr)
        return false;
    ds->off += 4;
    if (!DecodeUInt(ds))
        return false;
    // sanity check to prevent overflow problems
    if (ds->u > 1024)
        return false;
    ds->nFields = (int)ds->u;
    return true;
}

static uint8_t* DeserializeRec(DecodeState *ds, const StructMetadata *def);

static bool DecodeField(DecodeState *ds, const FieldMetadata *fieldDef, uint8_t *structDataStart)
{
    bool ok;
    Type type = fieldDef->type;
    if ((type & TYPE_NO_STORE_MASK) != 0)
        return true;
    type = (Type)(type & TYPE_MASK);

    uint8_t *structDataPtr = structDataStart + fieldDef->offset;
    if (TYPE_STR == type) {
        ok = DecodeString(ds);
        if (ok && (ds->sLen > 0)) {
            char *s = str::DupN(ds->s, ds->sLen - 1);
            WriteStructStr(structDataPtr, s);
        }
    } else if (TYPE_WSTR == type) {
        ok = DecodeString(ds);
        if (ok && (ds->sLen > 0)) {
            WCHAR *ws = str::conv::FromUtf8(ds->s);
            WriteStructWStr(structDataPtr, ws);
        }
    } else if (TYPE_ARRAY == type) {
        CrashIf(!fieldDef->def); // array elements must be a struct
        ok = DecodeUInt(ds);
        if (!ok)
            goto Error;
        size_t nElems = (size_t)ds->u;
        Vec<uint8_t*> *vec = new Vec<uint8_t*>(nElems);
        // we remember it right away, so that it gets freed in case of error
        WriteStructPtrVal(structDataPtr, (void*)vec);
        for (size_t i = 0; i < nElems; i++) {
            ok = DecodeOffset(ds);
            if (!ok)
                goto Error;
            DecodeState ds2 = { ds->data, ds->dataSize, ds->decodedOffset, 0, 0, 0, 0, 0, 0, 0 };
            uint8_t *d = DeserializeRec(&ds2, fieldDef->def);
            if (!d)
                goto Error;
            vec->Append(d);
        }
    } else if (TYPE_STRUCT_PTR == type) {
        ok = DecodeOffset(ds);
        if (!ok)
            goto Error;
        DecodeState ds2 = { ds->data, ds->dataSize, ds->decodedOffset, 0, 0, 0, 0, 0, 0, 0 };
        uint8_t *d = DeserializeRec(&ds2, fieldDef->def);
        if (!d)
            goto Error;
        WriteStructPtrVal(structDataPtr, d);
    } else if (TYPE_FLOAT == type) {
        ok = DecodeFloat(ds);
        if (ok)
            WriteStructFloat(structDataPtr, ds->f);
    } else if (IsSignedIntType(type)) {
        ok = DecodeInt(ds);
        if (ok)
            ok = WriteStructInt(structDataPtr, type, ds->i);
    } else if (IsUnsignedIntType(type)) {
        ok = DecodeUInt(ds);
        if (ok)
            ok = WriteStructUInt(structDataPtr, type, ds->u);
    } else {
        CrashIf(true);
        ok = false;
    }
    return ok;
Error:
    return false;
}

// TODO: do parallel decoding from default data and data from the client
// if no data from client - return the result from default data
// if data from client doesn't have enough fields, use fields from default data
// if data from client is corrupted, decode default data
static uint8_t* DeserializeRec(DecodeState *ds, const StructMetadata *def)
{
    uint8_t *res = AllocArray<uint8_t>(def->size);
    bool ok = DecodeStructHeader(ds);
    for (int i = 0; i < def->nFields; i++) {
        // previous decode failed
        if (!ok)
            goto Error;
        // if verion N decodes data from version N - 1, the number
        // of fields in the data might be less than number of fields
        // in the struct, so we stop when there's no more data to decode
        if (i + 1 > ds->nFields)
            return res;
        ok = DecodeField(ds, def->fields + i, res);
    }
    return res;
Error:
    FreeStruct(res, def);
    return NULL;
}

// a serialized format is a linear chunk of memory with pointers
// replaced with offsets from the beginning of the memory (base)
// to deserialize, we malloc() each struct and replace offsets
// with pointers to those newly allocated structs
uint8_t* Deserialize(const uint8_t *data, int dataSize, const char *version, const StructMetadata *def)
{
    if (!data)
        return NULL;
    if (dataSize < sizeof(SerializedHeader))
        return NULL;
    SerializedHeader *hdr = (SerializedHeader*)data;
    if (hdr->magicId != MAGIC_ID)
        return NULL;
    //uint32_t ver = VersionFromStr(version);
    DecodeState ds = { data, dataSize, hdr->topLevelStructOffset, 0, 0, 0, 0, 0, 0, 0 };
    return DeserializeRec(&ds, def);
}

// TODO: write me
uint8_t *Serialize(const uint8_t *data, const char *version, const StructMetadata *def, int *sizeOut)
{
    *sizeOut = 0;
    return NULL;
}

} // namespace serbin
