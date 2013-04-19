/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "SettingsUtil.h"
#include "SquareTreeParser.h"

static inline const StructInfo *GetSubstruct(const FieldInfo& field)
{
    return (const StructInfo *)field.value;
}

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

// only escape characters which are significant to SquareTreeParser:
// newlines and leading/trailing whitespace (and escape characters)
static bool NeedsEscaping(const char *s)
{
    return str::IsWs(*s) || *s && str::IsWs(*(s + str::Len(s) - 1)) ||
           str::FindChar(s, '\n') || str::FindChar(s, '\r') || str::FindChar(s, '$');
}

static void EscapeStr(str::Str<char>& out, const char *s)
{
    CrashIf(!NeedsEscaping(s));
    if (str::IsWs(*s) && *s != '\n' && *s != '\r')
        out.Append('$');
    for (const char *c = s; *c; c++) {
        switch (*c) {
        case '$': out.Append("$$"); break;
        case '\n': out.Append("$n"); break;
        case '\r': out.Append("$r"); break;
        default: out.Append(*c);
        }
    }
    if (*s && str::IsWs(s[str::Len(s) - 1]))
        out.Append('$');
}

static char *UnescapeStr(const char *s)
{
    if (!str::FindChar(s, '$'))
        return str::Dup(s);

    str::Str<char> ret;
    const char *end = s + str::Len(s);
    if ('$' == *s && str::IsWs(*(s + 1)))
        s++; // leading whitespace
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
            // keep all other instances of the escape character
            ret.Append('$');
            ret.Append(*c);
            break;
        }
    }
    return ret.StealData();
}

static void FreeArray(Vec<void *> *array, const FieldInfo& field)
{
    for (size_t j = 0; array && j < array->Count(); j++) {
        FreeStruct(GetSubstruct(field), array->At(j));
    }
    delete array;
}

#ifndef NDEBUG
static bool IsCompactable(const StructInfo *info)
{
    for (size_t i = 0; i < info->fieldCount; i++) {
        switch (info->fields[i].type) {
        case Type_Bool: case Type_Int: case Type_Float: case Type_Color:
            continue;
        default:
            return false;
        }
    }
    return info->fieldCount > 0;
}
#endif

STATIC_ASSERT(sizeof(float) == sizeof(int) && sizeof(COLORREF) == sizeof(int), can_simplify_compact_array_code);

static bool SerializeField(str::Str<char>& out, const uint8_t *base, const FieldInfo& field)
{
    const uint8_t *fieldPtr = base + field.offset;
    ScopedMem<char> value;
    COLORREF c;

    switch (field.type) {
    case Type_Bool:
        out.Append(*(bool *)fieldPtr ? "true" : "false");
        return true;
    case Type_Int:
        out.AppendFmt("%d", *(int *)fieldPtr);
        return true;
    case Type_Float:
        out.AppendFmt("%g", *(float *)fieldPtr);
        return true;
    case Type_Color:
        c = *(COLORREF *)fieldPtr;
        if (((c >> 24) & 0xff))
            out.AppendFmt("#%02x%02x%02x%02x", (c >> 24) & 0xff, GetRValue(c), GetGValue(c), GetBValue(c));
        else
            out.AppendFmt("#%02x%02x%02x", GetRValue(c), GetGValue(c), GetBValue(c));
        return true;
    case Type_String:
        if (!*(const WCHAR **)fieldPtr) {
            CrashIf(field.value);
            return false; // skip empty strings
        }
        value.Set(str::conv::ToUtf8(*(const WCHAR **)fieldPtr));
        if (!NeedsEscaping(value))
            out.Append(value);
        else
            EscapeStr(out, value);
        return true;
    case Type_Utf8String:
        if (!*(const char **)fieldPtr) {
            CrashIf(field.value);
            return false; // skip empty strings
        }
        if (!NeedsEscaping(*(const char **)fieldPtr))
            out.Append(*(const char **)fieldPtr);
        else
            EscapeStr(out, *(const char **)fieldPtr);
        return true;
    case Type_Compact:
        assert(IsCompactable(GetSubstruct(field)));
        for (size_t i = 0; i < GetSubstruct(field)->fieldCount; i++) {
            if (i > 0)
                out.Append(' ');
            SerializeField(out, fieldPtr, GetSubstruct(field)->fields[i]);
        }
        return true;
    case Type_ColorArray:
    case Type_FloatArray:
    case Type_IntArray:
        for (size_t i = 0; i < (*(Vec<int> **)fieldPtr)->Count(); i++) {
            FieldInfo info = { 0 };
            info.type = Type_IntArray == field.type ? Type_Int : Type_FloatArray == field.type ? Type_Float : Type_Color;
            if (i > 0)
                out.Append(' ');
            SerializeField(out, (const uint8_t *)&(*(Vec<int> **)fieldPtr)->At(i), info);
        }
        // prevent empty arrays from being replaced with the defaults
        return (*(Vec<int> **)fieldPtr)->Count() > 0 || field.value != NULL;
    default:
        CrashIf(true);
        return false;
    }
}

