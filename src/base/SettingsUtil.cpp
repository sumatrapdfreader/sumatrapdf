/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Base.h"
#include "SettingsUtil.h"
#include "SquareTreeParser.h"

static inline const StructInfo* GetSubstruct(const FieldInfo& field) {
    return (const StructInfo*)field.value;
}

static Str FieldDefaultStr(const FieldInfo& field) {
    return Str((const char*)field.value);
}

// only escape characters which are significant to SquareTreeParser:
// newlines and leading/trailing whitespace (and escape characters)
static bool NeedsEscaping(Str s) {
    if (!s) {
        return false;
    }
    return str::IsWs(s.s[0]) || str::IsWs(s.s[s.len - 1]) || str::ContainsChar(s, '\n') || str::ContainsChar(s, '\r') ||
           str::ContainsChar(s, '$');
}

static void EscapeStr(str::Builder& out, Str s) {
    ReportIf(!NeedsEscaping(s));
    if (str::IsWs(s.s[0]) && s.s[0] != '\n' && s.s[0] != '\r') {
        out.AppendChar('$');
    }
    for (int i = 0; i < s.len; i++) {
        char c = s.s[i];
        switch (c) {
            case '$':
                out.Append("$$");
                break;
            case '\n':
                out.Append("$n");
                break;
            case '\r':
                out.Append("$r");
                break;
            default:
                out.AppendChar(c);
        }
    }
    if (str::IsWs(s.s[s.len - 1])) {
        out.AppendChar('$');
    }
}

static Str UnescapeStr(Str s) {
    if (str::IsNull(s)) {
        return {};
    }
    if (!str::ContainsChar(s, '$')) {
        return str::Dup(s);
    }

    str::Builder ret;
    int off = 0;
    if (s.s[0] == '$' && s.len > 1 && str::IsWs(s.s[1])) {
        off = 1; // leading whitespace
    }
    for (int i = off; i < s.len; i++) {
        char c = s.s[i];
        if (c != '$') {
            ret.AppendChar(c);
            continue;
        }
        if (++i >= s.len) {
            break;
        }
        switch (s.s[i]) {
            case '$':
                ret.AppendChar('$');
                break;
            case 'n':
                ret.AppendChar('\n');
                break;
            case 'r':
                ret.AppendChar('\r');
                break;
            default:
                // keep all other instances of the escape character
                ret.AppendChar('$');
                ret.AppendChar(s.s[i]);
                break;
        }
    }
    return ret.TakeStr();
}

// string arrays are serialized by quoting strings containing spaces
// or quotation marks (doubling quotation marks within quotes);
// this is simpler than full command line serialization as read by ParseCmdLine
static Str SerializeUtf8StringArray(const Vec<Str>* strArray) {
    str::Builder serialized;

    for (int i = 0; i < len(*strArray); i++) {
        if (i > 0) {
            serialized.AppendChar(' ');
        }
        Str str = (*strArray)[i];
        bool needsQuotes = !str;
        for (int j = 0; str && !needsQuotes && j < str.len; j++) {
            char c = str.s[j];
            needsQuotes = str::IsWs(c) || '"' == c;
        }
        if (!needsQuotes) {
            serialized.Append(str);
        } else {
            serialized.AppendChar('"');
            for (int j = 0; str && j < str.len; j++) {
                char c = str.s[j];
                if ('"' == c) {
                    serialized.AppendChar('"');
                }
                serialized.AppendChar(c);
            }
            serialized.AppendChar('"');
        }
    }

    return serialized.TakeStr();
}

static int SkipNonWhitespaceOff(Str s, int off) {
    while (off < s.len && !str::IsWs(s.s[off])) {
        off++;
    }
    return off;
}

static int SkipWhitespaceOff(Str s, int off) {
    while (off < s.len && str::IsWs(s.s[off])) {
        off++;
    }
    return off;
}

