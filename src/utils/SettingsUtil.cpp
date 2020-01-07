/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "SettingsUtil.h"
#include "SquareTreeParser.h"
#include "utils/ColorUtil.h"

static inline const StructInfo* GetSubstruct(const FieldInfo& field) {
    return (const StructInfo*)field.value;
}

static int ParseInt(const char* bytes) {
    bool negative = *bytes == '-';
    if (negative)
        bytes++;
    int value = 0;
    for (; str::IsDigit(*bytes); bytes++) {
        value = value * 10 + (*bytes - '0');
        // return 0 on overflow
        if (value - (negative ? 1 : 0) < 0)
            return 0;
    }
    return negative ? -value : value;
}

// only escape characters which are significant to SquareTreeParser:
// newlines and leading/trailing whitespace (and escape characters)
static bool NeedsEscaping(const char* s) {
    return str::IsWs(*s) || *s && str::IsWs(*(s + str::Len(s) - 1)) || str::FindChar(s, '\n') ||
           str::FindChar(s, '\r') || str::FindChar(s, '$');
}

static void EscapeStr(str::Str& out, const char* s) {
    CrashIf(!NeedsEscaping(s));
    if (str::IsWs(*s) && *s != '\n' && *s != '\r')
        out.AppendChar('$');
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
    if (!str::FindChar(s, '$'))
        return str::Dup(s);

    str::Str ret;
    const char* end = s + str::Len(s);
    if ('$' == *s && str::IsWs(*(s + 1)))
        s++; // leading whitespace
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
static char* SerializeStringArray(const Vec<WCHAR*>* strArray) {
    str::WStr serialized;

    for (size_t i = 0; i < strArray->size(); i++) {
        if (i > 0)
            serialized.Append(' ');
        const WCHAR* str = strArray->at(i);
        bool needsQuotes = !*str;
        for (const WCHAR* c = str; !needsQuotes && *c; c++) {
            needsQuotes = str::IsWs(*c) || '"' == *c;
        }
        if (!needsQuotes) {
            serialized.Append(str);
        } else {
            serialized.Append('"');
            for (const WCHAR* c = str; *c; c++) {
                if ('"' == *c)
                    serialized.Append('"');
                serialized.Append(*c);
            }
            serialized.Append('"');
        }
    }

    auto tmp = strconv::WstrToUtf8(serialized.Get());
    return (char*)tmp.data();
}

static void DeserializeStringArray(Vec<WCHAR*>* strArray, const char* serialized) {
    AutoFreeWstr str = strconv::Utf8ToWstr(serialized);
    const WCHAR* s = str.Get();

    for (;;) {
        while (str::IsWs(*s))
            s++;
        if (!*s)
            return;
        if ('"' == *s) {
            str::WStr part;
            for (s++; *s && (*s != '"' || *(s + 1) == '"'); s++) {
                if ('"' == *s)
                    s++;
                part.Append(*s);
            }
            strArray->Append(part.StealData());
            if ('"' == *s)
                s++;
        } else {
            const WCHAR* e;
            for (e = s; *e && !str::IsWs(*e); e++)
                ;
            strArray->Append(str::DupN(s, e - s));
            s = e;
        }
    }
}

static void FreeStringArray(Vec<WCHAR*>* strArray) {
    if (!strArray)
        return;
    strArray->FreeMembers();
    delete strArray;
}

static void FreeArray(Vec<void*>* array, const FieldInfo& field) {
    for (size_t j = 0; array && j < array->size(); j++) {
        FreeStruct(GetSubstruct(field), array->at(j));
    }
    delete array;
}

bool IsCompactable(const StructInfo* info) {
    for (size_t i = 0; i < info->fieldCount; i++) {
        switch (info->fields[i].type) {
            case Type_Bool:
            case Type_Int:
            case Type_Float:
            case Type_Color:
                continue;
            default:
                return false;
        }
    }
    return info->fieldCount > 0;
}

static_assert(sizeof(float) == sizeof(int) && sizeof(COLORREF) == sizeof(int),
              "compact array code can't be simplified if int, float and colorref are of different sizes");

static bool SerializeField(str::Str& out, const uint8_t* base, const FieldInfo& field) {
    const uint8_t* fieldPtr = base + field.offset;
    AutoFree value;
    COLORREF c;

    switch (field.type) {
        case Type_Bool:
            out.Append(*(bool*)fieldPtr ? "true" : "false");
            return true;
        case Type_Int:
            out.AppendFmt("%d", *(int*)fieldPtr);
            return true;
        case Type_Float:
            out.AppendFmt("%g", *(float*)fieldPtr);
            return true;
        case Type_Color:
            c = *(COLORREF*)fieldPtr;
            SerializeColor(c, out);
            return true;
        case Type_String:
            if (!*(const WCHAR**)fieldPtr) {
                CrashIf(field.value);
                return false; // skip empty strings
            }
            {
                auto tmp = strconv::WstrToUtf8(*(const WCHAR**)fieldPtr);
                value.Set(tmp.data());
            }
            if (!NeedsEscaping(value)) {
                out.Append(value);
            } else {
                EscapeStr(out, value);
            }
            return true;
        case Type_Utf8String:
            if (!*(const char**)fieldPtr) {
                CrashIf(field.value);
                return false; // skip empty strings
            }
            if (!NeedsEscaping(*(const char**)fieldPtr))
                out.Append(*(const char**)fieldPtr);
            else
                EscapeStr(out, *(const char**)fieldPtr);
            return true;
        case Type_Compact:
            AssertCrash(IsCompactable(GetSubstruct(field)));
            for (size_t i = 0; i < GetSubstruct(field)->fieldCount; i++) {
                if (i > 0)
                    out.AppendChar(' ');
                SerializeField(out, fieldPtr, GetSubstruct(field)->fields[i]);
            }
            return true;
        case Type_ColorArray:
        case Type_FloatArray:
        case Type_IntArray:
            for (size_t i = 0; i < (*(Vec<int>**)fieldPtr)->size(); i++) {
                FieldInfo info = {0};
                info.type =
                    Type_IntArray == field.type ? Type_Int : Type_FloatArray == field.type ? Type_Float : Type_Color;
                if (i > 0)
                    out.AppendChar(' ');
                SerializeField(out, (const uint8_t*)&(*(Vec<int>**)fieldPtr)->at(i), info);
            }
            // prevent empty arrays from being replaced with the defaults
            return (*(Vec<int>**)fieldPtr)->size() > 0 || field.value != 0;
        case Type_StringArray:
            value.Set(SerializeStringArray(*(Vec<WCHAR*>**)fieldPtr));
            if (!NeedsEscaping(value))
                out.Append(value);
            else
                EscapeStr(out, value);
            // prevent empty arrays from being replaced with the defaults
            return (*(Vec<WCHAR*>**)fieldPtr)->size() > 0 || field.value != 0;
        default:
            CrashIf(true);
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

static void DeserializeField(const FieldInfo& field, uint8_t* base, const char* value) {
    uint8_t* fieldPtr = base + field.offset;

    char** strPtr = (char**)fieldPtr;
    WCHAR** wstrPtr = (WCHAR**)fieldPtr;
    COLORREF* colPtr = (COLORREF*)fieldPtr;
    bool* boolPtr = (bool*)fieldPtr;
    int* intPtr = (int*)fieldPtr;

    switch (field.type) {
        case Type_Bool:
            if (value) {
                *boolPtr = parseBool(value);
            } else {
                *boolPtr = field.value != 0;
            }
            break;

        case Type_Int:
            if (value) {
                *intPtr = ParseInt(value);
            } else {
                *intPtr = (int)field.value;
            }
            break;

        case Type_Float: {
            const char* s = value ? value : (const char*)field.value;
            str::Parse(s, "%f", (float*)fieldPtr);
            break;
        }

        case Type_Color:
            if (!value) {
                *colPtr = (COLORREF)field.value;
            } else {
                ParseColor(colPtr, value);
            }
            break;

        case Type_String:
            free(*wstrPtr);
            if (value) {
                AutoFree tmp = UnescapeStr(value);
                *wstrPtr = strconv::Utf8ToWstr(tmp.as_view());
            } else {
                *wstrPtr = str::Dup((const WCHAR*)field.value);
            }
            break;
        case Type_Utf8String:
            free(*strPtr);
            if (value)
                *strPtr = UnescapeStr(value);
            else
                *strPtr = str::Dup((const char*)field.value);
            break;
        case Type_Compact:
            AssertCrash(IsCompactable(GetSubstruct(field)));
            for (size_t i = 0; i < GetSubstruct(field)->fieldCount; i++) {
                if (value) {
                    for (; str::IsWs(*value); value++)
                        ;
                    if (!*value)
                        value = nullptr;
                }
                DeserializeField(GetSubstruct(field)->fields[i], fieldPtr, value);
                if (value)
                    for (; *value && !str::IsWs(*value); value++)
                        ;
            }
            break;
        case Type_ColorArray:
        case Type_FloatArray:
        case Type_IntArray:
            if (!value)
                value = (const char*)field.value;
            delete *(Vec<int>**)fieldPtr;
            *(Vec<int>**)fieldPtr = new Vec<int>();
            while (value && *value) {
                FieldInfo info = {0};
                info.type =
                    Type_IntArray == field.type ? Type_Int : Type_FloatArray == field.type ? Type_Float : Type_Color;
                DeserializeField(info, (uint8_t*)(*(Vec<int>**)fieldPtr)->AppendBlanks(1), value);
                for (; *value && !str::IsWs(*value); value++)
                    ;
                for (; str::IsWs(*value); value++)
                    ;
            }
            break;
        case Type_StringArray:
            FreeStringArray(*(Vec<WCHAR*>**)fieldPtr);
            *(Vec<WCHAR*>**)fieldPtr = new Vec<WCHAR*>();
            if (value)
                DeserializeStringArray(*(Vec<WCHAR*>**)fieldPtr, AutoFree(UnescapeStr(value)));
            else if (field.value)
                DeserializeStringArray(*(Vec<WCHAR*>**)fieldPtr, (const char*)field.value);
            break;
        default:
            CrashIf(true);
    }
}

static inline void Indent(str::Str& out, int indent) {
    while (indent-- > 0)
        out.AppendChar('\t');
}

static void MarkFieldKnown(SquareTreeNode* node, const char* fieldName, SettingType type) {
    if (!node)
        return;
    size_t off = 0;
    if (Type_Struct == type || Type_Prerelease == type) {
        if (node->GetChild(fieldName, &off)) {
            delete node->data.at(off - 1).value.child;
            node->data.RemoveAt(off - 1);
        }
    } else if (Type_Array == type) {
        while (node->GetChild(fieldName, &off)) {
            delete node->data.at(off - 1).value.child;
            node->data.RemoveAt(off - 1);
            off--;
        }
    } else if (node->GetValue(fieldName, &off)) {
        node->data.RemoveAt(off - 1);
    }
}

static void SerializeUnknownFields(str::Str& out, SquareTreeNode* node, int indent) {
    if (!node)
        return;
    for (size_t i = 0; i < node->data.size(); i++) {
        SquareTreeNode::DataItem& item = node->data.at(i);
        Indent(out, indent);
        out.Append(item.key);
        if (item.isChild) {
            out.Append(" [\r\n");
            SerializeUnknownFields(out, item.value.child, indent + 1);
            Indent(out, indent);
            out.Append("]\r\n");
        } else {
            out.Append(" = ");
            out.Append(item.value.str);
            out.Append("\r\n");
        }
    }
}

static void SerializeStructRec(str::Str& out, const StructInfo* info, const void* data, SquareTreeNode* prevNode,
                               int indent = 0) {
    const uint8_t* base = (const uint8_t*)data;
    const char* fieldName = info->fieldNames;
    for (size_t i = 0; i < info->fieldCount; i++, fieldName += str::Len(fieldName) + 1) {
        const FieldInfo& field = info->fields[i];
        CrashIf(str::FindChar(fieldName, '=') || str::FindChar(fieldName, ':') || str::FindChar(fieldName, '[') ||
                str::FindChar(fieldName, ']') || NeedsEscaping(fieldName));
        if (Type_Struct == field.type || Type_Prerelease == field.type) {
#if !(defined(PRE_RELEASE_VER) || defined(DEBUG))
            if (Type_Prerelease == field.type)
                continue;
#endif
            Indent(out, indent);
            out.Append(fieldName);
            out.Append(" [\r\n");
            SerializeStructRec(out, GetSubstruct(field), base + field.offset,
                               prevNode ? prevNode->GetChild(fieldName) : nullptr, indent + 1);
            Indent(out, indent);
            out.Append("]\r\n");
        } else if (Type_Array == field.type) {
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
        } else if (Type_Comment == field.type) {
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
            if (keep)
                out.Append("\r\n");
            else
                out.RemoveAt(offset, out.size() - offset);
        }
        MarkFieldKnown(prevNode, fieldName, field.type);
    }
    SerializeUnknownFields(out, prevNode, indent);
}

static void* DeserializeStructRec(const StructInfo* info, SquareTreeNode* node, uint8_t* base, bool useDefaults) {
    if (!base)
        base = AllocArray<uint8_t>(info->structSize);

    const char* fieldName = info->fieldNames;
    for (size_t i = 0; i < info->fieldCount; i++, fieldName += str::Len(fieldName) + 1) {
        const FieldInfo& field = info->fields[i];
        uint8_t* fieldPtr = base + field.offset;
        if (Type_Struct == field.type || Type_Prerelease == field.type) {
            SquareTreeNode* child = node ? node->GetChild(fieldName) : nullptr;
#if !(defined(PRE_RELEASE_VER) || defined(DEBUG))
            if (Type_Prerelease == field.type)
                child = nullptr;
#endif
            DeserializeStructRec(GetSubstruct(field), child, fieldPtr, useDefaults);
        } else if (Type_Array == field.type) {
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
                    array->Append(DeserializeStructRec(GetSubstruct(field), child, nullptr, true));
                }
                FreeArray(*(Vec<void*>**)fieldPtr, field);
                *(Vec<void*>**)fieldPtr = array;
            }
        } else if (field.type != Type_Comment) {
            const char* value = node ? node->GetValue(fieldName) : nullptr;
            if (useDefaults || value)
                DeserializeField(field, base, value);
        }
    }
    return base;
}

char* SerializeStruct(const StructInfo* info, const void* strct, const char* prevData, size_t* sizeOut) {
    str::Str out;
    out.Append(UTF8_BOM);
    SquareTree prevSqt(prevData);
    SerializeStructRec(out, info, strct, prevSqt.root);
    if (sizeOut)
        *sizeOut = out.size();
    return out.StealData();
}

void* DeserializeStruct(const StructInfo* info, const char* data, void* strct) {
    SquareTree sqt(data);
    return DeserializeStructRec(info, sqt.root, (uint8_t*)strct, !strct);
}

static void FreeStructData(const StructInfo* info, uint8_t* base) {
    for (size_t i = 0; i < info->fieldCount; i++) {
        const FieldInfo& field = info->fields[i];
        uint8_t* fieldPtr = base + field.offset;
        if (Type_Struct == field.type || Type_Prerelease == field.type) {
            FreeStructData(GetSubstruct(field), fieldPtr);
        } else if (Type_Array == field.type) {
            FreeArray(*(Vec<void*>**)fieldPtr, field);
        } else if (Type_String == field.type || Type_Utf8String == field.type) {
            void* m = *((void**)fieldPtr);
            free(m);
        } else if (Type_ColorArray == field.type || Type_FloatArray == field.type || Type_IntArray == field.type) {
            Vec<int>* v = *((Vec<int>**)fieldPtr);
            delete v;
        } else if (Type_StringArray == field.type) {
            FreeStringArray(*(Vec<WCHAR*>**)fieldPtr);
        }
    }
}

void FreeStruct(const StructInfo* info, void* strct) {
    if (strct)
        FreeStructData(info, (uint8_t*)strct);
    free(strct);
}