static void DeserializeField(const FieldInfo& field, uint8_t *base, const char *value)
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
        str::Parse(value ? value : (const char *)field.value, "%f", (float *)fieldPtr);
        break;
    case Type_Color:
        if (!value)
            *(COLORREF *)fieldPtr = (COLORREF)field.value;
        else if (str::Parse(value, "#%2x%2x%2x%2x", &a, &r, &g, &b))
            *(COLORREF *)fieldPtr = RGB(r, g, b) | (a << 24);
        else if (str::Parse(value, "#%2x%2x%2x", &r, &g, &b))
            *(COLORREF *)fieldPtr = RGB(r, g, b);
        break;
    case Type_String:
        free(*(WCHAR **)fieldPtr);
        if (value)
            *(WCHAR **)fieldPtr = str::conv::FromUtf8(ScopedMem<char>(UnescapeStr(value)));
        else
            *(WCHAR **)fieldPtr = str::Dup((const WCHAR *)field.value);
        break;
    case Type_Utf8String:
        free(*(char **)fieldPtr);
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
            DeserializeField(GetSubstruct(field)->fields[i], fieldPtr, value);
            if (value)
                for (; *value && !str::IsWs(*value); value++);
        }
        break;
    case Type_ColorArray:
    case Type_FloatArray:
    case Type_IntArray:
        if (!value)
            value = (const char *)field.value;
        delete *(Vec<int> **)fieldPtr;
        *(Vec<int> **)fieldPtr = new Vec<int>();
        while (value && *value) {
            FieldInfo info = { 0 };
            info.type = Type_IntArray == field.type ? Type_Int : Type_FloatArray == field.type ? Type_Float : Type_Color;
            DeserializeField(info, (uint8_t *)(*(Vec<int> **)fieldPtr)->AppendBlanks(1), value);
            for (; *value && !str::IsWs(*value); value++);
            for (; str::IsWs(*value); value++);
        }
        break;
    default:
        CrashIf(true);
    }
}

static inline void Indent(str::Str<char>& out, int indent)
{
    while (indent-- > 0)
        out.Append('\t');
}

static void MarkFieldKnown(SquareTreeNode *node, const char *fieldName, SettingType type)
{
    if (!node)
        return;
    size_t off = 0;
    if (Type_Struct == type) {
        if (node->GetChild(fieldName, &off)) {
            delete node->data.At(off - 1).value.child;
            node->data.RemoveAt(off - 1);
        }
    }
    else if (Type_Array == type) {
        while (node->GetChild(fieldName, &off)) {
            delete node->data.At(off - 1).value.child;
            node->data.RemoveAt(off - 1);
            off--;
        }
    }
    else if (node->GetValue(fieldName, &off)) {
        node->data.RemoveAt(off - 1);
    }
}

static void SerializeUnknownFields(str::Str<char>& out, SquareTreeNode *node, int indent)
{
    if (!node)
        return;
    for (size_t i = 0; i < node->data.Count(); i++) {
        SquareTreeNode::DataItem& item = node->data.At(i);
        Indent(out, indent);
        out.Append(item.key);
        if (item.isChild) {
            out.Append(" [\r\n");
            SerializeUnknownFields(out, item.value.child, indent + 1);
            Indent(out, indent);
            out.Append("]\r\n");
        }
        else {
            out.Append(" = ");
            out.Append(item.value.str);
            out.Append("\r\n");
        }
    }
}