static void DeserializeUtf8StringArray(Vec<Str>* strArray, Str serialized) {
    int off = 0;

    for (;;) {
        off = SkipWhitespaceOff(serialized, off);
        if (off >= serialized.len) {
            return;
        }
        if ('"' == serialized.s[off]) {
            str::Builder part;
            for (off++; off < serialized.len;) {
                if (serialized.s[off] == '"' && (off + 1 >= serialized.len || serialized.s[off + 1] != '"')) {
                    break;
                }
                if (serialized.s[off] == '"') {
                    off++;
                }
                part.AppendChar(serialized.s[off]);
                off++;
            }
            strArray->Append(part.TakeStr());
            if (off < serialized.len && '"' == serialized.s[off]) {
                off++;
            }
        } else {
            int end = SkipNonWhitespaceOff(serialized, off);
            strArray->Append(str::Dup(Str(serialized.s + off, end - off)));
            off = end;
        }
    }
}

static void FreeUtf8StringArray(Vec<Str>* strArray) {
    if (!strArray) {
        return;
    }
    for (int i = 0; i < len(*strArray); i++) {
        str::Free((*strArray)[i]);
    }
    delete strArray;
}

static void FreeArray(Vec<void*>* array, const FieldInfo& field) {
    if (!array) {
        return;
    }
    auto structInfo = GetSubstruct(field);
    for (auto el : *array) {
        FreeStruct(structInfo, el);
    }
    delete array;
}

bool IsCompactable(const StructInfo* info) {
    for (size_t i = 0; i < info->fieldCount; i++) {
        switch (info->fields[i].type) {
            case SettingType::Bool:
            case SettingType::Int:
            case SettingType::Float:
            case SettingType::Color:
                continue;
            default:
                return false;
        }
    }
    return info->fieldCount > 0;
}

static_assert(sizeof(float) == sizeof(int) && sizeof(COLORREF) == sizeof(int),
              "compact array code can't be simplified if int, float and colorref are of different sizes");

static bool SerializeField(str::Builder& out, const u8* base, const FieldInfo& field) {
    const u8* fieldPtr = base + field.offset;

    switch (field.type) {
        case SettingType::Bool:
            out.Append(*(bool*)fieldPtr ? "true" : "false");
            return true;
        case SettingType::Int:
            out.Append(fmt("%d", *(int*)fieldPtr));
            return true;
        case SettingType::Float:
            out.Append(fmt("%g", *(float*)fieldPtr));
            return true;
        case SettingType::String: {
            Str str = *(Str*)fieldPtr;
            if (!str) {
                return false; // skip empty strings
            }
            if (!NeedsEscaping(str)) {
                out.Append(str);
            } else {
                EscapeStr(out, str);
            }
            return true;
        }
        case SettingType::Color: {
            Str str = *(Str*)fieldPtr;
            if (!str) {
                return false; // skip empty strings
            }
            if (!NeedsEscaping(str)) {
                out.Append(str);
            } else {
                EscapeStr(out, str);
            }
            return true;
        }
        case SettingType::Compact:
            ReportIf(!IsCompactable(GetSubstruct(field)));
            for (size_t i = 0; i < GetSubstruct(field)->fieldCount; i++) {
                if (i > 0) {
                    out.AppendChar(' ');
                }
                SerializeField(out, fieldPtr, GetSubstruct(field)->fields[i]);
            }
            return true;
        case SettingType::FloatArray:
        case SettingType::IntArray:
            for (int i = 0; i < len(**(Vec<int>**)fieldPtr); i++) {
                FieldInfo info{};
                info.type = SettingType::Int;
                if (field.type == SettingType::FloatArray) {
                    info.type = SettingType::Float;
                }
                if (i > 0) {
                    out.AppendChar(' ');
                }
                SerializeField(out, (const u8*)&(*(*(Vec<int>**)fieldPtr))[i], info);
            }
            // prevent empty arrays from being replaced with the defaults
            return len(**(Vec<int>**)fieldPtr) > 0 || field.value != 0;
        case SettingType::ColorArray:
        case SettingType::StringArray: {
            Str serialized = SerializeUtf8StringArray(*(Vec<Str>**)fieldPtr);
            if (!NeedsEscaping(serialized)) {
                out.Append(serialized);
            } else {
                EscapeStr(out, serialized);
            }
            str::Free(serialized);
            // prevent empty arrays from being replaced with the defaults
            return len(**(Vec<Str>**)fieldPtr) > 0 || field.value != 0;
        }
        default:
            ReportIf(true);
            return false;
    }
}

