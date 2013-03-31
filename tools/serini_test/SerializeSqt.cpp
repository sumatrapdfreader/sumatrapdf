/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "SerializeSqt.h"

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
    if (!str::StartsWith(s, "$[") || !str::EndsWith(s, "]$"))
        return str::Dup(s);
    str::Str<char> ret;
    const char *end = s + str::Len(s) - 2;
    for (const char *c = s + 2; c < end; c++) {
        if (*c != '$') {
            ret.Append(*c);
            continue;
        }
        switch (*++c) {
        case '$': ret.Append('$'); break;
        case 'n': ret.Append('\n'); break;
        case 'r': ret.Append('\r'); break;
        default: ret.Append('$'); ret.Append(*c); break;
        }
    }
    return ret.StealData();
}

#ifndef NDEBUG
static bool IsCompactable(SettingInfo *meta)
{
    for (size_t i = 1; i <= GetFieldCount(meta); i++) {
        switch (meta[i].type) {
        case Type_Bool: case Type_Int: case Type_Float: case Type_Color:
            continue;
        default:
            return false;
        }
    }
    return GetFieldCount(meta) > 0;
}
#endif

static void DeserializeField(uint8_t *base, SettingInfo& field, const char *value)
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
        for (size_t i = 1; i <= GetFieldCount(GetSubstruct(field)); i++) {
            if (value) {
                for (; str::IsWs(*value); value++);
                if (!*value)
                    value = NULL;
            }
            DeserializeField(fieldPtr, GetSubstruct(field)[i], value);
            if (value)
                for (; *value && !str::IsWs(*value); value++);
        }
        break;
    default:
        CrashIf(true);
    }
}

static void *DeserializeRec(SquareTreeNode *node, SettingInfo *meta, uint8_t *base=NULL)
{
    if (!base)
        base = AllocArray<uint8_t>(GetStructSize(meta));

    for (size_t i = 1; i <= GetFieldCount(meta); i++) {
        if (Type_Struct == meta[i].type) {
            SquareTreeNode *child = node ? node->GetChild(GetFieldName(meta, i)) : NULL;
            DeserializeRec(child, GetSubstruct(meta[i]), base + meta[i].offset);
        }
        else if (Type_Array == meta[i].type) {
            Vec<void *> *array = new Vec<void *>();
            SquareTreeNode *child;
            for (size_t j = 0; node && (child = node->GetChild(GetFieldName(meta, i), j)) != NULL; j++) {
                array->Append(DeserializeRec(child, GetSubstruct(meta[i])));
            }
            *(Vec<void *> **)(base + meta[i].offset) = array;
        }
        else {
            const char *value = node ? node->GetValue(GetFieldName(meta, i)) : NULL;
            DeserializeField(base, meta[i], value);
        }
    }
    return base;
}

void *Deserialize(const char *data, size_t dataLen, SettingInfo *def)
{
    CrashIf(str::Len(data) != dataLen);
    SquareTree sqt(data);
    return DeserializeRec(sqt.root, def);
}

// only escape characters which are significant to SquareTreeParser:
// newlines, heading/trailing whitespace and single square brackets
static bool NeedsEscaping(const char *s)
{
    return str::IsWs(*s) || *s && str::IsWs(*(s + str::Len(s) - 1)) ||
           str::FindChar(s, '\n') || str::FindChar(s, '\r') ||
           str::Eq(s, "[") || str::Eq(s, "]");
}

// escapes strings containing newlines or heading/trailing whitespace
static char *EscapeStr(const char *s)
{
    str::Str<char> ret;
    // use an unlikely character combination for indicating an escaped string
    ret.Append("$[");
    for (const char *c = s; *c; c++) {
        switch (*c) {
        // TODO: escape any other characters?
        case '$': ret.Append("$$"); break;
        case '\n': ret.Append("$n"); break;
        case '\r': ret.Append("$r"); break;
        default: ret.Append(*c);
        }
    }
    ret.Append("]$");
    return ret.StealData();
}

static char *SerializeField(const uint8_t *base, SettingInfo& field)
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
        for (size_t i = 1; i <= GetFieldCount(GetSubstruct(field)); i++) {
            ScopedMem<char> val(SerializeField(fieldPtr, GetSubstruct(field)[i]));
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

static void SerializeRec(str::Str<char>& out, const void *data, SettingInfo *meta, int indent=0)
{
    const uint8_t *base = (const uint8_t *)data;
    for (size_t i = 1; i <= GetFieldCount(meta); i++) {
        CrashIf(str::FindChar(GetFieldName(meta, i), '=') || str::FindChar(GetFieldName(meta, i), ':') ||
                str::FindChar(GetFieldName(meta, i), '[') || str::FindChar(GetFieldName(meta, i), ']') ||
                NeedsEscaping(GetFieldName(meta, i)));
        if (Type_Struct == meta[i].type) {
            Indent(out, indent);
            out.Append(GetFieldName(meta, i));
            out.Append(" [\r\n");
            SerializeRec(out, base + meta[i].offset, GetSubstruct(meta[i]), indent + 1);
            Indent(out, indent);
            out.Append("]\r\n");
            continue;
        }
        if (Type_Array == meta[i].type) {
            Vec<void *> *array = *(Vec<void *> **)(base + meta[i].offset);
            if (array->Count() == 0)
                continue;
            Indent(out, indent);
            out.Append(GetFieldName(meta, i));
            for (size_t j = 0; j < array->Count(); j++) {
                out.Append(" [\r\n");
                SerializeRec(out, array->At(j), GetSubstruct(meta[i]), indent + 1);
                Indent(out, indent);
                out.Append("]");
            }
            out.Append("\r\n");
            continue;
        }
        ScopedMem<char> value(SerializeField(base, meta[i]));
        if (value) {
            Indent(out, indent);
            out.Append(GetFieldName(meta, i));
            out.Append(" = ");
            out.Append(value);
            out.Append("\r\n");
        }
    }
}

char *Serialize(const void *data, SettingInfo *def, size_t *sizeOut, const char *comment)
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

static void FreeStructData(uint8_t *base, SettingInfo *meta)
{
    for (size_t i = 1; i <= GetFieldCount(meta); i++) {
        if (Type_Struct == meta[i].type)
            FreeStructData(base + meta[i].offset, GetSubstruct(meta[i]));
        else if (Type_Array == meta[i].type) {
            Vec<void *> *array = *(Vec<void *> **)(base + meta[i].offset);
            for (size_t j = 0; j < array->Count(); j++) {
                FreeStruct(array->At(j), GetSubstruct(meta[i]));
            }
            delete array;
        }
        else if (Type_String == meta[i].type || Type_Utf8String == meta[i].type)
            free(*(void **)(base + meta[i].offset));
    }
}

void FreeStruct(void *data, SettingInfo *meta)
{
    if (data)
        FreeStructData((uint8_t *)data, meta);
    free(data);
}

};
