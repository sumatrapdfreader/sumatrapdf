/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
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
void FreeStruct(u8* structStart, const StructMetadata* def) {
    if (!structStart) {
        return;
    }
    Type type;
    const FieldMetadata* fieldDef = nullptr;
    for (int i = 0; i < def->nFields; i++) {
        fieldDef = def->fields + i;
        u8* data = structStart + fieldDef->offset;
        type = (Type)(fieldDef->type & TYPE_MASK);
        if (TYPE_STRUCT_PTR == type) {
            u8** p = (u8**)data;
            FreeStruct(*p, GetStructDef(fieldDef));
            *p = nullptr;
        } else if (TYPE_ARRAY == type) {
            Vec<u8*>** vecPtr = (Vec<u8*>**)data;
            Vec<u8*>* vec = *vecPtr;
            CrashIf(!vec);
            for (size_t j = 0; vec && j < vec->size(); j++) {
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

static void WriteStructInt(u8* p, Type type, i64 val) {
    if (TYPE_I16 == type) {
        if (val > 0xffff) {
            CrashIf(true);
            return;
        }
        i16 v = (i16)val;
        i16* vp = (i16*)p;
        *vp = v;
        return;
    }

    if (TYPE_I32 == type) {
        if (val > 0xffffffff) {
            CrashIf(true);
            return;
        }
        i32 v = (i32)val;
        i32* vp = (i32*)p;
        *vp = v;
        return;
    }
    CrashIf(true);
}

static void WriteStructBool(u8* p, bool val) {
    bool* bp = (bool*)p;
    if (val) {
        *bp = true;
    } else {
        *bp = false;
    }
}

static void WriteStructUInt(u8* p, Type type, u64 val) {
    if (TYPE_U16 == type) {
        if (val > 0xffff) {
            CrashIf(true);
            return;
        }
        u16 v = (u16)val;
        u16* vp = (u16*)p;
        *vp = v;
        return;
    }

    if ((TYPE_U32 == type) || (TYPE_COLOR == type)) {
        if (val > 0xffffffff) {
            CrashIf(true);
            return;
        }
        u32 v = (u32)val;
        u32* vp = (u32*)p;
        *vp = v;
        return;
    }

    if (TYPE_U64 == type) {
        u64* vp = (u64*)p;
        *vp = val;
        return;
    }

    CrashIf(true);
}

static void WriteStructPtrVal(u8* p, void* val) {
    void** pp = (void**)p;
    *pp = val;
}

static void WriteStructStr(u8* p, char* s) {
    char** sp = (char**)p;
    *sp = s;
}

static void WriteStructWStr(u8* p, WCHAR* s) {
    WCHAR** sp = (WCHAR**)p;
    *sp = s;
}

static void WriteStructFloat(u8* p, float f) {
    float* fp = (float*)p;
    *fp = f;
}

static bool ReadStructBool(const u8* p) {
    bool* bp = (bool*)p;
    return *bp;
}

static i64 ReadStructInt(const u8* p, Type type) {
    if (TYPE_I16 == type) {
        i16* vp = (i16*)p;
        return (i64)*vp;
    }
    if (TYPE_I32 == type) {
        i32* vp = (i32*)p;
        return (i64)*vp;
    }
    CrashIf(true);
    return 0;
}

static u64 ReadStructUInt(const u8* p, Type type) {
    if (TYPE_U16 == type) {
        u16* vp = (u16*)p;
        return (u64)*vp;
    }
    if ((TYPE_U32 == type) || (TYPE_COLOR == type)) {
        u32* vp = (u32*)p;
        return (u64)*vp;
    }
    if (TYPE_U64 == type) {
        u64* vp = (u64*)p;
        return *vp;
    }
    CrashIf(true);
    return 0;
}

static float ReadStructFloat(const u8* p) {
    float* fp = (float*)p;
    return *fp;
}

static void* ReadStructPtr(const u8* p) {
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

static bool ParseUInt(char* s, char* e, u64* nOut) {
    str::TrimWsEnd(s, e);
    int d;
    u64 n = 0;
    u64 prev = 0;
    while (s < e) {
        d = *s - '0';
        if (d < 0 || d > 9) {
            return false;
        }
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

static bool ParseInt(char* s, char* e, i64* iOut) {
    if (s >= e) {
        return false;
    }

    bool neg = false;
    if ('-' == *s) {
        neg = true;
        s++;
    }
    u64 u;
    if (!ParseUInt(s, e, &u)) {
        return false;
    }
    if (u > MAXLONG64) {
        return false;
    }
    i64 i = (i64)u;
    if (neg) {
        i = -i;
    }
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
    i64 i;
    if (!ParseInt(s, e, &i)) {
        return false;
    }
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

static u8* DeserializeRec(DecodeState& ds, TxtNode* firstNode, const StructMetadata* def);

static TxtNode* FindNode(TxtNode* curr, const char* name, size_t nameLen) {
    if (!curr) {
        return nullptr;
    }

    char* nodeName;
    size_t nodeNameLen;

    TxtNode* found = nullptr;
    for (TxtNode* child = curr->firstChild; child != nullptr; child = child->sibling) {
        if (child->IsText() || child->IsStruct()) {
            nodeName = child->keyStart;
            nodeNameLen = child->keyEnd - nodeName;
            if (nameLen == nodeNameLen && str::EqNI(name, nodeName, nameLen)) {
                return child;
            }
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

static void WriteDefaultValue(u8* structDataPtr, Type type) {
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
    u16 fieldNo = 0;
    char* fieldName = (char*)structDef->fieldNames;
    TxtNode* child;
    for (;;) {
        slice.SkipWsUntilNewline();
        if (slice.Finished()) {
            goto Error;
        }
        child = ds.parser.AllocTxtNode(TxtNode::Type::Text);
        child->valStart = slice.curr;
        slice.SkipNonWs();
        child->valEnd = slice.curr;
        child->keyStart = fieldName;
        child->keyEnd = fieldName + str::Len(fieldName);
        node->AddChild(child);
        ++fieldNo;
        if (fieldNo == structDef->nFields) {
            break;
        }
        fieldName = seqstrings::SkipStr(fieldName);
    }
    return node;
Error:
    return nullptr;
}

static u8* DeserializeCompact(DecodeState& ds, TxtNode* node, const StructMetadata* structDef) {
    CrashIf(!node->IsText());
    TxtNode* structNode = StructNodeFromTextNode(ds, node, structDef);
    if (!structNode) {
        return nullptr;
    }
    u8* res = DeserializeRec(ds, structNode, structDef);
    return res;
}

static u8* DecodeStruct(DecodeState& ds, const FieldMetadata* fieldDef, TxtNode* node, bool isCompact) {
    u8* d = nullptr;
    if (isCompact && node->IsText()) {
        d = DeserializeCompact(ds, node, GetStructDef(fieldDef));
    } else {
        if (node->IsStruct()) {
            d = DeserializeRec(ds, node, GetStructDef(fieldDef));
        }
    }
    return d;
}

static bool DecodeField(DecodeState& ds, TxtNode* firstNode, const char* fieldName, const FieldMetadata* fieldDef,
                        u8* structDataStart) {
    Type type = fieldDef->type;
    u8* structDataPtr = structDataStart + fieldDef->offset;

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
        u64 n;
        ok = ParseUInt(node->valStart, node->valEnd, &n);
        if (ok) {
            WriteStructUInt(structDataPtr, type, n);
        }
        return true;
    }

    if (IsSignedIntType(type)) {
        i64 n;
        ok = ParseInt(node->valStart, node->valEnd, &n);
        if (ok) {
            WriteStructInt(structDataPtr, type, n);
        }
        return true;
    }

    if (TYPE_STRUCT_PTR == type) {
        u8* d = DecodeStruct(ds, fieldDef, node, isCompact);
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
        if (ok) {
            WriteStructFloat(structDataPtr, f);
        }
    }

    if (TYPE_ARRAY == type) {
        if (!node->IsStruct()) {
            return false;
        }
        Vec<u8*>* vec = new Vec<u8*>();
        // we remember it right away, so that it gets freed in case of error
        WriteStructPtrVal(structDataPtr, (void*)vec);
        for (TxtNode* child = node->firstChild; child != nullptr; child = child->sibling) {
            u8* d = DecodeStruct(ds, fieldDef, child, isCompact);
            if (d) {
                vec->Append(d);
            }
        }
        return true;
    }

    CrashIf(true);
    return false;
}

static u8* DeserializeRec(DecodeState& ds, TxtNode* firstNode, const StructMetadata* def) {
    bool ok;
    if (!firstNode) {
        return nullptr;
    }

    u8* res = AllocArray<u8>(def->size);
    const StructMetadata** defPtr = (const StructMetadata**)res;
    *defPtr = def;
    const char* fieldName = def->fieldNames;
    for (int i = 0; i < def->nFields; i++) {
        ok = DecodeField(ds, firstNode, fieldName, def->fields + i, res);
        if (!ok) {
            goto Error;
        }
        fieldName = seqstrings::SkipStr(fieldName);
    }
    return res;
Error:
    FreeStruct(res, def);
    return nullptr;
}

u8* Deserialize(struct TxtNode* root, const StructMetadata* def) {
    DecodeState ds;
    return DeserializeRec(ds, root, def);
}

u8* Deserialize(const std::string_view str, const StructMetadata* def) {
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
        if (escapeChar == c) {
            escaped = escapeChar;
        } else if (']' == c) {
            escaped = ']';
        } else if ('[' == c) {
            escaped = '[';
        } else if ('\n' == c) {
            escaped = 'n';
        } else if ('\r' == c) {
            escaped = 'r';
        }
        if (0 == escaped) {
            continue;
        }

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

void SerializeRec(EncodeState& es, const u8* structStart, const StructMetadata* def);

static void SerializeField(EncodeState& es, const char* fieldName, const FieldMetadata* fieldDef,
                           const u8* structStart) {
    str::Str val;
    str::Str& res = es.res;

    Type type = fieldDef->type;
    if ((type & TYPE_NO_STORE_MASK) != 0) {
        return;
    }

    if (!structStart) {
        return;
    }

    bool isCompact = ((type & TYPE_STORE_COMPACT_MASK) != 0);
    type = (Type)(type & TYPE_MASK);

    const u8* data = structStart + fieldDef->offset;
    if (TYPE_BOOL == type) {
        bool b = ReadStructBool(data);
        AppendKeyVal(es, fieldName, b ? "true" : "false");
    } else if (TYPE_COLOR == type) {
        u64 u = ReadStructUInt(data, type);
        COLORREF c = (COLORREF)u;
        int r = (int)((u8)(c & 0xff));
        int g = (int)((u8)((c >> 8) & 0xff));
        int b = (int)((u8)((c >> 16) & 0xff));
        int a = (int)((u8)((c >> 24) & 0xff));
        if (a > 0) {
            val.AppendFmt("#%02x%02x%02x%02x", a, r, g, b);
        } else {
            val.AppendFmt("#%02x%02x%02x", r, g, b);
        }
        AppendKeyVal(es, fieldName, val.Get());
    } else if (IsUnsignedIntType(type)) {
        u64 u = ReadStructUInt(data, type);
        // val.AppendFmt("%" PRIu64, u);
        val.AppendFmt("%I64u", u);
        AppendKeyVal(es, fieldName, val.Get());
    } else if (IsSignedIntType(type)) {
        i64 i = ReadStructInt(data, type);
        // val.AppendFmt("%" PRIi64, u);
        val.AppendFmt("%I64d", i);
        AppendKeyVal(es, fieldName, val.Get());
    } else if (TYPE_FLOAT == type) {
        float f = ReadStructFloat(data);
        val.AppendFmt("%g", f);
        AppendKeyVal(es, fieldName, val.Get());
    } else if (TYPE_STR == type) {
        char* s = (char*)ReadStructPtr(data);
        if (s) {
            AppendKeyVal(es, fieldName, s);
        }
    } else if (TYPE_WSTR == type) {
        WCHAR* s = (WCHAR*)ReadStructPtr(data);
        if (s) {
            AutoFree val2(strconv::WstrToUtf8(s));
            AppendKeyVal(es, fieldName, val2.Get());
        }
    } else if (TYPE_STRUCT_PTR == type) {
        AppendNest(res, es.nest);
        res.Append(fieldName);
        if (isCompact) {
            res.Append(":");
        } else {
            res.Append(" [" NL);
        }
        const u8* structStart2 = (const u8*)ReadStructPtr(data);
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
        Vec<const u8*>* vec = (Vec<const u8*>*)ReadStructPtr(data);
        ++es.nest;
        for (size_t i = 0; vec && (i < vec->size()); i++) {
            AppendNest(res, es.nest);
            res.Append("[" NL);
            const u8* elData = vec->at(i);
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

void SerializeRec(EncodeState& es, const u8* structStart, const StructMetadata* def) {
    if (!structStart) {
        return;
    }
    const char* fieldName = def->fieldNames;
    for (size_t i = 0; i < def->nFields; i++) {
        const FieldMetadata* fieldDef = &def->fields[i];
        SerializeField(es, fieldName, fieldDef, structStart);
        fieldName = seqstrings::SkipStr(fieldName);
    }
}

std::string_view Serialize(const u8* rootStruct, const StructMetadata* def) {
    EncodeState es;
    es.res.Append(UTF8_BOM "; see https://www.sumatrapdfreader.org/settings/settings.html for documentation" NL);
    es.nest = 0;
    SerializeRec(es, rootStruct, def);
    size_t size = es.res.size();
    return {es.res.StealData(), size};
}

} // namespace sertxt