// boolean true are "true", "yes" and any non-zero integer
static bool parseBool(Str value) {
    if (str::IsNull(value)) {
        return false;
    }
    if (str::StartsWithI(value, "true") && (value.len <= 4 || str::IsWs(value.s[4]))) {
        return true;
    }
    if (str::StartsWithI(value, "yes") && (value.len <= 3 || str::IsWs(value.s[3]))) {
        return true;
    }

    int i = ParseInt(value);
    return i != 0;
}

static void deserializeField(const FieldInfo& field, u8* base, Str value) {
    u8* fieldPtr = base + field.offset;

    switch (field.type) {
        case SettingType::Bool: {
            bool* boolPtr = (bool*)fieldPtr;
            if (!str::IsNull(value)) {
                *boolPtr = parseBool(value);
            } else {
                *boolPtr = field.value != 0;
            }
            break;
        }

        case SettingType::Int: {
            int* intPtr = (int*)fieldPtr;
            if (!str::IsNull(value)) {
                *intPtr = ParseInt(value);
            } else {
                *intPtr = (int)field.value;
            }
        } break;

        case SettingType::Float: {
            Str s = !str::IsNull(value) ? value : FieldDefaultStr(field);
            str::Parse(s, "%f", (float*)fieldPtr);
            break;
        }

        case SettingType::Color: {
            Str* strPtr = (Str*)fieldPtr;
            str::Free(*strPtr);
            if (!str::IsNull(value)) {
                *strPtr = UnescapeStr(value);
            } else {
                *strPtr = str::Dup(FieldDefaultStr(field));
            }
        } break;

        case SettingType::String: {
            Str* strPtr = (Str*)fieldPtr;
            str::Free(*strPtr);
            if (!str::IsNull(value)) {
                *strPtr = UnescapeStr(value);
            } else {
                *strPtr = str::Dup(FieldDefaultStr(field));
            }
        } break;

        case SettingType::Compact:
            ReportIf(!IsCompactable(GetSubstruct(field)));
            {
                int off = 0;
                for (size_t i = 0; i < GetSubstruct(field)->fieldCount; i++) {
                    Str token;
                    if (!str::IsNull(value)) {
                        off = SkipWhitespaceOff(value, off);
                        token = off < value.len ? Str(value.s + off, value.len - off) : Str{};
                    }
                    deserializeField(GetSubstruct(field)->fields[i], fieldPtr, token);
                    if (!str::IsNull(value)) {
                        off = SkipNonWhitespaceOff(value, off);
                    }
                }
            }
            break;
        case SettingType::FloatArray:
        case SettingType::IntArray: {
            Str src = !str::IsNull(value) ? value : FieldDefaultStr(field);
            Vec<int>* v = *(Vec<int>**)fieldPtr;
            delete v;
            v = new Vec<int>();
            *(Vec<int>**)fieldPtr = v;
            int off = 0;
            while (src && off < src.len) {
                FieldInfo info{};
                switch (field.type) {
                    case SettingType::IntArray:
                        info.type = SettingType::Int;
                        break;
                    case SettingType::FloatArray:
                        info.type = SettingType::Float;
                        break;
                    default:
                        ReportIf(true);
                        break;
                }
                Str token = Str(src.s + off, src.len - off);
                deserializeField(info, (u8*)v->AppendBlanks(1), token);
                off = SkipNonWhitespaceOff(src, off);
                off = SkipWhitespaceOff(src, off);
            }
        } break;
        case SettingType::ColorArray:
        case SettingType::StringArray: {
            Vec<Str>* v = *(Vec<Str>**)fieldPtr;
            FreeUtf8StringArray(v);
            v = new Vec<Str>();
            *(Vec<Str>**)fieldPtr = v;
            if (!str::IsNull(value)) {
                DeserializeUtf8StringArray(v, UnescapeStr(value));
            } else if (field.value) {
                DeserializeUtf8StringArray(v, FieldDefaultStr(field));
            }
        } break;
        default:
            ReportIf(true);
    }
}

static inline void Indent(str::Builder& out, int indent) {
    while (indent-- > 0) {
        out.AppendChar('\t');
    }
}