static void SerializeStructRec(str::Str<char>& out, const StructInfo *info, const void *data, SquareTreeNode *prevNode, int indent=0)
{
    const uint8_t *base = (const uint8_t *)data;
    const char *fieldName = info->fieldNames;
    for (size_t i = 0; i < info->fieldCount; i++) {
        const FieldInfo& field = info->fields[i];
        CrashIf(str::FindChar(fieldName, '=') || str::FindChar(fieldName, ':') ||
                str::FindChar(fieldName, '[') || str::FindChar(fieldName, ']') ||
                NeedsEscaping(fieldName));
        if (Type_Struct == field.type) {
            // TODO: insert empty line for readability?
            Indent(out, indent);
            out.Append(fieldName);
            out.Append(" [\r\n");
            SerializeStructRec(out, GetSubstruct(field), base + field.offset, prevNode ? prevNode->GetChild(fieldName) : NULL, indent + 1);
            Indent(out, indent);
            out.Append("]\r\n");
        }
        else if (Type_Array == field.type) {
            Indent(out, indent);
            out.Append(fieldName);
            out.Append(" [\r\n");
            Vec<void *> *array = *(Vec<void *> **)(base + field.offset);
            if (array && array->Count() > 0) {
                for (size_t j = 0; j < array->Count(); j++) {
                    Indent(out, indent + 1);
                    out.Append("[\r\n");
                    SerializeStructRec(out, GetSubstruct(field), array->At(j), NULL, indent + 2);
                    Indent(out, indent + 1);
                    out.Append("]\r\n");
                }
            }
            Indent(out, indent);
            out.Append("]\r\n");
        }
        else {
            size_t offset = out.Size();
            Indent(out, indent);
            out.Append(fieldName);
            out.Append(" = ");
            bool keep = SerializeField(out, base, field);
            if (keep)
                out.Append("\r\n");
            else
                out.RemoveAt(offset, out.Size() - offset);
        }
        MarkFieldKnown(prevNode, fieldName, field.type);
        fieldName += str::Len(fieldName) + 1;
    }
    SerializeUnknownFields(out, prevNode, indent);
}

static void *DeserializeStructRec(const StructInfo *info, SquareTreeNode *node, uint8_t *base, bool useDefaults)
{
    if (!base)
        base = AllocArray<uint8_t>(info->structSize);

    const char *fieldName = info->fieldNames;
    for (size_t i = 0; i < info->fieldCount; i++) {
        const FieldInfo& field = info->fields[i];
        uint8_t *fieldPtr = base + field.offset;
        if (Type_Struct == field.type) {
            SquareTreeNode *child = node ? node->GetChild(fieldName) : NULL;
            DeserializeStructRec(GetSubstruct(field), child, fieldPtr, useDefaults);
        }
        else if (Type_Array == field.type) {
            SquareTreeNode *parent = node, *child = NULL;
            if (parent && (child = parent->GetChild(fieldName)) != NULL &&
                (0 == child->data.Count() || child->GetChild(""))) {
                parent = child;
                fieldName += str::Len(fieldName);
            }
            if (child || useDefaults || !*(Vec<void *> **)fieldPtr) {
                Vec<void *> *array = new Vec<void *>();
                size_t idx = 0;
                while (parent && (child = parent->GetChild(fieldName, &idx)) != NULL) {
                    array->Append(DeserializeStructRec(GetSubstruct(field), child, NULL, true));
                }
                FreeArray(*(Vec<void *> **)fieldPtr, field);
                *(Vec<void *> **)fieldPtr = array;
            }
        }
        else {
            const char *value = node ? node->GetValue(fieldName) : NULL;
            if (useDefaults || value)
                DeserializeField(field, base, value);
        }
        fieldName += str::Len(fieldName) + 1;
    }
    return base;
}

char *SerializeStruct(const StructInfo *info, const void *strct, const char *prevData,
                      const char *infoUrl, size_t *sizeOut)
{
    str::Str<char> out;
    if (infoUrl)
        out.AppendFmt(UTF8_BOM "# see %s\nfor documentation\r\n\r\n", infoUrl);
    else
        out.Append(UTF8_BOM "# This file will be overwritten - modify at your own risk!\r\n\r\n");
    SquareTree prevSqt(prevData);
    SerializeStructRec(out, info, strct, prevSqt.root);
    if (sizeOut)
        *sizeOut = out.Size();
    return out.StealData();
}

void *DeserializeStruct(const StructInfo *info, const char *data, void *strct)
{
    SquareTree sqt(data);
    return DeserializeStructRec(info, sqt.root, (uint8_t *)strct, !strct);
}

