#include "BaseUtil.h"
#include "Settings.h"

#define MAGIC_ID 0x53756d53  // 'SumS' as 'Sumatra Settings'

#define SERIALIZED_HEADER_LEN 12

typedef struct {
    uint32_t   magicId;
    uint32_t   version;
    uint32_t   topLevelStructOffset;
} SerializedHeader;

STATIC_ASSERT(sizeof(SerializedHeader) == SERIALIZED_HEADER_LEN, SerializedHeader_is_12_bytes);

typedef enum : uint16_t {
    TYPE_BOOL         = 0,
    TYPE_I16          = 1,
    TYPE_U16          = 2,
    TYPE_I32          = 3,
    TYPE_U32          = 4,
    TYPE_STR          = 5,
    TYPE_STRUCT_PTR   = 6,
} Typ;

struct FieldMetadata;

typedef struct {
    uint16_t        size;
    uint16_t        nFields;
    FieldMetadata * fields;
} StructMetadata;

// information about a single field
struct FieldMetadata {
    Typ              type; // TYPE_*
    // from the beginning of the struct
    uint16_t         offset;
    // type is TYPE_STRUCT_PTR, otherwise NULL
    StructMetadata * def;
};

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
static void FreeStruct(uint8_t *data, StructMetadata *def)
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

static bool WriteStructInt(uint16_t type, int64_t val, uint8_t *p)
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

static bool WriteStructUInt(Typ type, uint64_t val, uint8_t *p)
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

    CrashIf(true);
    return false;
}

static void WriteStructPtrVal(void *val, uint8_t *p)
{
    void **pp = (void**)p;
    *pp = val;
}

static void WriteStructStr(char *val, uint8_t *p)
{
    char **pp = (char **)p;
    *pp = val;
}

static uint8_t* DeserializeRec(const uint8_t *data, int dataSize, int dataOff, StructMetadata *def)
{
    uint8_t *res = AllocArray<uint8_t>(def->size);
    FieldMetadata *fieldDef = NULL;
    uint64_t decodedUInt;
    int64_t decodedInt;
    int n;
    bool ok;
    Typ type;
    uint16_t offset;
    for (int i = 0; i < def->nFields; i++) {
        if (dataOff >= dataSize)
            goto Error;
        fieldDef = def->fields + i;
        offset = fieldDef->offset;
        type = fieldDef->type;
        if (TYPE_STR == type) {
            n = GobUVarintDecode(data + dataOff, dataSize - dataOff, &decodedUInt);
            if (0 == n)
                goto Error;
            dataOff += n;
            if (decodedUInt > 128*1024) // set a reasonable limit to avoid overflow issues
                goto Error;
            n = (int)decodedUInt;
            if (n >= dataSize - dataOff)
                goto Error;
            if (n > 0) {
                char *s = str::DupN((char*)data + dataOff, n);
                dataOff += n;
                WriteStructStr(s, res + offset);
            }
        } else if (TYPE_STRUCT_PTR == type) {
            n = GobUVarintDecode(data + dataOff, dataSize - dataOff, &decodedUInt);
            if (0 == n)
                goto Error;
            dataOff += n;
            if (decodedUInt > 512*1024*1024) // avoid overflow issues
                goto Error;
            n = (int)decodedUInt;
            uint8_t *d = DeserializeRec(data, dataSize, n, fieldDef->def);
            if (!d)
                goto Error;
            WriteStructPtrVal(d, res + offset);
        } else {
            if (IsSignedIntType(type)) {
                n = GobVarintDecode(data + dataOff, dataSize - dataOff, &decodedInt);
                if (0 == n)
                    goto Error;
                dataOff += n;
                ok = WriteStructInt(type, decodedInt, res + offset);
                if (!ok)
                    goto Error;
            } else {
                n = GobUVarintDecode(data + dataOff, dataSize - dataOff, &decodedUInt);
                if (0 == n)
                    goto Error;
                dataOff += n;
                ok = WriteStructUInt(type, decodedUInt, res + offset);
                if (!ok)
                    goto Error;
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
static uint8_t* Deserialize(const uint8_t *data, int dataSize, const char *version, StructMetadata *def)
{
    if (!data)
        return NULL;
    if (dataSize < sizeof(SerializedHeader))
        return NULL;
    SerializedHeader *hdr = (SerializedHeader*)data;
    if (hdr->magicId != MAGIC_ID)
        return NULL;
    //uint32_t ver = VersionFromStr(version);
    return DeserializeRec(data, dataSize, hdr->topLevelStructOffset, def);
}

// TODO: write me
uint8_t *Serialize(const uint8_t *data, const char *version, StructMetadata *def, int *sizeOut)
{
    *sizeOut = 0;
    return NULL;
}