// removes the item from node->data, freeing what ~SquareTreeNode would
static void RemoveDataItemAt(SquareTreeNode* node, size_t idx) {
    SquareTreeNode::DataItem& item = node->data[(int)idx];
    str::Free(item.key);
    str::Free(item.str);
    delete item.child;
    node->data.RemoveAt((int)idx);
}

static void MarkFieldKnown(SquareTreeNode* node, Str fieldName, SettingType type) {
    if (!node) {
        return;
    }
    size_t off = 0;
    if (SettingType::Struct == type || SettingType::Prerelease == type) {
        if (node->GetChild(fieldName, &off)) {
            RemoveDataItemAt(node, off - 1);
        }
    } else if (SettingType::Array == type) {
        while (node->GetChild(fieldName, &off)) {
            RemoveDataItemAt(node, off - 1);
            off--;
        }
    } else {
        Str value = node->GetValue(fieldName, &off);
        if (!str::IsNull(value)) {
            RemoveDataItemAt(node, off - 1);
        }
    }
}

static void SerializeUnknownFields(str::Builder& out, SquareTreeNode* node, int indent) {
    if (!node) {
        return;
    }
    for (int i = 0; i < len(node->data); i++) {
        SquareTreeNode::DataItem& item = node->data[i];
        Indent(out, indent);
        out.Append(item.key);
        if (item.child) {
            out.Append(" [\r\n");
            SerializeUnknownFields(out, item.child, indent + 1);
            Indent(out, indent);
            out.Append("]\r\n");
        } else {
            out.Append(" = ");
            out.Append(item.str);
            out.Append("\r\n");
        }
    }
}

static void SerializeStructRec(str::Builder& out, const StructInfo* info, const void* data, SquareTreeNode* prevNode,
                               int indent = 0) {
    const u8* base = (const u8*)data;
    const char* fieldName = info->fieldNames;
    for (size_t i = 0; i < info->fieldCount; i++, fieldName += len(fieldName) + 1) {
        const FieldInfo& field = info->fields[i];
        Str fieldNameStr = Str(fieldName);
        ReportIf(str::ContainsChar(fieldNameStr, '=') || str::ContainsChar(fieldNameStr, ':') ||
                 str::ContainsChar(fieldNameStr, '[') || str::ContainsChar(fieldNameStr, ']') ||
                 NeedsEscaping(fieldNameStr));
        if (SettingType::Struct == field.type || SettingType::Prerelease == field.type) {
#if !(defined(PRE_RELEASE_VER) || defined(DEBUG))
            if (SettingType::Prerelease == field.type) {
                continue;
            }
#endif
            Indent(out, indent);
            out.Append(fieldNameStr);
            out.Append(" [\r\n");
            SerializeStructRec(out, GetSubstruct(field), base + field.offset,
                               prevNode ? prevNode->GetChild(fieldNameStr) : nullptr, indent + 1);
            Indent(out, indent);
            out.Append("]\r\n");
        } else if (SettingType::Array == field.type) {
            Indent(out, indent);
            out.Append(fieldNameStr);
            out.Append(" [\r\n");
            Vec<void*>* array = *(Vec<void*>**)(base + field.offset);
            if (array && len(*array) > 0) {
                for (int j = 0; j < len(*array); j++) {
                    Indent(out, indent + 1);
                    out.Append("[\r\n");
                    SerializeStructRec(out, GetSubstruct(field), (*array)[j], nullptr, indent + 2);
                    Indent(out, indent + 1);
                    out.Append("]\r\n");
                }
            }
            Indent(out, indent);
            out.Append("]\r\n");
        } else if (SettingType::Comment == field.type) {
            if (field.value) {
                Indent(out, indent);
                out.Append("# ");
                out.Append(FieldDefaultStr(field));
            }
            out.Append("\r\n");
        } else {
            int offset = len(out);
            Indent(out, indent);
            out.Append(fieldNameStr);
            out.Append(" = ");
            bool keep = SerializeField(out, base, field);
            if (keep) {
                out.Append("\r\n");
            } else {
                out.RemoveAt(offset, len(out) - offset);
            }
        }
        MarkFieldKnown(prevNode, fieldNameStr, field.type);
    }
    SerializeUnknownFields(out, prevNode, indent);
}