static void FreeStructData(const StructInfo *info, uint8_t *base)
{
    for (size_t i = 0; i < info->fieldCount; i++) {
        const FieldInfo& field = info->fields[i];
        uint8_t *fieldPtr = base + field.offset;
        if (Type_Struct == field.type)
            FreeStructData(GetSubstruct(field), fieldPtr);
        else if (Type_Array == field.type)
            FreeArray(*(Vec<void *> **)fieldPtr, field);
        else if (Type_String == field.type || Type_Utf8String == field.type)
            free(*(void **)fieldPtr);
        else if (Type_ColorArray == field.type || Type_FloatArray == field.type || Type_IntArray == field.type)
            delete *(Vec<int> **)fieldPtr;
    }
}

void FreeStruct(const StructInfo *info, void *strct)
{
    if (strct)
        FreeStructData(info, (uint8_t *)strct);
    free(strct);
}

// TODO: keep Benc deserialization for at least two minor releases (ideally at least a year)

#include "BencUtil.h"

static void *DeserializeStructBencRec(const StructInfo *info, BencDict *dict, uint8_t *base, CompactCallback cb)
{
    if (!base)
        base = AllocArray<uint8_t>(info->structSize);

    const char *fieldName = info->fieldNames;
    for (size_t i = 0; i < info->fieldCount; i++) {
        const FieldInfo& field = info->fields[i];
        uint8_t *fieldPtr = base + field.offset;
        if (Type_Struct == field.type) {
            BencDict *child = dict ? dict->GetDict(fieldName) : NULL;
            DeserializeStructBencRec(GetSubstruct(field), child, fieldPtr, cb);
        }
        else if (Type_Array == field.type) {
            Vec<void *> *array = new Vec<void *>();
            BencArray *list = dict ? dict->GetArray(fieldName) : NULL;
            for (size_t j = 0; list && j < list->Length(); j++) {
                array->Append(DeserializeStructBencRec(GetSubstruct(field), list->GetDict(j), NULL, cb));
            }
            FreeArray(*(Vec<void *> **)fieldPtr, field);
            *(Vec<void *> **)fieldPtr = array;
        }
        else if (Type_Bool == field.type) {
            BencInt *val = dict ? dict->GetInt(fieldName) : NULL;
            *(bool *)fieldPtr = (val ? val->Value() : field.value) != 0;
        }
        else if (Type_Int == field.type) {
            BencInt *val = dict ? dict->GetInt(fieldName) : NULL;
            *(int *)fieldPtr = (int)(val ? val->Value() : field.value);
        }
        else if (Type_Float == field.type) {
            BencString *val = dict ? dict->GetString(fieldName) : NULL;
            if (!val || !str::Parse(val->RawValue(), "%f", (float *)fieldPtr))
                str::Parse((const char *)field.value, "%f", (float *)fieldPtr);
        }
        else if (Type_Color == field.type) {
            BencInt *val = dict ? dict->GetInt(fieldName) : NULL;
            *(COLORREF *)fieldPtr = (COLORREF)(val ? val->Value() : field.value);
        }
        else if (Type_String == field.type) {
            BencString *val = dict ? dict->GetString(fieldName) : NULL;
            free(*(WCHAR **)fieldPtr);
            *(WCHAR **)fieldPtr = val ? val->Value() : str::Dup((const WCHAR *)field.value);
        }
        else if (Type_Utf8String == field.type) {
            BencString *val = dict ? dict->GetString(fieldName) : NULL;
            free(*(char **)fieldPtr);
            *(char **)fieldPtr = str::Dup(val ? val->RawValue() : (const char *)field.value);
        }
        else if (Type_IntArray == field.type) {
            CrashIf(field.value);
            Vec<int> *vec = new Vec<int>();
            BencArray *val = dict ? dict->GetArray(fieldName) : NULL;
            for (size_t j = 0; val && j < val->Length(); j++) {
                BencInt *val2 = val->GetInt(j);
                vec->Append(val2 ? (int)val2->Value() : -1);
            }
            delete *(Vec<int> **)fieldPtr;
            *(Vec<int> **)fieldPtr = vec;
        }
        else if (Type_Compact == field.type) {
            bool ok = cb && cb(dict, &field, fieldName, fieldPtr);
            CrashIf(!ok);
        }
        else {
            CrashIf(true);
        }
        fieldName += str::Len(fieldName) + 1;
    }
    return base;
}

void *DeserializeStructBenc(const StructInfo *info, const char *data, void *strct, CompactCallback cb)
{
    void *result = NULL;
    BencObj *root = BencObj::Decode(data);
    if (root && BT_DICT == root->Type())
        result = DeserializeStructBencRec(info, static_cast<BencDict *>(root), (uint8_t *)strct, cb);
    delete root;
    return result;
}
