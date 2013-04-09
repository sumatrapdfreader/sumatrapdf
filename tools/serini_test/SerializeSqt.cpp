/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "SerializeSqt.h"

#define INCLUDE_APPPREFS3_STRUCTS
#include "AppPrefs3.h"
#include "SquareTreeParser.h"

namespace sqt {

static int ParseInt(const char *bytes)
{
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

static char *UnescapeStr(const char *s)
{
    if (!str::FindChar(s, '$'))
        return str::Dup(s);
    str::Str<char> ret;
    const char *end = s + str::Len(s);
    for (const char *c = s; c < end; c++) {
        if (*c != '$') {
            ret.Append(*c);
            continue;
        }
        switch (*++c) {
        case '$': ret.Append('$'); break;
        case 'n': ret.Append('\n'); break;
        case 'r': ret.Append('\r'); break;
        case '\0': break; // trailing whitespace
        default:
            if (!str::IsWs(*c))
                ret.Append('$');
            // else it's leading whitespace
            ret.Append(*c);
            break;
        }
    }
    return ret.StealData();
}

// only escape characters which are significant to SquareTreeParser:
// newlines and leading/trailing whitespace (and escape characters)
static bool NeedsEscaping(const char *s)
{
    return str::IsWs(*s) || *s && str::IsWs(*(s + str::Len(s) - 1)) ||
           str::FindChar(s, '\n') || str::FindChar(s, '\r') || str::FindChar(s, '$');
}

// escapes strings containing newlines or heading/trailing whitespace
static char *EscapeStr(const char *s)
{
    str::Str<char> ret;
    if (str::IsWs(*s))
        ret.Append("$");
    for (const char *c = s; *c; c++) {
        switch (*c) {
        case '$': ret.Append("$$"); break;
        case '\n': ret.Append("$n"); break;
        case '\r': ret.Append("$r"); break;
        default: ret.Append(*c);
        }
    }
    if (*s && str::IsWs(s[str::Len(s) - 1]))
        ret.Append("$");
    return ret.StealData();
}

#ifndef NDEBUG
static bool IsCompactable(const SettingInfo *meta)
{
    for (size_t i = 0; i < meta->fieldCount; i++) {
        switch (meta->fields[i].type) {
        case Type_Bool: case Type_Int: case Type_Float: case Type_Color:
            continue;
        default:
            return false;
        }
    }
    return meta->fieldCount > 0;
}
#endif

static void DeserializeField(uint8_t *base, const FieldInfo& field, const char *value)
{
    uint8_t *fieldPtr = base + field.offset;
    int r, g, b, a;

    switch (field.type) {
    case Type_Bool:
        *(bool *)fieldPtr = value ? str::StartsWithI(value, "true") && (!value[4] || str::IsWs(value[4])) || ParseInt(value) != 0 : field.value != 0;
        break;
    case Type_Int:
        *(int *)fieldPtr = value ? ParseInt(value) : (int)field.value;
        break;
    case Type_Float:
        if (!value || !str::Parse(value, "%f", (float *)fieldPtr))
            str::Parse((const char *)field.value, "%f", (float *)fieldPtr);
        break;
    case Type_Color:
        if (value && str::Parse(value, "#%2x%2x%2x%2x", &a, &r, &g, &b))
            *(COLORREF *)fieldPtr = RGB(r, g, b) | (a << 24);
        else if (value && str::Parse(value, "#%2x%2x%2x", &r, &g, &b))
            *(COLORREF *)fieldPtr = RGB(r, g, b);
        else
            *(COLORREF *)fieldPtr = (COLORREF)field.value;
        break;
    case Type_String:
        if (value)
            *(WCHAR **)fieldPtr = str::conv::FromUtf8(ScopedMem<char>(UnescapeStr(value)));
        else
            *(WCHAR **)fieldPtr = str::Dup((const WCHAR *)field.value);
        break;
    case Type_Utf8String:
        if (value)
            *(char **)fieldPtr = UnescapeStr(value);
        else
            *(char **)fieldPtr = str::Dup((const char *)field.value);
        break;
    case Type_Compact:
        assert(IsCompactable(GetSubstruct(field)));
        for (size_t i = 0; i < GetSubstruct(field)->fieldCount; i++) {
            if (value) {
                for (; str::IsWs(*value); value++);
                if (!*value)
                    value = NULL;
            }
            DeserializeField(fieldPtr, GetSubstruct(field)->fields[i], value);
            if (value)
                for (; *value && !str::IsWs(*value); value++);
        }
        break;
    case Type_ColorArray:
    case Type_FloatArray:
    case Type_IntArray:
        CrashIf(sizeof(float) != sizeof(int) || sizeof(COLORREF) != sizeof(int));
        if (!value)
            value = (const char *)field.value;
        *(Vec<int> **)fieldPtr = new Vec<int>();
        while (value && *value) {
            FieldInfo info = { 0 };
            info.type = Type_IntArray == field.type ? Type_Int : Type_FloatArray == field.type ? Type_Float : Type_Color;
            DeserializeField((uint8_t *)(*(Vec<int> **)fieldPtr)->AppendBlanks(1), info, value);
            for (; *value && !str::IsWs(*value); value++);
            for (; str::IsWs(*value); value++);
        }
        break;
    default:
        CrashIf(true);
    }
}

static void *DeserializeRec(SquareTreeNode *node, const SettingInfo *meta, uint8_t *base=NULL)
{
    if (!base)
        base = AllocArray<uint8_t>(meta->structSize);

    const char *fieldName = meta->fieldNames;
    for (size_t i = 0; i < meta->fieldCount; i++) {
        const FieldInfo& field = meta->fields[i];
        if (Type_Struct == field.type) {
            SquareTreeNode *child = node ? node->GetChild(fieldName) : NULL;
            DeserializeRec(child, GetSubstruct(field), base + field.offset);
        }
        else if (Type_Array == field.type) {
            Vec<void *> *array = new Vec<void *>();
            SquareTreeNode *child;
            size_t idx = 0;
            while (node && (child = node->GetChild(fieldName, &idx)) != NULL) {
                array->Append(DeserializeRec(child, GetSubstruct(field)));
            }
            *(Vec<void *> **)(base + field.offset) = array;
        }
        else {
            const char *value = node ? node->GetValue(fieldName) : NULL;
            DeserializeField(base, field, value);
        }
        fieldName += str::Len(fieldName) + 1;
    }
    return base;
}

void *Deserialize(const char *data, size_t dataLen, const SettingInfo *def)
{
    CrashIf(str::Len(data) != dataLen);
    SquareTree sqt(data);
    return DeserializeRec(sqt.root, def);
}

static char *SerializeField(const uint8_t *base, const FieldInfo& field)
{
    const uint8_t *fieldPtr = base + field.offset;
    ScopedMem<char> value;
    COLORREF c;

    switch (field.type) {
    // TODO: only write non-default values?
    case Type_Bool: return str::Dup(*(bool *)fieldPtr ? "true" : "false");
    case Type_Int: return str::Format("%d", *(int *)fieldPtr);
    case Type_Float: return str::Format("%g", *(float *)fieldPtr);
    case Type_Color:
        c = *(COLORREF *)fieldPtr;
        // TODO: COLORREF doesn't really have an alpha value
        if (((c >> 24) & 0xff))
            return str::Format("#%02x%02x%02x%02x", (c >> 24) & 0xff, GetRValue(c), GetGValue(c), GetBValue(c));
        return str::Format("#%02x%02x%02x", GetRValue(c), GetGValue(c), GetBValue(c));
    case Type_String:
        if (!*(const WCHAR **)fieldPtr)
            return NULL; // skip empty strings
        value.Set(str::conv::ToUtf8(*(const WCHAR **)fieldPtr));
        if (NeedsEscaping(value))
            return EscapeStr(value);
        return value.StealData();
    case Type_Utf8String:
        if (!*(const char **)fieldPtr)
            return NULL; // skip empty strings
        if (!NeedsEscaping(*(const char **)fieldPtr))
            return str::Dup(*(const char **)fieldPtr);
        return EscapeStr(*(const char **)fieldPtr);
    case Type_Compact:
        assert(IsCompactable(GetSubstruct(field)));
        for (size_t i = 0; i < GetSubstruct(field)->fieldCount; i++) {
            ScopedMem<char> val(SerializeField(fieldPtr, GetSubstruct(field)->fields[i]));
            if (!value)
                value.Set(val.StealData());
            else
                value.Set(str::Format("%s %s", value, val));
        }
        return value.StealData();
    case Type_ColorArray:
    case Type_FloatArray:
    case Type_IntArray:
        CrashIf(sizeof(float) != sizeof(int) || sizeof(COLORREF) != sizeof(int));
        for (size_t i = 0; i < (*(Vec<int> **)fieldPtr)->Count(); i++) {
            FieldInfo info = { 0 };
            info.type = Type_IntArray == field.type ? Type_Int : Type_FloatArray == field.type ? Type_Float : Type_Color;
            ScopedMem<char> val(SerializeField((const uint8_t *)&(*(Vec<int> **)fieldPtr)->At(i), info));
            if (!value)
                value.Set(val.StealData());
            else
                value.Set(str::Format("%s %s", value, val));
        }
        return value.StealData();
    default:
        CrashIf(true);
    }
    return NULL;
}

static inline void Indent(str::Str<char>& out, int indent)
{
    while (indent-- > 0)
        out.Append('\t');
}

static void SerializeRec(str::Str<char>& out, const void *data, const SettingInfo *meta, int indent=0)
{
    const uint8_t *base = (const uint8_t *)data;
    const char *fieldName = meta->fieldNames;
    for (size_t i = 0; i < meta->fieldCount; i++) {
        const FieldInfo& field = meta->fields[i];
        CrashIf(str::FindChar(fieldName, '=') || str::FindChar(fieldName, ':') ||
                str::FindChar(fieldName, '[') || str::FindChar(fieldName, ']') ||
                NeedsEscaping(fieldName));
        if (Type_Struct == field.type) {
            Indent(out, indent);
            out.Append(fieldName);
            out.Append(" [\r\n");
            SerializeRec(out, base + field.offset, GetSubstruct(field), indent + 1);
            Indent(out, indent);
            out.Append("]\r\n");
        }
        else if (Type_Array == field.type) {
            Vec<void *> *array = *(Vec<void *> **)(base + field.offset);
            if (array->Count() > 0) {
                Indent(out, indent);
                out.Append(fieldName);
                for (size_t j = 0; j < array->Count(); j++) {
                    out.Append(" [\r\n");
                    SerializeRec(out, array->At(j), GetSubstruct(field), indent + 1);
                    Indent(out, indent);
                    out.Append("]");
                }
                out.Append("\r\n");
            }
        }
        else {
            ScopedMem<char> value(SerializeField(base, field));
            if (value) {
                Indent(out, indent);
                out.Append(fieldName);
                out.Append(" = ");
                out.Append(value);
                out.Append("\r\n");
            }
        }
        fieldName += str::Len(fieldName) + 1;
    }
}

char *Serialize(const void *data, const SettingInfo *def, size_t *sizeOut, const char *comment)
{
    str::Str<char> out;
    if (comment) {
        out.Append(UTF8_BOM "# ");
        out.Append(comment);
        out.Append("\r\n");
    }
    else {
        out.Append(UTF8_BOM "# this file will be overwritten - modify at your own risk\r\n");
    }
    SerializeRec(out, data, def);
    if (sizeOut)
        *sizeOut = out.Size();
    return out.StealData();
}

static void FreeStructData(uint8_t *base, const SettingInfo *meta)
{
    for (size_t i = 0; i < meta->fieldCount; i++) {
        const FieldInfo& field = meta->fields[i];
        if (Type_Struct == field.type)
            FreeStructData(base + field.offset, GetSubstruct(field));
        else if (Type_Array == field.type) {
            Vec<void *> *array = *(Vec<void *> **)(base + field.offset);
            for (size_t j = 0; j < array->Count(); j++) {
                FreeStruct(array->At(j), GetSubstruct(field));
            }
            delete array;
        }
        else if (Type_String == field.type || Type_Utf8String == field.type)
            free(*(void **)(base + field.offset));
        else if (Type_ColorArray == field.type || Type_FloatArray == field.type || Type_IntArray == field.type) {
            CrashIf(sizeof(float) != sizeof(int) || sizeof(COLORREF) != sizeof(int));
            delete *(Vec<int> **)(base + field.offset);
        }
    }
}

void FreeStruct(void *data, const SettingInfo *meta)
{
    if (data)
        FreeStructData((uint8_t *)data, meta);
    free(data);
}

};
