/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "Settings.h"

#include "BitManip.h"

namespace settings {

#define MAGIC_ID 0x53657454  // 'SetT' for 'Settings'

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

// the assumption here is that the data was either built by Deserialize()
// or was created by application code in a way that observes our rule: each
// struct and string was separately allocated with malloc()
void FreeStruct(uint8_t *data, StructMetadata *def)
{
    if (!data)
        return;
    FieldMetadata *fieldDef = NULL;
    Typ type;
    for (int i = 0; i < def->nFields; i++) {
        fieldDef = def->fields + i;
        type = fieldDef->type;
        if (TYPE_STRUCT_PTR ==  type) {
            uint8_t **p = (uint8_t**)(data + fieldDef->offset);
            FreeStruct(*p, fieldDef->def);
        } else if ((TYPE_STR == type) || (TYPE_WSTR == type)) {
            char **sp = (char**)(data + fieldDef->offset);
            char *s = *sp;
            free(s);
        }
    }
    free(data);
}

static bool IsSignedIntType(Typ type)
{
    return ((TYPE_I16 == type) ||
            (TYPE_I32 == type));
}

static bool IsUnsignedIntType(Typ type)
{
    return ((TYPE_BOOL == type) ||
            (TYPE_U16 == type) ||
            (TYPE_U32 == type) ||
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

static uint8_t* DeserializeRec(DecodeState *ds, StructMetadata *def);

static bool DecodeField(DecodeState *ds, FieldMetadata *fieldDef, uint8_t *structDataStart)
{
    bool ok;
    Typ type = fieldDef->type;
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
    }
    return ok;
Error:
    return false;
}

// TODO: do parallel decoding from default data and data from the client
// if no data from client - return the result from default data
// if data from client doesn't have enough fields, use fields from default data
// if data from client is corrupted, decode default data
static uint8_t* DeserializeRec(DecodeState *ds, StructMetadata *def)
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
    DecodeState ds = { data, dataSize, hdr->topLevelStructOffset, 0, 0, 0, 0, 0, 0, 0 };
    return DeserializeRec(&ds, def);
}

// TODO: write me
uint8_t *Serialize(const uint8_t *data, const char *version, StructMetadata *def, int *sizeOut)
{
    *sizeOut = 0;
    return NULL;
}

} // namespace Settings

// Varint decoding/encoding is the same as the scheme used by GOB in Go.
// For unsinged 64-bit integer
// - if the value is <= 0x7f, it's written as a single byte
// - other values are written as:
//  - count of bytes, negated
//  - bytes of the number follow
// Signed 64-bit integer is turned into unsigned by:
//  - negative value has first bit set to 1 and other bits
//    are negated and shifted by one to the left
//  - positive value has first bit set to 0 and other bits
//    shifted by one to the left
// Smmaller values are cast to uint64 or int64, according to their signed-ness
//
// The downside of this scheme is that it's impossible to tell
// if the value is signed or unsigned from the value itself.
// The client needs to know the sign-edness to properly interpret the data

// decodes unsigned 64-bit int from data d of dLen size
// returns 0 on error
// returns the number of consumed bytes from d on success
int GobUVarintDecode(const uint8_t *d, int dLen, uint64_t *resOut)
{
    if (dLen < 1)
        return 0;
    uint8_t b = *d++;
    if (b <= 0x7f) {
        *resOut = b;
        return 1;
    }
    --dLen;
    if (dLen < 1)
        return 0;
    char numLenEncoded = (char)b;
    int numLen = -numLenEncoded;
    CrashIf(numLen < 1);
    CrashIf(numLen > 8);
    if (numLen > dLen)
        return 0;
    uint64_t res = 0;
    for (int i=0; i < numLen; i++) {
        b = *d++;
        res = (res << 8) | b;
    }
    *resOut = res;
    return 1 + numLen;
}

int GobVarintDecode(const uint8_t *d, int dLen, int64_t *resOut)
{
    uint64_t val;
    int n = GobUVarintDecode(d, dLen, &val);
    if (n == 0)
        return 0;

    bool negative = bit::IsSet(val, 0);
    val = val >> 1;
    int64_t res = (int64_t)val;
    if (negative)
        res = ~res;
    *resOut = res;
    return n;
}

// max 8 bytes plus 1 to encode size
static const int MinGobEncodeBufferSize = 9;
static const int UInt64SizeOf = 8;

// encodes unsigned integer val into a buffer d of dLen size (must be at least 9 bytes)
// returns number of bytes used
int GobUVarintEncode(uint64_t val, uint8_t *d, int dLen)
{
    uint8_t b;
    CrashIf(dLen < MinGobEncodeBufferSize);
    if (val <= 0x7f) {
        *d = (int8_t)val;
        return 1;
    }

    uint8_t buf[UInt64SizeOf];
    uint8_t *bufPtr = buf + UInt64SizeOf;
    int len8Minus = UInt64SizeOf;
    while (val > 0) {
        b = (uint8_t)(val & 0xff);
        val = val >> 8;
        --bufPtr;
        *bufPtr = b;
        --len8Minus;
    }
    CrashIf(len8Minus < 1);
    int realLen = 8-len8Minus;
    CrashIf(realLen > 8);
    int lenEncoded = (len8Minus - UInt64SizeOf);
    uint8_t lenEncodedU = (uint8_t)lenEncoded;
    *d++ = lenEncodedU;
    memcpy(d, bufPtr, realLen);
    return (int)realLen+1; // +1 for the length byte
}

// encodes signed integer val into a buffer d of dLen size (must be at least 9 bytes)
// returns number of bytes used
int GobVarintEncode(int64_t val, uint8_t *d, int dLen)
{
    uint64_t uVal;
    if (val < 0) {
        val = ~val;
        uVal = (uint64_t)val;
        uVal = (uVal << 1) | 1;
    } else {
        uVal = (uint64_t)val;
        uVal = uVal << 1;
    }
    return GobUVarintEncode(uVal, d, dLen);
}
