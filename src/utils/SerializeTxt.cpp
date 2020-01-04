/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "StrSlice.h"
#include "SerializeTxt.h"
#include "TxtParser.h"

namespace sertxt {

#define NL "\r\n"

static const StructMetadata* GetStructDef(const FieldMetadata* fieldDef) {
    CrashIf(0 == fieldDef->defValOrDefinition);
    return (const StructMetadata*)fieldDef->defValOrDefinition;
}

// the assumption here is that the data was either built by Deserialize()
// or was created by application code in a way that observes our rule: each
// struct and string was separately allocated with malloc()
void FreeStruct(uint8_t* structStart, const StructMetadata* def) {
    if (!structStart)
        return;
    Type type;
    const FieldMetadata* fieldDef = nullptr;
    for (int i = 0; i < def->nFields; i++) {
        fieldDef = def->fields + i;
        uint8_t* data = structStart + fieldDef->offset;
        type = (Type)(fieldDef->type & TYPE_MASK);
        if (TYPE_STRUCT_PTR == type) {
            uint8_t** p = (uint8_t**)data;
            FreeStruct(*p, GetStructDef(fieldDef));
            *p = nullptr;
        } else if (TYPE_ARRAY == type) {
            Vec<uint8_t*>** vecPtr = (Vec<uint8_t*>**)data;
            Vec<uint8_t*>* vec = *vecPtr;
            CrashIf(!vec);
            for (size_t j = 0; j < vec->size(); j++) {
                FreeStruct(vec->at(j), GetStructDef(fieldDef));
            }
            delete vec;
            *vecPtr = nullptr;
        } else if ((TYPE_STR == type) || (TYPE_WSTR == type)) {
            char** sp = (char**)data;
            char* s = *sp;
            free(s);
            *sp = nullptr;
        }
    }
    free(structStart);
}

static bool IsSignedIntType(Type type) {
    return ((TYPE_I16 == type) || (TYPE_I32 == type));
}

static bool IsUnsignedIntType(Type type) {
    return ((TYPE_U16 == type) || (TYPE_U32 == type) || (TYPE_U64 == type));
}

static bool WriteStructInt(uint8_t* p, Type type, int64_t val) {
    if (TYPE_I16 == type) {
        if (val > 0xffff)
            return false;
        int16_t v = (int16_t)val;
        int16_t* vp = (int16_t*)p;
        *vp = v;
        return true;
    }

    if (TYPE_I32 == type) {
        if (val > 0xffffffff)
            return false;
        int32_t v = (int32_t)val;
        int32_t* vp = (int32_t*)p;
        *vp = v;
        return true;
    }
    CrashIf(true);
    return false;
}

static void WriteStructBool(uint8_t* p, bool val) {
    bool* bp = (bool*)p;
    if (val)
        *bp = true;
    else
        *bp = false;
}

static bool WriteStructUInt(uint8_t* p, Type type, uint64_t val) {
    if (TYPE_U16 == type) {
        if (val > 0xffff)
            return false;
        uint16_t v = (uint16_t)val;
        uint16_t* vp = (uint16_t*)p;
        *vp = v;
        return true;
    }

    if ((TYPE_U32 == type) || (TYPE_COLOR == type)) {
        if (val > 0xffffffff)
            return false;
        uint32_t v = (uint32_t)val;
        uint32_t* vp = (uint32_t*)p;
        *vp = v;
        return true;
    }

    if (TYPE_U64 == type) {
        uint64_t* vp = (uint64_t*)p;
        *vp = val;
        return true;
    }

    CrashIf(true);
    return false;
}

static void WriteStructPtrVal(uint8_t* p, void* val) {
    void** pp = (void**)p;
    *pp = val;
}

static void WriteStructStr(uint8_t* p, char* s) {
    char** sp = (char**)p;
    *sp = s;
}

static void WriteStructWStr(uint8_t* p, WCHAR* s) {
    WCHAR** sp = (WCHAR**)p;
    *sp = s;
}

static void WriteStructFloat(uint8_t* p, float f) {
    float* fp = (float*)p;
    *fp = f;
}

static bool ReadStructBool(const uint8_t* p) {
    bool* bp = (bool*)p;
    return *bp;
}

static int64_t ReadStructInt(const uint8_t* p, Type type) {
    if (TYPE_I16 == type) {
        int16_t* vp = (int16_t*)p;
        return (int64_t)*vp;
    }
    if (TYPE_I32 == type) {
        int32_t* vp = (int32_t*)p;
        return (int64_t)*vp;
    }
    CrashIf(true);
    return 0;
}

static uint64_t ReadStructUInt(const uint8_t* p, Type type) {
    if (TYPE_U16 == type) {
        uint16_t* vp = (uint16_t*)p;
        return (uint64_t)*vp;
    }
    if ((TYPE_U32 == type) || (TYPE_COLOR == type)) {
        uint32_t* vp = (uint32_t*)p;
        return (uint64_t)*vp;
    }
    if (TYPE_U64 == type) {
        uint64_t* vp = (uint64_t*)p;
        return *vp;
    }
    CrashIf(true);
    return 0;
}

static float ReadStructFloat(const uint8_t* p) {
    float* fp = (float*)p;
    return *fp;
}

static void* ReadStructPtr(const uint8_t* p) {
    void** pp = (void**)p;
    return *pp;
}

class DecodeState {
  public:
    // data being decoded
    TxtParser parser;
    DecodeState() {
    }
};

static bool ParseUInt(char* s, char* e, uint64_t* nOut) {
    str::TrimWsEnd(s, e);
    int d;
    uint64_t n = 0;
    uint64_t prev = 0;
    while (s < e) {
        d = *s - '0';
        if (d < 0 || d > 9)
            return false;
        n = n * 10 + d;
        if (n < prev) {
            // on overflow return 0
            *nOut = 0;
            return true;
        }
        prev = n;
        ++s;
    }
    *nOut = n;
    return true;
}

static bool ParseInt(char* s, char* e, int64_t* iOut) {
    if (s >= e)
        return false;

    bool neg = false;
    if ('-' == *s) {
        neg = true;
        s++;
    }
    uint64_t u;
    if (!ParseUInt(s, e, &u))
        return false;
#if 0 // TODO:: why is this missing?
    if (u > MAXLONG64)
        return false;
#endif
    int64_t i = (int64_t)u;
    if (neg)
        i = -i;
    *iOut = i;
    return true;
}

static bool ParseColor(char* s, char* e, COLORREF* colOut) {
    str::TrimWsEnd(s, e);
    *e = 0;
    int a, r, g, b;
    if (!str::Parse(s, "#%2x%2x%2x%2x", &a, &r, &g, &b)) {
        a = 0;
        if (!str::Parse(s, "#%2x%2x%2x", &r, &g, &b)) {
            return false;
        }
    }
    COLORREF col = RGB(r, g, b);
    COLORREF alpha = (COLORREF)a;
    alpha = alpha << 24;
    col = col | alpha;
    *colOut = col;
    return true;
}

static bool ParseBool(char* s, char* e, bool* bOut) {
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

static bool ParseFloat(char* s, char* e, float* f) {
    char* end = e;
    *f = (float)strtod(s, &end);
    return true;
}

static uint8_t* DeserializeRec(DecodeState& ds, TxtNode* firstNode, const StructMetadata* def);

static TxtNode* FindNode(TxtNode* curr, const char* name, size_t nameLen) {
    if (!curr)
        return nullptr;

    char* nodeName;
    size_t nodeNameLen;

    TxtNode* found = nullptr;
    for (TxtNode* child = curr->firstChild; child != nullptr; child = child->sibling) {
        if (child->IsText() || child->IsStruct()) {
            nodeName = child->keyStart;
            nodeNameLen = child->keyEnd - nodeName;
            if (nameLen == nodeNameLen && str::EqNI(name, nodeName, nameLen))
                return child;
        }
        if (child->IsText()) {
            continue;
        }
        found = FindNode(child, name, nameLen);
        if (found) {
            return found;
        }
    }
    return nullptr;
}

static void WriteDefaultValue(uint8_t* structDataPtr, Type type) {
    // all other types have default value of 0, which we get for
    // free because the memory for struct is zero-allocated
    if (TYPE_FLOAT == type) {
        WriteStructFloat(structDataPtr, 0);
    }
}

static TxtNode* StructNodeFromTextNode(DecodeState& ds, TxtNode* txtNode, const StructMetadata* structDef) {
    CrashIf(!txtNode->IsText());
    str::Slice slice(txtNode->valStart, txtNode->valEnd);
    TxtNode* node = ds.parser.AllocTxtNode(TxtNode::Type::Struct);
    uint16_t fieldNo = 0;
    char* fieldName = (char*)structDef->fieldNames;
    TxtNode* child;
    for (;;) {
        slice.SkipWsUntilNewline();
        if (slice.Finished())
            goto Error;
        child = ds.parser.AllocTxtNode(TxtNode::Type::Text);
        child->valStart = slice.curr;
        slice.SkipNonWs();
        child->valEnd = slice.curr;
        child->keyStart = fieldName;
        child->keyEnd = fieldName + str::Len(fieldName);
        node->AddChild(child);
        ++fieldNo;
        if (fieldNo == structDef->nFields)
            break;
        seqstrings::SkipStr(fieldName);
    }
    return node;
Error:
    return nullptr;
}

static uint8_t* DeserializeCompact(DecodeState& ds, TxtNode* node, const StructMetadata* structDef) {
    CrashIf(!node->IsText());
    TxtNode* structNode = StructNodeFromTextNode(ds, node, structDef);
    if (!structNode) {
        return nullptr;
    }
    uint8_t* res = DeserializeRec(ds, structNode, structDef);
    return res;
}

static uint8_t* DecodeStruct(DecodeState& ds, const FieldMetadata* fieldDef, TxtNode* node, bool isCompact) {
    uint8_t* d = nullptr;
    if (isCompact && node->IsText()) {
        d = DeserializeCompact(ds, node, GetStructDef(fieldDef));
    } else {
        if (node->IsStruct())
            d = DeserializeRec(ds, node, GetStructDef(fieldDef));
    }
    return d;
}

static bool DecodeField(DecodeState& ds, TxtNode* firstNode, const char* fieldName, const FieldMetadata* fieldDef,
                        uint8_t* structDataStart) {
    Type type = fieldDef->type;
    uint8_t* structDataPtr = structDataStart + fieldDef->offset;

    if ((type & TYPE_NO_STORE_MASK) != 0) {
        WriteDefaultValue(structDataPtr, type);
        return true;
    }

    bool isCompact = ((type & TYPE_STORE_COMPACT_MASK) != 0);
    type = (Type)(type & TYPE_MASK);

    size_t fieldNameLen = str::Len(fieldName);

    // if the node doesn't exist in data, try to get it from default data
    TxtNode* node = FindNode(firstNode, fieldName, fieldNameLen);
    if (!node) {
        WriteDefaultValue(structDataPtr, type);
        return true;
    }
    bool ok;

    if (TYPE_BOOL == type) {
        bool bVal;
        ok = ParseBool(node->valStart, node->valEnd, &bVal);
        if (ok) {
            WriteStructBool(structDataPtr, bVal);
        }
        return ok;
    }

    // TODO: should we return ok instead of true in cases below?
    if (TYPE_COLOR == type) {
        COLORREF val;
        ok = ParseColor(node->valStart, node->valEnd, &val);
        if (ok) {
            WriteStructUInt(structDataPtr, TYPE_COLOR, val);
        }
        return true;
    }

    if (IsUnsignedIntType(type)) {
        uint64_t n;
        ok = ParseUInt(node->valStart, node->valEnd, &n);
        if (ok) {
            ok = WriteStructUInt(structDataPtr, type, n);
        }
        return true;
    }

    if (IsSignedIntType(type)) {
        int64_t n;
        ok = ParseInt(node->valStart, node->valEnd, &n);
        if (ok) {
            ok = WriteStructInt(structDataPtr, type, n);
        }
        return true;
    }

    if (TYPE_STRUCT_PTR == type) {
        uint8_t* d = DecodeStruct(ds, fieldDef, node, isCompact);
        if (d) {
            WriteStructPtrVal(structDataPtr, d);
        }
        return true;
    }

    if (TYPE_STR == type) {
        char* s = node->valStart;
        size_t sLen = node->valEnd - s;
        if (s && (sLen > 0)) {
            // note: we don't free s because it's remembered in structDataPtr
            s = str::DupN(s, sLen);
            WriteStructStr(structDataPtr, s);
        }
        return true;
    }

    if (TYPE_WSTR == type) {
        char* s = node->valStart;
        size_t sLen = node->valEnd - s;
        if (s && (sLen > 0)) {
            // note: we don't free ws because it's remembered in structDataPtr
            WCHAR* ws = strconv::Utf8ToWstr(s);
            WriteStructWStr(structDataPtr, ws);
        }
        return true;
    }

    if (TYPE_FLOAT == type) {
        float f;
        ok = ParseFloat(node->valStart, node->valEnd, &f);
        if (ok)
            WriteStructFloat(structDataPtr, f);
    }

    if (TYPE_ARRAY == type) {
        if (!node->IsStruct()) {
            return false;
        }
        Vec<uint8_t*>* vec = new Vec<uint8_t*>();
        // we remember it right away, so that it gets freed in case of error
        WriteStructPtrVal(structDataPtr, (void*)vec);
        for (TxtNode* child = node->firstChild; child != nullptr; child = child->sibling) {
            uint8_t* d = DecodeStruct(ds, fieldDef, child, isCompact);
            if (d) {
                vec->Append(d);
            }
        }
        return true;
    }

    CrashIf(true);
    return false;
}

static uint8_t* DeserializeRec(DecodeState& ds, TxtNode* firstNode, const StructMetadata* def) {
    bool ok = true;
    if (!firstNode)
        return nullptr;

    uint8_t* res = AllocArray<uint8_t>(def->size);
    const StructMetadata** defPtr = (const StructMetadata**)res;
    *defPtr = def;
    const char* fieldName = def->fieldNames;
    for (int i = 0; i < def->nFields; i++) {
        ok = DecodeField(ds, firstNode, fieldName, def->fields + i, res);
        if (!ok)
            goto Error;
        seqstrings::SkipStr(fieldName);
    }
    return res;
Error:
    FreeStruct(res, def);
    return nullptr;
}

uint8_t* Deserialize(struct TxtNode* root, const StructMetadata* def) {
    DecodeState ds;
    return DeserializeRec(ds, root, def);
}

uint8_t* Deserialize(const std::string_view str, const StructMetadata* def) {
    if (!str.data()) {
        return nullptr;
    }

    DecodeState ds;
    ds.parser.SetToParse(str);
    bool ok = ParseTxt(ds.parser);
    if (!ok) {
        return nullptr;
    }

    return DeserializeRec(ds, ds.parser.nodes.at(0), def);
}

static void AppendNest(str::Str& s, int nest) {
    while (nest > 0) {
        s.Append("  ");
        --nest;
    }
}

static void AppendVal(const char* val, char escapeChar, bool compact, str::Str& res) {
    const char* start = val;
    const char* s = start;
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
        res.AppendChar(escapeChar);
        res.AppendChar(escaped);
        start = s;
        escaped = 0;
    }
    size_t len = s - start;
    res.Append(start, len);
    if (!compact) {
        res.Append(NL);
    }
}

struct EncodeState {
    str::Str res;

