/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "Settings.h"

namespace settings {

#define MAGIC_ID 0x53657454  // 'SetT' as 'Settings'

#define SERIALIZED_HEADER_LEN 12

typedef struct {
    uint32_t   magicId;
    uint32_t   version;
    uint32_t   topLevelStructOffset;
} SerializedHeader;

STATIC_ASSERT(sizeof(SerializedHeader) == SERIALIZED_HEADER_LEN, SerializedHeader_is_12_bytes);

STATIC_ASSERT(sizeof(FieldMetadata) == 4 + sizeof(StructMetadata*), valid_FieldMetadata_size);

// returns -1 on error
static int NextIntVal(const char **sInOut)
{
    const char *s = *sInOut;
    char c;
    int val = 0;
    int n;
    for (;;) {
        c = *s++;
        if (0 == c) {
            s--; // position at ending 0
            goto Exit;
        }
        if ('.' == c)
            goto Exit;
        n = c - '0';
        if ((c < 0) || (c > 9))
            return -1;
        val *= 10;
        val += n;
    }
Exit:
    if (val > 255)
        return -1;
    *sInOut = s;
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
    }
Exit:
    ver = ver << (left * 8);
    return ver;
}

// the assumption here is that the data was either build by Deserialize()
// or was set by application code in a way that observes our rule: each
// object was separately allocated with malloc()
void FreeStruct(uint8_t *data, StructMetadata *def)
{
    if (!data)
        return;
    FieldMetadata *fieldDef = NULL;
    for (int i = 0; i < def->nFields; i++) {
        fieldDef = def->fields + i;
        if (TYPE_STRUCT_PTR ==  fieldDef->type) {
            uint8_t **p = (uint8_t**)(data + fieldDef->offset);
            FreeStruct(*p, fieldDef->def);
        } else if (TYPE_STR == fieldDef->type) {
            char **sp = (char**)(data + fieldDef->offset);
            char *s = *sp;
            free(s);
        }
    }
    free(data);
}

static bool IsSignedIntType(uint16_t type)
{
    return ((TYPE_I16 == type) ||
            (TYPE_I32 == type));
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

static bool WriteStructUInt(uint8_t *p, Typ type, uint64_t val)
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

    if (TYPE_U32 == type) {
        if (val > 0xffffffff)
            return false;
        uint32_t v = (uint32_t)val;
        uint32_t *vp = (uint32_t*)p;
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
    int n = GobVarintDecode(ds->data + ds->off, ds->dataSize - ds->off, &ds->i);
    ds->off += n;
    return 0 != n;
}

static bool DecodeUInt(DecodeState *ds)
{
    int n = GobUVarintDecode(ds->data + ds->off, ds->dataSize - ds->off, &ds->u);
    ds->off += n;
    return 0 != n;
}

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

static uint8_t* DeserializeRec(DecodeState *ds, StructMetadata *def)
{
    uint8_t *res = AllocArray<uint8_t>(def->size);
    FieldMetadata *fieldDef;
    Typ type;
    uint8_t *structDataPtr;
    bool ok = true;
    for (int i = 0; i < def->nFields; i++) {
        // previous decode failed
        if (!ok)
            goto Error;
        fieldDef = def->fields + i;
        structDataPtr = res + fieldDef->offset;
        type = fieldDef->type;
        if (TYPE_STR == type) {
            ok = DecodeString(ds);
            if (ok && (ds->sLen > 0)) {
                char *s = str::DupN(ds->s, ds->sLen - 1);
                WriteStructStr(structDataPtr, s);
            }
        } else if (TYPE_STRUCT_PTR == type) {
            ok = DecodeOffset(ds);
            if (!ok)
                goto Error;
            DecodeState ds2 = { ds->data, ds->dataSize, ds->decodedOffset, 0, 0, 0, 0, 0, 0 };
            uint8_t *d = DeserializeRec(&ds2, fieldDef->def);
            if (!d)
                goto Error;
            WriteStructPtrVal(structDataPtr, d);
        } else if (TYPE_FLOAT == type) {
            ok = DecodeFloat(ds);
            if (ok)
                WriteStructFloat(structDataPtr, ds->f);
        } else {
            if (IsSignedIntType(type)) {
                ok = DecodeInt(ds);
                if (ok)
                    ok = WriteStructInt(structDataPtr, type, ds->i);
            } else {
                ok = DecodeUInt(ds);
                if (ok)
                    ok = WriteStructUInt(structDataPtr, type, ds->u);
            }
        }
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
// TODO: when version of the data doesn't match our version,
// especially in the case of our version being higher (and hence
// having more data), we should decode the default values and
// then over-write them with whatever values we decoded.
// alternatively, we could keep a default value in struct metadata
uint8_t* Deserialize(const uint8_t *data, int dataSize, const char *version, StructMetadata *def)
{
    if (!data)
        return NULL;
    if (dataSize < sizeof(SerializedHeader))
        return NULL;
    SerializedHeader *hdr = (SerializedHeader*)data;
    if (hdr->magicId != MAGIC_ID)
        return NULL;
    //uint32_t ver = VersionFromStr(version);
    DecodeState ds = { data, dataSize, hdr->topLevelStructOffset, 0, 0, 0, 0, 0, 0 };
    return DeserializeRec(&ds, def);
}

// TODO: write me
uint8_t *Serialize(const uint8_t *data, const char *version, StructMetadata *def, int *sizeOut)
{
    *sizeOut = 0;
    return NULL;
}

} // namespace Settings

