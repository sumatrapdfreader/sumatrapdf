/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "SettingsUtil.h"
#include "SquareTreeParser.h"

static inline const StructInfo* GetSubstruct(const FieldInfo& field) {
    return (const StructInfo*)field.value;
}

// only escape characters which are significant to SquareTreeParser:
// newlines and leading/trailing whitespace (and escape characters)
static bool NeedsEscaping(const char* s) {
    return str::IsWs(*s) || *s && str::IsWs(*(s + str::Len(s) - 1)) || str::FindChar(s, '\n') ||
           str::FindChar(s, '\r') || str::FindChar(s, '$');
}

static void EscapeStr(str::Str& out, const char* s) {
    ReportIf(!NeedsEscaping(s));
    if (str::IsWs(*s) && *s != '\n' && *s != '\r') {
        out.AppendChar('$');
    }
    for (const char* c = s; *c; c++) {
        switch (*c) {
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
                out.AppendChar(*c);
        }
    }
    if (*s && str::IsWs(s[str::Len(s) - 1])) {
        out.AppendChar('$');
    }
}

static char* UnescapeStr(const char* s) {
    if (!str::FindChar(s, '$')) {
        return str::Dup(s);
    }

    str::Str ret;
    const char* end = s + str::Len(s);
    if ('$' == *s && str::IsWs(*(s + 1))) {
        s++; // leading whitespace
    }
    for (const char* c = s; c < end; c++) {
        if (*c != '$') {
            ret.AppendChar(*c);
            continue;
        }
        switch (*++c) {
            case '$':
                ret.AppendChar('$');
                break;
            case 'n':
                ret.AppendChar('\n');
                break;
            case 'r':
                ret.AppendChar('\r');
                break;
            case '\0':
                break; // trailing whitespace
            default:
                // keep all other instances of the escape character
                ret.AppendChar('$');
                ret.AppendChar(*c);
                break;
        }
    }
    return ret.StealData();
}

// string arrays are serialized by quoting strings containing spaces
// or quotation marks (doubling quotation marks within quotes);
// this is simpler than full command line serialization as read by ParseCmdLine
static char* SerializeUtf8StringArray(const Vec<char*>* strArray) {
    str::Str serialized;

    for (size_t i = 0; i < strArray->size(); i++) {
        if (i > 0) {
            serialized.AppendChar(' ');
        }
        const char* str = strArray->at(i);
        bool needsQuotes = !*str;
        for (const char* c = str; !needsQuotes && *c; c++) {
            needsQuotes = str::IsWs(*c) || '"' == *c;
        }
        if (!needsQuotes) {
            serialized.Append(str);
        } else {
            serialized.AppendChar('"');
            for (const char* c = str; *c; c++) {
                if ('"' == *c) {
                    serialized.AppendChar('"');
                }
                serialized.AppendChar(*c);
            }
            serialized.AppendChar('"');
        }
    }

    return (char*)serialized.StealData();
}

static char* skipNonWhitespace(const char* s) {
    while (*s && !str::IsWs(*s)) {
        s++;
    }
    return (char*)s;
}

static char* skipWhitespace(const char* s) {
    while (str::IsWs(*s)) {
        s++;
    }
    return (char*)s;
}

static void DeserializeUtf8StringArray(Vec<char*>* strArray, const char* serialized) {
    char* str = (char*)serialized;
    const char* s = str;

    for (;;) {
        s = skipWhitespace(s);
        if (!*s) {
            return;
        }
        if ('"' == *s) {
            str::Str part;
            for (s++; *s && (*s != '"' || *(s + 1) == '"'); s++) {
                if ('"' == *s) {
                    s++;
                }
                part.AppendChar(*s);
            }
            strArray->Append(part.StealData());
            if ('"' == *s) {
                s++;
            }
        } else {
            const char* e = skipNonWhitespace(s);
            strArray->Append(str::Dup(s, e - s));
            s = e;
        }
    }
}