    char escapeChar;

    // nesting level for the currently serialized value
    int nest;

    // is currently serialized structure in compact form
    bool compact;

    EncodeState() {
        escapeChar = SERIALIZE_ESCAPE_CHAR;
        nest = 0;
        compact = false;
    }
};

static void AppendKeyVal(EncodeState& es, const char* key, const char* val) {
    if (es.compact) {
        es.res.Append(" ");
    } else {
        AppendNest(es.res, es.nest);
        es.res.Append(key);
        es.res.Append(": ");
    }
    AppendVal(val, es.escapeChar, es.compact, es.res);
}

void SerializeRec(EncodeState& es, const uint8_t* structStart, const StructMetadata* def);

static void SerializeField(EncodeState& es, const char* fieldName, const FieldMetadata* fieldDef,
                           const uint8_t* structStart) {
    str::Str val;
    str::Str& res = es.res;

    Type type = fieldDef->type;
    if ((type & TYPE_NO_STORE_MASK) != 0)
        return;

    if (!structStart)
        return;

    bool isCompact = ((type & TYPE_STORE_COMPACT_MASK) != 0);
    type = (Type)(type & TYPE_MASK);

    const uint8_t* data = structStart + fieldDef->offset;
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
        // val.AppendFmt("%" PRIu64, u);
        val.AppendFmt("%I64u", u);
        AppendKeyVal(es, fieldName, val.Get());
    } else if (IsSignedIntType(type)) {
        int64_t i = ReadStructInt(data, type);
        // val.AppendFmt("%" PRIi64, u);
        val.AppendFmt("%I64d", i);
        AppendKeyVal(es, fieldName, val.Get());
    } else if (TYPE_FLOAT == type) {
        float f = ReadStructFloat(data);
        val.AppendFmt("%g", f);
        AppendKeyVal(es, fieldName, val.Get());
    } else if (TYPE_STR == type) {
        char* s = (char*)ReadStructPtr(data);
        if (s)
            AppendKeyVal(es, fieldName, s);
    } else if (TYPE_WSTR == type) {
        WCHAR* s = (WCHAR*)ReadStructPtr(data);
        if (s) {
            AutoFree val2(strconv::WstrToUtf8(s));
            AppendKeyVal(es, fieldName, val2.Get());
        }
    } else if (TYPE_STRUCT_PTR == type) {
        AppendNest(res, es.nest);
        res.Append(fieldName);
        if (isCompact)
            res.Append(":");
        else
            res.Append(" [" NL);
        const uint8_t* structStart2 = (const uint8_t*)ReadStructPtr(data);
        ++es.nest;
        // compact status only lives for one structure, so this is enough
        es.compact = isCompact;
        SerializeRec(es, structStart2, GetStructDef(fieldDef));
        --es.nest;
        es.compact = false;
        if (isCompact) {
            res.Append(NL);
        } else {
            AppendNest(res, es.nest);
            res.Append("]" NL);
        }
    } else if (TYPE_ARRAY == type) {
        AppendNest(res, es.nest);
        res.Append(fieldName);
        res.Append(" [" NL);
        Vec<const uint8_t*>* vec = (Vec<const uint8_t*>*)ReadStructPtr(data);
        ++es.nest;
        for (size_t i = 0; vec && (i < vec->size()); i++) {
            AppendNest(res, es.nest);
            res.Append("[" NL);
            const uint8_t* elData = vec->at(i);
            ++es.nest;
            SerializeRec(es, elData, GetStructDef(fieldDef));
            --es.nest;
            AppendNest(res, es.nest);
            res.Append("]" NL);
        }
        --es.nest;
        AppendNest(res, es.nest);
        res.Append("]" NL);
    } else {
        CrashIf(true);
    }
}

void SerializeRec(EncodeState& es, const uint8_t* structStart, const StructMetadata* def) {
    if (!structStart)
        return;
    const char* fieldName = def->fieldNames;
    for (size_t i = 0; i < def->nFields; i++) {
        const FieldMetadata* fieldDef = &def->fields[i];
        SerializeField(es, fieldName, fieldDef, structStart);
        seqstrings::SkipStr(fieldName);
    }
}

std::string_view Serialize(const uint8_t* rootStruct, const StructMetadata* def) {
    EncodeState es;
    es.res.Append(UTF8_BOM "; see https://www.sumatrapdfreader.org/settings.html for documentation" NL);
    es.nest = 0;
    SerializeRec(es, rootStruct, def);
    size_t size = es.res.size();
    return {es.res.StealData(), size};
}

} // namespace sertxt