static void* DeserializeStructRec(const StructInfo* info, SquareTreeNode* node, u8* base, bool useDefaults) {
    if (!base) {
        base = AllocArray<u8>(info->structSize);
    }

    const char* fieldName = info->fieldNames;
    for (size_t i = 0; i < info->fieldCount; i++, fieldName += len(fieldName) + 1) {
        const FieldInfo& field = info->fields[i];
        u8* fieldPtr = base + field.offset;
        Str fieldNameStr = Str(fieldName);
        if (SettingType::Struct == field.type || SettingType::Prerelease == field.type) {
            SquareTreeNode* child = node ? node->GetChild(fieldNameStr) : nullptr;
#if !(defined(PRE_RELEASE_VER) || defined(DEBUG))
            if (SettingType::Prerelease == field.type) {
                child = nullptr;
            }
#endif
            DeserializeStructRec(GetSubstruct(field), child, fieldPtr, useDefaults);
        } else if (SettingType::Array == field.type) {
            SquareTreeNode *parent = node, *child = nullptr;
            if (parent && (child = parent->GetChild(fieldNameStr)) != nullptr &&
                (0 == len(child->data) || child->GetChild(StrL("")))) {
                parent = child;
                fieldName += len(fieldName);
                fieldNameStr = Str(fieldName);
            }
            if (child || useDefaults || !*(Vec<void*>**)fieldPtr) {
                Vec<void*>* array = new Vec<void*>();
                size_t idx = 0;
                while (parent && (child = parent->GetChild(fieldNameStr, &idx)) != nullptr) {
                    void* v = DeserializeStructRec(GetSubstruct(field), child, nullptr, true);
                    array->Append(v);
                }
                FreeArray(*(Vec<void*>**)fieldPtr, field);
                *(Vec<void*>**)fieldPtr = array;
            }
        } else if (field.type != SettingType::Comment) {
            Str value = node ? node->GetValue(fieldNameStr) : Str{};
            if (useDefaults || !str::IsNull(value)) {
                deserializeField(field, base, value);
            }
        }
    }
    return base;
}

Str SerializeStruct(const StructInfo* info, const void* strct, Str prevData) {
    str::Builder out;
    out.Append(UTF8_BOM);
    SquareTreeNode* root = ParseSquareTree(prevData);
    SerializeStructRec(out, info, strct, root);
    delete root;
    return out.TakeStr();
}

void* DeserializeStruct(const StructInfo* info, Str data, void* strct) {
    SquareTreeNode* root = ParseSquareTree(data);
    auto res = DeserializeStructRec(info, root, (u8*)strct, !strct);
    delete root;
    return res;
}

static void FreeStructData(const StructInfo* info, u8* base) {
    for (size_t i = 0; i < info->fieldCount; i++) {
        const FieldInfo& field = info->fields[i];
        u8* fieldPtr = base + field.offset;
        switch (field.type) {
            case SettingType::Bool:
            case SettingType::Int:
            case SettingType::Float:
            case SettingType::Comment:
                // nothing to free
                break;
            case SettingType::Struct:
            case SettingType::Prerelease: {
                const StructInfo* substruct = GetSubstruct(field);
                FreeStructData(substruct, fieldPtr);
                break;
            }
            case SettingType::Array: {
                Vec<void*>* array = *(Vec<void*>**)fieldPtr;
                FreeArray(array, field);
                break;
            }
            case SettingType::Color: {
                Str* str = (Str*)fieldPtr;
                str::Free(*str);
                str->s = nullptr;
                str->len = 0;
                break;
            }
            case SettingType::String: {
                Str* str = (Str*)fieldPtr;
                str::Free(*str);
                str->s = nullptr;
                str->len = 0;
                break;
            }
            case SettingType::FloatArray:
            case SettingType::IntArray: {
                Vec<int>* vec = *((Vec<int>**)fieldPtr);
                delete vec;
                break;
            }
            case SettingType::StringArray:
            case SettingType::ColorArray: {
                Vec<Str>* strArray = *(Vec<Str>**)fieldPtr;
                FreeUtf8StringArray(strArray);
                break;
            }
            default:
                break;
        }
    }
}

void FreeStruct(const StructInfo* info, void* strct) {
    if (!strct) {
        return;
    }
    FreeStructData(info, (u8*)strct);
    free(strct);
}