static void FreeUtf8StringArray(Vec<char*>* strArray) {
    if (!strArray) {
        return;
    }
    strArray->FreeMembers();
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

static bool SerializeField(str::Str& out, const u8* base, const FieldInfo& field) {
    const u8* fieldPtr = base + field.offset;
    AutoFreeStr value;

    switch (field.type) {
        case SettingType::Bool:
            out.Append(*(bool*)fieldPtr ? "true" : "false");
            return true;
        case SettingType::Int:
            out.AppendFmt("%d", *(int*)fieldPtr);
            return true;
        case SettingType::Float:
            out.AppendFmt("%g", *(float*)fieldPtr);
            return true;
        case SettingType::String:
        case SettingType::Color:
            if (!*(const char**)fieldPtr) {
                ReportIf(field.value);
                return false; // skip empty strings
            }
            if (!NeedsEscaping(*(const char**)fieldPtr)) {
                out.Append(*(const char**)fieldPtr);
            } else {
                EscapeStr(out, *(const char**)fieldPtr);
            }
            return true;
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
            for (size_t i = 0; i < (*(Vec<int>**)fieldPtr)->size(); i++) {
                FieldInfo info{};
                info.type = SettingType::Int;
                if (field.type == SettingType::FloatArray) {
                    info.type = SettingType::Float;
                }
                if (i > 0) {
                    out.AppendChar(' ');
                }
                SerializeField(out, (const u8*)&(*(Vec<int>**)fieldPtr)->at(i), info);
            }
            // prevent empty arrays from being replaced with the defaults
            return (*(Vec<int>**)fieldPtr)->size() > 0 || field.value != 0;
        case SettingType::ColorArray:
        case SettingType::StringArray:
            value.Set(SerializeUtf8StringArray(*(Vec<char*>**)fieldPtr));
            if (!NeedsEscaping(value)) {
                out.Append(value);
            } else {
                EscapeStr(out, value);
            }
            // prevent empty arrays from being replaced with the defaults
            return (*(Vec<char*>**)fieldPtr)->size() > 0 || field.value != 0;
        default:
            ReportIf(true);
            return false;
    }
}

// boolean true are "true", "yes" and any non-zero integer
static bool parseBool(const char* value) {
    if (str::StartsWithI(value, "true") && (!value[4] || str::IsWs(value[4]))) {
        return true;
    }
    if (str::StartsWithI(value, "yes") && (!value[3] || str::IsWs(value[3]))) {
        return true;
    }

    int i = ParseInt(value);
    return i != 0;
}

static void deserializeField(const FieldInfo& field, u8* base, const char* value) {
    u8* fieldPtr = base + field.offset;

    switch (field.type) {
        case SettingType::Bool: {
            bool* boolPtr = (bool*)fieldPtr;
            if (value) {
                *boolPtr = parseBool(value);
            } else {
                *boolPtr = field.value != 0;
            }
            break;
        }

        case SettingType::Int: {
            int* intPtr = (int*)fieldPtr;
            if (value) {
                *intPtr = ParseInt(value);
            } else {
                *intPtr = (int)field.value;
            }
        } break;

        case SettingType::Float: {
            const char* s = value ? value : (const char*)field.value;
            str::Parse(s, "%f", (float*)fieldPtr);
            break;
        }

        case SettingType::Color:
        case SettingType::String: {
            char** strPtr = (char**)fieldPtr;
            free(*strPtr);
            if (value) {
                *strPtr = UnescapeStr(value);
            } else {
                *strPtr = str::Dup((const char*)field.value);
            }
        } break;

        case SettingType::Compact:
            ReportIf(!IsCompactable(GetSubstruct(field)));
            for (size_t i = 0; i < GetSubstruct(field)->fieldCount; i++) {
                if (value) {
                    value = skipWhitespace(value);
                    if (!*value) {
                        value = nullptr;
                    }
                }
                deserializeField(GetSubstruct(field)->fields[i], fieldPtr, value);
                if (value) {
                    value = skipNonWhitespace(value);
                }
            }
            break;
        case SettingType::FloatArray:
        case SettingType::IntArray: {
            if (!value) {
                value = (const char*)field.value;
            }
            Vec<int>* v = *(Vec<int>**)fieldPtr;
            delete v;
            v = new Vec<int>();
            *(Vec<int>**)fieldPtr = v;
            while (value && *value) {
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
                deserializeField(info, (u8*)v->AppendBlanks(1), value);
                value = skipNonWhitespace(value);
                value = skipWhitespace(value);
            }
        } break;
        case SettingType::ColorArray:
        case SettingType::StringArray: {
            Vec<char*>* v = *(Vec<char*>**)fieldPtr;
            FreeUtf8StringArray(v);
            v = new Vec<char*>();
            *(Vec<char*>**)fieldPtr = v;
            if (value) {
                char* v2 = UnescapeStr(value);
                DeserializeUtf8StringArray(v, v2);
                free(v2);
            } else if (field.value) {
                DeserializeUtf8StringArray(v, (const char*)field.value);
            }
        } break;
        default:
            ReportIf(true);
    }
}

static inline void Indent(str::Str& out, int indent) {
    while (indent-- > 0) {
        out.AppendChar('\t');
    }
}

static void MarkFieldKnown(SquareTreeNode* node, const char* fieldName, SettingType type) {
    if (!node) {
        return;
    }
    size_t off = 0;
    if (SettingType::Struct == type || SettingType::Prerelease == type) {
        if (node->GetChild(fieldName, &off)) {
            delete node->data.at(off - 1).child;
            node->data.RemoveAt(off - 1);
        }
    } else if (SettingType::Array == type) {
        while (node->GetChild(fieldName, &off)) {
            delete node->data.at(off - 1).child;
            node->data.RemoveAt(off - 1);
            off--;
        }
    } else if (node->GetValue(fieldName, &off)) {
        node->data.RemoveAt(off - 1);
    }
}

static void SerializeUnknownFields(str::Str& out, SquareTreeNode* node, int indent) {
    if (!node) {
        return;
    }
    for (size_t i = 0; i < node->data.size(); i++) {
        SquareTreeNode::DataItem& item = node->data.at(i);
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

static void SerializeStructRec(str::Str& out, const StructInfo* info, const void* data, SquareTreeNode* prevNode,
                               int indent = 0) {
    const u8* base = (const u8*)data;
    const char* fieldName = info->fieldNames;
    for (size_t i = 0; i < info->fieldCount; i++, fieldName += str::Len(fieldName) + 1) {
        const FieldInfo& field = info->fields[i];
        ReportIf(str::FindChar(fieldName, '=') || str::FindChar(fieldName, ':') || str::FindChar(fieldName, '[') ||
                 str::FindChar(fieldName, ']') || NeedsEscaping(fieldName));
        if (SettingType::Struct == field.type || SettingType::Prerelease == field.type) {
#if !(defined(PRE_RELEASE_VER) || defined(DEBUG))
            if (SettingType::Prerelease == field.type) {
                continue;
            }
#endif
            Indent(out, indent);
            out.Append(fieldName);
            out.Append(" [\r\n");
            SerializeStructRec(out, GetSubstruct(field), base + field.offset,
                               prevNode ? prevNode->GetChild(fieldName) : nullptr, indent + 1);
            Indent(out, indent);
            out.Append("]\r\n");
        } else if (SettingType::Array == field.type) {
            Indent(out, indent);
            out.Append(fieldName);
            out.Append(" [\r\n");
            Vec<void*>* array = *(Vec<void*>**)(base + field.offset);
            if (array && array->size() > 0) {
                for (size_t j = 0; j < array->size(); j++) {
                    Indent(out, indent + 1);
                    out.Append("[\r\n");
                    SerializeStructRec(out, GetSubstruct(field), array->at(j), nullptr, indent + 2);
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
                out.Append((const char*)field.value);
            }
            out.Append("\r\n");
        } else {
            size_t offset = out.size();
            Indent(out, indent);
            out.Append(fieldName);
            out.Append(" = ");
            bool keep = SerializeField(out, base, field);
            if (keep) {
                out.Append("\r\n");
            } else {
                out.RemoveAt(offset, out.size() - offset);
            }
        }
        MarkFieldKnown(prevNode, fieldName, field.type);
    }
    SerializeUnknownFields(out, prevNode, indent);
}

static void* DeserializeStructRec(const StructInfo* info, SquareTreeNode* node, u8* base, bool useDefaults) {
    if (!base) {
        base = AllocArray<u8>(info->structSize);
    }

    const char* fieldName = info->fieldNames;
    for (size_t i = 0; i < info->fieldCount; i++, fieldName += str::Len(fieldName) + 1) {
        const FieldInfo& field = info->fields[i];
        u8* fieldPtr = base + field.offset;
        if (SettingType::Struct == field.type || SettingType::Prerelease == field.type) {
            SquareTreeNode* child = node ? node->GetChild(fieldName) : nullptr;
#if !(defined(PRE_RELEASE_VER) || defined(DEBUG))
            if (SettingType::Prerelease == field.type) {
                child = nullptr;
            }
#endif
            DeserializeStructRec(GetSubstruct(field), child, fieldPtr, useDefaults);
        } else if (SettingType::Array == field.type) {
            SquareTreeNode *parent = node, *child = nullptr;
            if (parent && (child = parent->GetChild(fieldName)) != nullptr &&
                (0 == child->data.size() || child->GetChild(""))) {
                parent = child;
                fieldName += str::Len(fieldName);
            }
            if (child || useDefaults || !*(Vec<void*>**)fieldPtr) {
                Vec<void*>* array = new Vec<void*>();
                size_t idx = 0;
                while (parent && (child = parent->GetChild(fieldName, &idx)) != nullptr) {
                    void* v = DeserializeStructRec(GetSubstruct(field), child, nullptr, true);
                    array->Append(v);
                }
                FreeArray(*(Vec<void*>**)fieldPtr, field);
                *(Vec<void*>**)fieldPtr = array;
            }
        } else if (field.type != SettingType::Comment) {
            const char* value = node ? node->GetValue(fieldName) : nullptr;
            if (useDefaults || value) {
                deserializeField(field, base, value);
            }
        }
    }
    return base;
}

ByteSlice SerializeStruct(const StructInfo* info, const void* strct, const char* prevData) {
    str::Str out;
    out.Append(UTF8_BOM);
    SquareTreeNode* root = ParseSquareTree(prevData);
    SerializeStructRec(out, info, strct, root);
    delete root;
    return out.StealAsByteSlice();
}

void* DeserializeStruct(const StructInfo* info, const char* data, void* strct) {
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
            case SettingType::Color:
            case SettingType::String: {
                void* str = *((void**)fieldPtr);
                free(str);
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
                Vec<char*>* strArray = *(Vec<char*>**)fieldPtr;
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
