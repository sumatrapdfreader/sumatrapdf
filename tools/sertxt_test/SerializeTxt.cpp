/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "SerializeTxt.h"
#include "SerializeTxtParser.h"

namespace sertxt {

#define NL "\r\n"

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
        } else if (TYPE_ARRAY == type) {
            ListNode<void> **nodePtr = (ListNode<void>**)(data + fieldDef->offset);
            ListNode<void> *node = *nodePtr;
            ListNode<void> *next;
            while (node) {
                next = node->next;
                FreeStruct((uint8_t*)node->val, fieldDef->def);
                free(node);
                node = next;
            }
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
    TxtParser       parser;

    const char *    fieldNamesSeq;

    // last decoded value
    uint64_t        u;
    int64_t         i;
    float           f;
    char *          s;
    int             sLen;

    DecodeState() {}
};

// TODO: over-flow detection?
static bool ParseInt(char *s, char *e, int64_t *iOut)
{
    str::TrimWsEnd(s, e);
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
    str::TrimWsEnd(s, e);
    *e = 0;
    int a, r, g, b;
    if (!str::Parse(s, "#%2x%2x%2x%2x", &a, &r, &g, &b)) {
        a = 0;
        if (!str::Parse(s, "#%2x%2x%2x", &r, &g, &b)) {
            return false;
        }
    }
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
    str::TrimWsEnd(s, e);
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

static uint8_t* DeserializeRec(DecodeState& ds, TxtNode *firstNode, TxtNode *defaultFirstNode, StructMetadata *def);

static TxtNode *FindNode(TxtNode *curr, const char *name, size_t nameLen)
{
    if (!curr)
        return NULL;

    char *nodeName;
    size_t nodeNameLen;

    TxtNode *child;
    TxtNode *found = NULL;
    for (size_t i = 0; i < curr->children->Count(); i++) {
        child = curr->children->At(i);
        if (TextNode == child->type || StructNode == child->type) {
            nodeName = child->keyStart;
            nodeNameLen = child->keyEnd - nodeName;
            if (nameLen == nodeNameLen && str::EqNI(name, nodeName, nameLen))
                return child;
        }
        if (TextNode == child->type)
            continue;
        found = FindNode(child, name, nameLen);
        if (found)
            return found;
    }
    return NULL;
}

static void WriteDefaultValue(uint8_t *structDataPtr, Type type)
{
    // all other types have default value of 0, which we get for
    // free because the memory for struct is zero-allocated
    if (TYPE_FLOAT == type) {
        WriteStructFloat(structDataPtr, 0);
    }
}

static void FreeTxtNode(TxtNode *node)
{
    if (node->children) {
        for (size_t i = 0; i < node->children->Count(); i++) {
            TxtNode *child = node->children->At(i);
            CrashIf(TextNode != child->type);
            delete child;
        }
    }
    delete node->children;
    delete node;
}

static TxtNode *StructNodeFromTextNode(DecodeState& ds, TxtNode *txtNode, StructMetadata *structDef)
{
    CrashIf(TextNode != txtNode->type);
    str::Slice slice(txtNode->valStart, txtNode->valEnd);
    TxtNode *node = new TxtNode(StructNode);
    node->children = new Vec<TxtNode*>();
    uint16_t fieldNo = 0;
    TxtNode *child;
    for (;;) {
        slice.SkipWsUntilNewline();
        if (slice.Finished())
            goto Error;
        child = new TxtNode(TextNode);
        child->valStart = slice.curr;
        slice.SkipNonWs();
        child->valEnd = slice.curr;
        FieldMetadata *fieldDef = structDef->fields + fieldNo;
        char *fieldName = (char*)ds.fieldNamesSeq + fieldDef->nameOffset;
        child->keyStart = fieldName;
        child->keyEnd = fieldName + str::Len(fieldName);
        node->children->Append(child);
        ++fieldNo;
        if (fieldNo == structDef->nFields)
            break;
    }
    return node;
Error:
    FreeTxtNode(node);
    return NULL;
}

static uint8_t *DeserializeCompact(DecodeState& ds, TxtNode *node, TxtNode *defaultNode, StructMetadata *structDef)
{
    CrashIf(TextNode != node->type);
    CrashIf(defaultNode && (TextNode != defaultNode->type));
    TxtNode *structNode = StructNodeFromTextNode(ds, node, structDef);
    if (!structNode)
        structNode = StructNodeFromTextNode(ds, defaultNode, structDef);
    if (!structNode)
        return NULL;
    uint8_t *res = DeserializeRec(ds, structNode, NULL, structDef);
    FreeTxtNode(structNode);
    return res;
}

static bool DecodeField(DecodeState& ds, TxtNode *firstNode, TxtNode *defaultFirstNode, FieldMetadata *fieldDef, uint8_t *structDataStart)
{
    Type type = fieldDef->type;
    uint8_t *structDataPtr = structDataStart + fieldDef->offset;

    if ((type & TYPE_NO_STORE_MASK) != 0) {
        WriteDefaultValue(structDataPtr, type);
        return true;
    }

    bool isCompact = ((type & TYPE_STORE_COMPACT_MASK) != 0);
    type = (Type)(type & TYPE_NO_FLAGS_MASK);

    const char *fieldName = ds.fieldNamesSeq + fieldDef->nameOffset;
    size_t fieldNameLen = str::Len(fieldName);
    TxtNode *dataNode = FindNode(firstNode, fieldName, fieldNameLen);
    TxtNode *defaultNode = FindNode(defaultFirstNode, fieldName, fieldNameLen);

    // if the node doesn't exist in data, try to get it from default data
    TxtNode *node = dataNode ? dataNode : defaultNode;
    if (!node) {
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
        uint8_t *d = NULL;
        if (isCompact && (TextNode == node->type)) {
            d = DeserializeCompact(ds, node, defaultNode, fieldDef->def);
        } else {
            if (StructNode != node->type)
                return false;
            d = DeserializeRec(ds, node, defaultNode, fieldDef->def);
        }
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
    } else if (TYPE_ARRAY == type) {
        CrashIf(!fieldDef->def); // array elements must be a struct
        if (StructNode != node->type)
            return false;
        TxtNode *child;
        ListNode<void> *last = NULL;
        for (size_t i = 0; i < node->children->Count(); i++) {
            child = node->children->At(i);
            if (ArrayNode != child->type)
                return false;
            uint8_t *d = DeserializeRec(ds, child, NULL, fieldDef->def);
            if (!d)
                goto Error; // TODO: free root
            ListNode<void> *tmp = AllocArray<ListNode<void>>(1);
            tmp->val = (void*)d;
            if (!last) {
                // this is root
                last = tmp;
                // we remember it so that it gets freed in case of error
                WriteStructPtrVal(structDataPtr, (void*)last);
            } else {
                last->next = tmp;
                last = tmp;
            }
        }
    } else {
        CrashIf(true);
        return false;
    }
    return true;
Error:
    return false;
}

static uint8_t* DeserializeRec(DecodeState& ds, TxtNode *firstNode, TxtNode *defaultFirstNode, StructMetadata *def)
{
    bool ok = true;
    if (!firstNode)
        return NULL;

    uint8_t *res = AllocArray<uint8_t>(def->size);
    for (int i = 0; i < def->nFields; i++) {
        ok = DecodeField(ds, firstNode, defaultFirstNode, def->fields + i, res);
        if (!ok)
            goto Error;
    }
    return res;
Error:
    FreeStruct(res, def);
    return NULL;
}

// data and defaultData is in text format. we might modify it in place
uint8_t* DeserializeWithDefault(char *data, size_t dataSize, char *defaultData, size_t defaultDataSize, StructMetadata *def, const char *fieldNamesSeq)
{
    if (!data)
        return NULL;

    DecodeState ds;
    ds.fieldNamesSeq = fieldNamesSeq;
    ds.parser.SetToParse(data, dataSize);
    bool ok = ParseTxt(ds.parser);
    if (!ok)
        return NULL;

    TxtNode *defaultFirstNode = NULL;
    DecodeState ds2;
    ds2.fieldNamesSeq = fieldNamesSeq;
    ds2.parser.SetToParse(defaultData, defaultDataSize);
    ok = ParseTxt(ds2.parser);
    if (ok)
        defaultFirstNode =  ds2.parser.nodes.At(0);

    return DeserializeRec(ds, ds.parser.nodes.At(0), defaultFirstNode, def);
}

uint8_t* Deserialize(char *data, size_t dataSize, StructMetadata *def, const char *fieldNamesSeq)
{
    return DeserializeWithDefault(data, dataSize, NULL, 0, def, fieldNamesSeq);
}

static void AppendNest(str::Str<char>& s, int nest)
{
    while (nest > 0) {
        s.Append("  ");
        --nest;
    }
}

static void AppendVal(const char *val, char escapeChar, bool compact, str::Str<char>& res)
{
    const char *start = val;
    const char *s = start;
    char escaped = 0;
    while (*s) {
        char c = *s++;
        if (escapeChar == c)
            escaped = escapeChar;
        else if (']' == c)
            escaped = ']';
        else if ('[' == c)
            escaped = '[';
        else if ('\n' == c)
            escaped = 'n';
        else if ('\r' == c)
            escaped = 'r';
        if (0 == escaped)
            continue;

        size_t len = s - start - 1;
        res.Append(start, len);
        res.Append(escapeChar);
        res.Append(escaped);
        start = s;
        escaped = 0;
    }
    size_t len = s - start;
    res.Append(start, len);
    if (!compact)
        res.Append(NL);
}

struct EncodeState {
    str::Str<char>  res;

    char            escapeChar;
    const char *    fieldNamesSeq;

    // nesting level for the currently serialized value
    int             nest;

    // is currently serialized structure in compact form
    bool            compact;

    EncodeState() {
        escapeChar = SERIALIZE_ESCAPE_CHAR;

        fieldNamesSeq = NULL;
        nest = 0;
        compact = false;
    }
};

static void AppendKeyVal(EncodeState& es, const char *key, const char *val)
{
    if (es.compact) {
        es.res.Append(" ");
    } else {
        AppendNest(es.res, es.nest);
        es.res.Append(key);
        es.res.Append(": ");
    }
    AppendVal(val, es.escapeChar, es.compact, es.res);
}

void SerializeRec(EncodeState& es, const uint8_t *data, StructMetadata *def);

// converts "1.00" => "1" i.e. strips unnecessary trailing zeros
static void FixFloatStr(char *s)
{
    char *dot = (char*)str::FindCharLast(s, '.');
    if (!dot)
        return;
    char *end = dot;
    while (*end) {
        ++end;
    }
    --end;
    while ((end > dot) && ('0' == *end)) {
        *end = 0;
        --end;
    }
    if (end == dot)
        *end = 0;
}

static void SerializeField(EncodeState& es, FieldMetadata *fieldDef, const uint8_t *structStart)
{
    str::Str<char> val;
    str::Str<char>& res = es.res;

    Type type = fieldDef->type;
    if ((type & TYPE_NO_STORE_MASK) != 0)
        return;
    
    if (!structStart)
        return;

    bool isCompact = ((type & TYPE_STORE_COMPACT_MASK) != 0);
    type = (Type)(type & TYPE_NO_FLAGS_MASK);

    const char *fieldName = es.fieldNamesSeq + fieldDef->nameOffset;
    const uint8_t *data = structStart + fieldDef->offset;
    if (TYPE_BOOL == type) {
        bool b = ReadStructBool(data);
        AppendKeyVal(es, fieldName, b ? "true" : "false");
    } else if (TYPE_COLOR == type) {
        uint64_t u = ReadStructUInt(data, type);
        COLORREF c = (COLORREF)u;
        int r = (int)((uint8_t)(c & 0xff));
        int g = (int)((uint8_t)((c >> 8) & 0xff));
        int b = (int)((uint8_t)((c >> 16) & 0xff));
        int a = (int)((uint8_t)((c >> 24) & 0xff));
        if (a > 0)
            val.AppendFmt("#%02x%02x%02x%02x", a, r, g, b);
        else
            val.AppendFmt("#%02x%02x%02x", r, g, b);
        AppendKeyVal(es, fieldName, val.Get());
    } else if (IsUnsignedIntType(type)) {
        uint64_t u = ReadStructUInt(data, type);
        //val.AppendFmt("%" PRIu64, u);
        val.AppendFmt("%I64u", u);
        AppendKeyVal(es, fieldName, val.Get());
    } else if (IsSignedIntType(type)) {
        int64_t i = ReadStructInt(data, type);
        //val.AppendFmt("%" PRIi64, u);
        val.AppendFmt("%I64d", i);
        AppendKeyVal(es, fieldName, val.Get());
    } else if (TYPE_FLOAT == type) {
        float f = ReadStructFloat(data);
        val.AppendFmt("%f", f);
        char *floatStr = val.Get();
        FixFloatStr(floatStr);
        AppendKeyVal(es, fieldName, floatStr);
    } else if (TYPE_STR == type) {
        char *s = (char*)ReadStructPtr(data);
        if (s)
            AppendKeyVal(es, fieldName, s);
    } else if (TYPE_WSTR == type) {
        WCHAR *s = (WCHAR*)ReadStructPtr(data);
        if (s) {
            ScopedMem<char> val(str::conv::ToUtf8(s));
            AppendKeyVal(es, fieldName, val);
        }
    } else if (TYPE_STRUCT_PTR == type) {
        AppendNest(res, es.nest);
        res.Append(fieldName);
        if (isCompact)
            res.Append(":");
        else
            res.Append(" [" NL);
        const uint8_t *structStart2 = (const uint8_t *)ReadStructPtr(data);
        ++es.nest;
        // compact status only lives for one structure, so this is enough
        es.compact = isCompact;
        SerializeRec(es, structStart2, fieldDef->def);
        --es.nest;
        es.compact = false;
        if (isCompact) {
            res.Append(NL);
        } else {
            AppendNest(res, es.nest);
            res.Append("]" NL);
        }
    } else if (TYPE_ARRAY == type) {
        CrashIf(!fieldDef->def);
        AppendNest(res, es.nest);
        res.Append(fieldName);
        res.Append(" [" NL);
        ListNode<void> *el = (ListNode<void>*)ReadStructPtr(data);
        ++es.nest;
        while (el) {
            AppendNest(res, es.nest);
            res.Append("[" NL);
            const uint8_t *elData = (const uint8_t*)el->val;
            ++es.nest;
            SerializeRec(es, elData, fieldDef->def);
            --es.nest;
            AppendNest(res, es.nest);
            res.Append("]" NL);
            el = el->next;
        }
        --es.nest;
        AppendNest(res, es.nest);
        res.Append("]" NL);
    } else {
        CrashIf(true);
    }
}

void SerializeRec(EncodeState& es, const uint8_t *data, StructMetadata *def)
{
    for (size_t i = 0; i < def->nFields; i++) {
        FieldMetadata *fieldDef = &def->fields[i];
        SerializeField(es, fieldDef, data);
    }
}

uint8_t *Serialize(const uint8_t *data, StructMetadata *def, const char *fieldNamesSeq, size_t *sizeOut)
{
    EncodeState es;
    es.res.Append(UTF8_BOM "; see http://blog.kowalczyk.info/software/sumatrapdf/settings.html for documentation" NL);
    es.fieldNamesSeq = fieldNamesSeq;
    es.nest = 0;
    SerializeRec(es, data, def);
    if (sizeOut)
        *sizeOut = es.res.Size();
    return (uint8_t *)es.res.StealData();
}

} // namespace sertxt

