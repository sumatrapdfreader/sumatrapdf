/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "SerializeTxt.h"

#include "BitManip.h"
#include "SerializeTxtParser.h"

namespace sertxt {

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
    return ((TYPE_U16 == type) ||
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

static void WriteStructBool(uint8_t *p, bool val)
{
    bool *bp = (bool*)p;
    if (val)
        *bp = true;
    else
        *bp = false;
}

static bool WriteStructUInt(uint8_t *p, Typ type, uint64_t val)
{
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

class DecodeState {
public:
    // data being decoded
    TxtParser parser;

    // last decoded value
    uint64_t          u;
    int64_t           i;
    float             f;
    char *            s;
    int               sLen;

    DecodeState() {}
};

// TODO: over-flow detection?
static bool ParseInt(char *s, char *e, int64_t *iOut)
{
    int d;
    bool neg = false;
    if (s >= e)
        return false;

    if (*s = '-') {
        neg = true;
        s++;
    }
    int64_t i = 0;
    while (s < e) {
        d = *s - '0';
        if (d < 0 || d > 9)
            return false;
        i = i * 10 + d;
        ++s;
    }
    if (neg)
        i = -i;
    *iOut = i;
    return true;
}

static bool ParseBool(char *s, char *e, bool *bOut)
{
    str::TrimWsEnd(s, e);
    size_t len = e - s;
    if (4 == len && str::EqNI(s, "true", 4)) {
        *bOut = true;
        return true;
    }
    if (5 == len && str::EqNI(s, "false", 5)) {
        *bOut = false;
        return true;
    }
    int64_t i;
    if (!ParseInt(s, e, &i))
        return false;
    if (0 == i) {
        *bOut = false;
        return true;
    }
    if (1 == i) {
        *bOut = true;
        return true;
    }
    return false;
}

// TODO: over-flow detection?
static bool ParseUInt(char *s, char *e, uint64_t *iOut)
{
    int d;
    uint64_t i = 0;
    while (s < e) {
        d = *s - '0';
        if (d < 0 || d > 9)
            return false;
        i = i * 10 + d;
        ++s;
    }
    *iOut = i;
    return true;
}

static bool DecodeInt(DecodeState& ds, TxtNode *n)
{
    return ParseInt(n->valStart, n->valEnd, &ds.i);
}

static bool DecodeUInt(DecodeState& ds, TxtNode *n)
{
    return ParseUInt(n->valStart, n->valEnd, &ds.u);
}

static bool DecodeString(DecodeState& ds, TxtNode *n)
{
    ds.s = n->valStart;
    ds.sLen = n->valEnd - n->valStart;
    return true;
}

static bool DecodeFloat(DecodeState& ds, TxtNode *n)
{
    bool ok = DecodeString(ds, n);
    if (!ok)
        return false;
    char *end;
    ds.f = (float)strtod(ds.s, &end);
    return true;
}

static uint8_t* DeserializeRec(DecodeState& ds, TxtNode *firstNode, StructMetadata *def);

static TxtNode *FindTxtNode(TxtNode *curr, char *name)
{
    // TODO: optimize by passing len, which can be stored in FieldMetadata
    size_t nameLen = str::Len(name);
    char *nodeName;
    size_t nodeNameLen;
    while (curr) {
        nodeName = curr->keyStart;
        if (nodeName) {
            nodeNameLen = curr->keyEnd - nodeName;
            if (nameLen == nodeNameLen && str::EqNI(name, nodeName, nameLen))
                return curr;
        }
        // TODO: could only check curr->keyStart if decoder always puts such values in key
        // and trims key value there
        nodeName = curr->valStart;
        if (nodeName) {
            char *e = curr->valEnd;
            str::TrimWsEnd(nodeName, e);
            nodeNameLen = e - nodeName;
            if (nameLen == nodeNameLen && str::EqNI(name, nodeName, nameLen))
                return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

static void WriteDefaultValue(uint8_t *structDataPtr, Typ type)
{
    // all other types have default value of 0, which is already 
    if (TYPE_FLOAT == type) {
        WriteStructFloat(structDataPtr, 0);
    }
}

static bool DecodeField(DecodeState& ds, TxtNode *firstNode, FieldMetadata *fieldDef, uint8_t *structDataStart)
{
    Typ type = fieldDef->type;
    uint8_t *structDataPtr = structDataStart + fieldDef->offset;
    TxtNode *node = FindTxtNode(firstNode, (char*)fieldDef->name);

    if (!node) {
        // TODO: a real default value must be taken from somewhere else
        WriteDefaultValue(structDataPtr, type);
        return true;
    }
    bool ok;
    if (TYPE_BOOL == type) {
        bool bVal;
        ok = ParseBool(node->valStart, node->valEnd, &bVal);
        if (!ok)
            return false;
        WriteStructBool(structDataPtr, bVal);
    } else if (IsUnsignedIntType(type)) {
        ok = DecodeUInt(ds, node);
        if (ok)
            ok = WriteStructUInt(structDataPtr, type, ds.u);
    } else if (IsSignedIntType(type)) {
        ok = DecodeInt(ds, node);
        if (ok)
            ok = WriteStructInt(structDataPtr, type, ds.i);
    } else if (TYPE_STRUCT_PTR == type) {
        if (!node->child)
            return false;
        uint8_t *d = DeserializeRec(ds, node->child, fieldDef->def);
        if (!d)
            goto Error;
        WriteStructPtrVal(structDataPtr, d);
    } else if (TYPE_STR == type) {
        ok = DecodeString(ds, node);
        if (ok && (ds.sLen > 0)) {
            char *s = str::DupN(ds.s, ds.sLen);
            WriteStructStr(structDataPtr, s);
        }
    } else if (TYPE_WSTR == type) {
        ok = DecodeString(ds, node);
        if (ok && (ds.sLen > 0)) {
            WCHAR *ws = str::conv::FromUtf8(ds.s);
            WriteStructWStr(structDataPtr, ws);
        }
    }  else if (TYPE_FLOAT == type) {
        ok = DecodeFloat(ds, node);
        if (ok)
            WriteStructFloat(structDataPtr, ds.f);
    } else {
        CrashIf(true);
        return false;
    }
    return true;
Error:
    return false;
}

// TODO: do parallel decoding from default data and data from the client
// if no data from client - return the result from default data
// if data from client doesn't have enough fields, use fields from default data
// if data from client is corrupted, decode default data
static uint8_t* DeserializeRec(DecodeState& ds, TxtNode *firstNode, StructMetadata *def)
{
    bool ok = true;
    if (!firstNode)
        return NULL;

    uint8_t *res = AllocArray<uint8_t>(def->size);
    for (int i = 0; i < def->nFields; i++) {
        ok = DecodeField(ds, firstNode, def->fields + i, res);
        if (!ok)
            goto Error;
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
    //uint32_t ver = VersionFromStr(version);
    DecodeState ds;
    ds.parser.toParse.Init((char*)data, (size_t)dataSize);
    bool ok = ParseTxt(ds.parser);
    if (!ok)
        return NULL;
    return DeserializeRec(ds, ds.parser.firstNode, def);
}

// TODO: write me
uint8_t *Serialize(const uint8_t *data, const char *version, StructMetadata *def, int *sizeOut)
{
    *sizeOut = 0;
    return NULL;
}

} // namespace sertxt

