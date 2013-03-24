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
    Type type;
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

static bool IsSignedIntType(Type type)
{
    return ((TYPE_I16 == type) ||
            (TYPE_I32 == type));
}

static bool IsUnsignedIntType(Type type)
{
    return ((TYPE_U16 == type) ||
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

static void WriteStructBool(uint8_t *p, bool val)
{
    bool *bp = (bool*)p;
    if (val)
        *bp = true;
    else
        *bp = false;
}

static bool WriteStructUInt(uint8_t *p, Type type, uint64_t val)
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

static bool ReadStructBool(const uint8_t *p)
{
    bool *bp = (bool*)p;
    return *bp;
}

static int64_t ReadStructInt(const uint8_t *p, Type type)
{
    if (TYPE_I16 == type) {
        int16_t *vp = (int16_t*)p;
        return (int64_t)*vp;
    }
    if (TYPE_I32 == type) {
        int32_t *vp = (int32_t*)p;
        return (int64_t)*vp;
    }
    CrashIf(true);
    return 0;
}

static uint64_t ReadStructUInt(const uint8_t *p, Type type)
{
    if (TYPE_U16 == type) {
        uint16_t *vp = (uint16_t*)p;
        return (uint64_t)*vp;
    }
    if ((TYPE_U32 == type) || (TYPE_COLOR == type)) {
        uint32_t *vp = (uint32_t*)p;
        return (uint64_t)*vp;
    }
    if (TYPE_U64 == type) {
        uint64_t *vp = (uint64_t*)p;
        return *vp;
    }
    CrashIf(true);
    return 0;
}

static float ReadStructFloat(const uint8_t *p)
{
    float *fp = (float*)p;
    return *fp;
}

static void *ReadStructPtr(const uint8_t *p)
{
    void **pp = (void**)p;
    return *pp;
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

    if ('-' == *s) {
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

static bool ParseColor(char *s, char *e, COLORREF *colOut)
{
    int a, r, g, b;
    if (!str::Parse(s, "#%2x%2x%2x%2x", &a, &r, &g, &b))
        return false;
    COLORREF col =  RGB(r, g, b);
    COLORREF alpha = (COLORREF)a;
    alpha = alpha << 24;
    col = col | alpha;
    *colOut = col;
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

static void WriteDefaultValue(uint8_t *structDataPtr, Type type)
{
    // all other types have default value of 0, which is already 
    if (TYPE_FLOAT == type) {
        WriteStructFloat(structDataPtr, 0);
    }
}

static bool DecodeField(DecodeState& ds, TxtNode *firstNode, FieldMetadata *fieldDef, uint8_t *structDataStart)
{
    Type type = fieldDef->type;
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
    } else if (TYPE_COLOR == type) {
        COLORREF val;
        ok = ParseColor(node->valStart, node->valEnd, &val);
        if (ok)
            WriteStructUInt(structDataPtr, TYPE_U32, val);
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

static void AppendNest(str::Str<char>& s, int nest)
{
    while (nest > 0) {
        s.Append("  ");
        --nest;
    }
}

// TODO: if val contains newline, we must escape it. We also need to un-escape it.
static void AppendKeyVal(const char *key, const char *val, int nest, str::Str<char>& res)
{
    AppendNest(res, nest);
    res.Append(key);
    res.Append(": ");
    res.Append(val);
    res.Append("\n");
}

void SerializeRec(const uint8_t *data, StructMetadata *def, int nest, str::Str<char>& res);

// converts "1.00" => "1" i.e. strips unnecessary trailing zeros
static void FixFloatStr(char *s)
{
    char *dot = (char*)str::FindCharLast(s, '.');
    if (!dot)
        return;
    char *tmp = dot + 1;
    while (*tmp) {
        if (*tmp != '0')
            return;
        ++tmp;
    }
    *dot = 0;
}

#define HEX_CHARS "0123456789abcdef"

static void AppendHex(uint8_t b, str::Str<char>& res)
{
    char c;
    uint8_t n = (b & 0xf0) >> 4;
    c = HEX_CHARS[n];
    res.Append(c);
    n = b & 0xf;
    c = HEX_CHARS[n];
    res.Append(HEX_CHARS[n]);
}

static void SerializeField(FieldMetadata *fieldDef, const uint8_t *structStart, int nest, str::Str<char>& res)
{
    str::Str<char> val;
    Type type = fieldDef->type;
    const uint8_t *data = structStart + fieldDef->offset;
    if (TYPE_BOOL == type) {
        bool b = ReadStructBool(data);
        AppendKeyVal(fieldDef->name, b ? "true" : "false", nest, res);
    } else if (TYPE_COLOR == type) {
        uint64_t u = ReadStructUInt(data, type);
        COLORREF c = (COLORREF)u;
        uint8_t r = (uint8_t)(c & 0xff);
        uint8_t g = (uint8_t)((c >> 8) & 0xff);
        uint8_t b = (uint8_t)((c >> 16) & 0xff);
        uint8_t a = (uint8_t)((c >> 24) & 0xff);
        val.Append("#");
        AppendHex(a, val);
        AppendHex(r, val);
        AppendHex(g, val);
        AppendHex(b, val);
        AppendKeyVal(fieldDef->name, val.Get(), nest, res);
    } else if (IsUnsignedIntType(type)) {
        uint64_t u = ReadStructUInt(data, type);
        //val.AppendFmt("%" PRIu64, u);
        val.AppendFmt("%I64u", u);
        AppendKeyVal(fieldDef->name, val.Get(), nest, res);
    } else if (IsSignedIntType(type)) {
        int64_t i = ReadStructInt(data, type);
        //val.AppendFmt("%" PRIi64, u);
        val.AppendFmt("%I64d", i);
        AppendKeyVal(fieldDef->name, val.Get(), nest, res);
    } else if (TYPE_FLOAT == type) {
        float f = ReadStructFloat(data);
        val.AppendFmt("%f", f);
        char *floatStr = val.Get();
        FixFloatStr(floatStr);
        AppendKeyVal(fieldDef->name, floatStr, nest, res);
    } else if (TYPE_STR == type) {
        char *s = (char*)ReadStructPtr(data);
        if (s)
            AppendKeyVal(fieldDef->name, s, nest, res);
    } else if (TYPE_WSTR == type) {
        WCHAR *s = (WCHAR*)ReadStructPtr(data);
        if (s) {
            ScopedMem<char> val(str::conv::ToUtf8(s));
            AppendKeyVal(fieldDef->name, val, nest, res);
        }
    } else if (TYPE_STRUCT_PTR == type) {
        AppendNest(res, nest);
        res.Append(fieldDef->name);
        res.Append(" [\n");
        const uint8_t *structStart2 = (const uint8_t *)ReadStructPtr(data);
        SerializeRec(structStart2, fieldDef->def, nest + 1, res);
        AppendNest(res, nest);
        res.Append("]\n");
    } else {
        CrashIf(true);
    }
}

void SerializeRec(const uint8_t *data, StructMetadata *def, int nest, str::Str<char>& res)
{
    for (size_t i = 0; i < def->nFields; i++) {
        FieldMetadata *field = &def->fields[i];
        SerializeField(field, data, nest, res);
    }
}

uint8_t *Serialize(const uint8_t *data, const char *version, StructMetadata *def, int *sizeOut)
{
    str::Str<char> res;
    SerializeRec(data, def, 0, res);
    if (sizeOut)
        *sizeOut = (int)res.Size();
    return (uint8_t *)res.StealData();
}

} // namespace sertxt

