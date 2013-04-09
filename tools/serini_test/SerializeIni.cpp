/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "SerializeIni.h"

#include "IniParser.h"
#include "SquareTreeParser.h"

#include "../../src/utils/StrSlice.cpp"
#include "../../src/utils/TxtParser.cpp"

// undefine to prevent code for that format to be linked
#define ENABLE_FORMAT_INI
#define ENABLE_FORMAT_SQT
#define ENABLE_FORMAT_TXT
#define ENABLE_FORMAT_TXT_SQT
#define ENABLE_FORMAT_TXT2

namespace sertxt {

static int64_t ParseBencInt(const char *bytes)
{
    bool negative = *bytes == '-';
    if (negative)
        bytes++;
    int64_t value = 0;
    for (; str::IsDigit(*bytes); bytes++) {
        value = value * 10 + (*bytes - '0');
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

// only escape characters which are significant to IniParser/SquareTreeParser:
// newlines and heading/trailing whitespace (and escape sequence delimiters)
static bool NeedsEscaping(const char *s)
{
    return str::IsWs(*s) || *s && str::IsWs(*(s + str::Len(s) - 1)) ||
           str::FindChar(s, '\n') || str::FindChar(s, '\r') ||
           str::StartsWith(s, "$[") && str::EndsWith(s, "]$");
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

static const StructMetadata *GetStructDef(const FieldMetadata& fieldDef)
{
    CrashIf(!fieldDef.defValOrDefinition);
    return (const StructMetadata *)fieldDef.defValOrDefinition;
}

static void FreeListNode(Vec<void *> *list, const StructMetadata *def)
{
    if (!list)
        return;
    for (size_t i = 0; i < list->Count(); i++)
        FreeStruct((uint8_t *)list->At(i), def);
    delete list;
}

static void DeserializeField(uint8_t *data, const FieldMetadata& field, const char *value)
{
    uint8_t *fieldPtr = data + field.offset;
    int r, g, b, a;

    switch (field.type | 0) {
    case TYPE_BOOL:
        *(bool *)fieldPtr = value ? str::StartsWithI(value, "true") && (!value[4] || str::IsWs(value[4])) || ParseBencInt(value) != 0 : field.defValOrDefinition != 0;
        break;
    // TODO: are all these int-types really needed?
    case TYPE_I16:
        *(int16_t *)fieldPtr = (int16_t)(value ? ParseBencInt(value) : field.defValOrDefinition);
        break;
    case TYPE_U16:
        *(uint16_t *)fieldPtr = (uint16_t)(value ? ParseBencInt(value) : field.defValOrDefinition);
        break;
    case TYPE_I32:
        *(int32_t *)fieldPtr = (int32_t)(value ? ParseBencInt(value) : field.defValOrDefinition);
        break;
    case TYPE_U32:
        *(uint32_t *)fieldPtr = (uint32_t)(value ? ParseBencInt(value) : field.defValOrDefinition);
        break;
    case TYPE_U64:
        *(uint64_t *)fieldPtr = (uint64_t)(value ? ParseBencInt(value) : field.defValOrDefinition);
        break;
    case TYPE_FLOAT:
        if (!value || !str::Parse(value, "%f", (float *)fieldPtr))
            str::Parse((const char *)field.defValOrDefinition, "%f", (float *)fieldPtr);
        break;
    case TYPE_COLOR:
        if (value && str::Parse(value, "#%2x%2x%2x%2x", &a, &r, &g, &b))
            *(COLORREF *)fieldPtr = RGB(r, g, b) | (a << 24);
        else if (value && str::Parse(value, "#%2x%2x%2x", &r, &g, &b))
            *(COLORREF *)fieldPtr = RGB(r, g, b);
        else
            *(COLORREF *)fieldPtr = (COLORREF)field.defValOrDefinition;
        break;
    case TYPE_STR:
        free(*(char **)fieldPtr);
        if (value)
            *(char **)fieldPtr = UnescapeStr(value);
        else
            *(char **)fieldPtr = str::Dup((const char *)field.defValOrDefinition);
        break;
    case TYPE_WSTR:
        free(*(WCHAR **)fieldPtr);
        if (value)
            *(WCHAR **)fieldPtr = str::conv::FromUtf8(ScopedMem<char>(UnescapeStr(value)));
        else
            *(WCHAR **)fieldPtr = str::Dup((const WCHAR *)field.defValOrDefinition);
        break;
    case (TYPE_STRUCT_PTR | TYPE_STORE_COMPACT_MASK):
        if (!*(uint8_t **)fieldPtr)
            *(uint8_t **)fieldPtr = AllocArray<uint8_t>(GetStructDef(field)->size);
        for (size_t i = 0; i < GetStructDef(field)->nFields; i++) {
            if (value) {
                for (; str::IsWs(*value); value++);
                if (!*value)
                    value = NULL;
            }
            DeserializeField(*(uint8_t **)fieldPtr, GetStructDef(field)->fields[i], value);
            CrashIf(TYPE_STR == GetStructDef(field)->fields[i].type || TYPE_WSTR == GetStructDef(field)->fields[i].type);
            if (value)
                for (; *value && !str::IsWs(*value); value++);
        }
        break;
    default:
        CrashIf(!(field.type & TYPE_NO_STORE_MASK));
    }
}

static char *SerializeField(const uint8_t *data, const FieldMetadata& field)
{
    const uint8_t *fieldPtr = data + field.offset;
    ScopedMem<char> value;
    COLORREF c;

    switch (field.type | 0) {
    case TYPE_BOOL: return str::Dup(*(bool *)fieldPtr ? "true" : "false");
    case TYPE_I16: return str::Format("%d", (int32_t)*(int16_t *)fieldPtr);
    case TYPE_U16: return str::Format("%u", (uint32_t)*(uint16_t *)fieldPtr);
    case TYPE_I32: return str::Format("%d", *(int32_t *)fieldPtr);
    case TYPE_U32: return str::Format("%u", *(uint32_t *)fieldPtr);
    case TYPE_U64: return str::Format("%I64u", *(uint64_t *)fieldPtr);
    case TYPE_FLOAT: return str::Format("%g", *(float *)fieldPtr);
    case TYPE_COLOR:
        c = *(COLORREF *)fieldPtr;
        // TODO: COLORREF doesn't really have an alpha value
        if (((c >> 24) & 0xff))
            return str::Format("#%02x%02x%02x%02x", (c >> 24) & 0xff, GetRValue(c), GetGValue(c), GetBValue(c));
        return str::Format("#%02x%02x%02x", GetRValue(c), GetGValue(c), GetBValue(c));
    case TYPE_STR:
        if (!*(const char **)fieldPtr)
            return NULL; // skip empty strings
        if (!NeedsEscaping(*(const char **)fieldPtr))
            return str::Dup(*(const char **)fieldPtr);
        return EscapeStr(*(const char **)fieldPtr);
    case TYPE_WSTR:
        if (!*(const WCHAR **)fieldPtr)
            return NULL; // skip empty strings
        value.Set(str::conv::ToUtf8(*(const WCHAR **)fieldPtr));
        if (NeedsEscaping(value))
            return EscapeStr(value);
        return value.StealData();
    case (TYPE_STRUCT_PTR | TYPE_STORE_COMPACT_MASK):
        for (size_t i = 0; i < GetStructDef(field)->nFields; i++) {
            ScopedMem<char> val(SerializeField(*(const uint8_t **)fieldPtr, GetStructDef(field)->fields[i]));
            if (!value)
                value.Set(str::Format("%s", val));
            else
                value.Set(str::Format("%s %s", value, val));
        }
        return value.StealData();
    case TYPE_STRUCT_PTR:
    case TYPE_ARRAY:
        // nested structs are serialized after all other values
        break;
    default:
        CrashIf(!(field.type & TYPE_NO_STORE_MASK));
    }
    return NULL;
}

static IniSection *FindSection(IniFile& ini, const char *name, size_t idx, size_t endIdx, size_t *foundIdx)
{
    for (size_t i = idx; i < endIdx; i++) {
        if (str::EqI(ini.sections.At(i)->name, name)) {
            *foundIdx = i;
            return ini.sections.At(i);
        }
    }
    return NULL;
}

static void *DeserializeRec(IniFile& ini, void *base, const StructMetadata *def,
                            const char *sectionName=NULL, size_t startIdx=0, size_t endIdx=-1)
{
    if ((size_t)-1 == endIdx)
        endIdx = ini.sections.Count();

    size_t secIdx = startIdx;
    IniSection *section = FindSection(ini, sectionName, startIdx, endIdx, &secIdx);

    uint8_t *data = (uint8_t *)base;
    if (!data)
        data = AllocArray<uint8_t>(def->size);
    if (secIdx >= endIdx) {
        section = NULL;
        secIdx = startIdx - 1;
    }

    const char *fieldName = def->fieldNames;
    for (size_t i = 0; i < def->nFields; i++) {
        const FieldMetadata& field = def->fields[i];
        if (TYPE_STRUCT_PTR == field.type) {
            ScopedMem<char> name(sectionName ? str::Format("%s.%s", sectionName, fieldName) : str::Dup(fieldName));
            *(void **)(data + field.offset) = DeserializeRec(ini, *(void **)(data + field.offset), GetStructDef(field), name, secIdx + 1, endIdx);
        }
        else if (TYPE_ARRAY == field.type) {
            ScopedMem<char> name(sectionName ? str::Format("%s.%s", sectionName, fieldName) : str::Dup(fieldName));
            Vec<void *> *array = new Vec<void *>();
            FreeListNode(*(Vec<void *> **)(data + field.offset), GetStructDef(field));
            *(Vec<void *> **)(data + field.offset) = array;
            size_t nextSecIdx = endIdx;
            FindSection(ini, sectionName, secIdx + 1, endIdx, &nextSecIdx);
            size_t subSecIdx = nextSecIdx;
            IniSection *subSection = FindSection(ini, name, secIdx + 1, nextSecIdx, &subSecIdx);
            while (subSection && subSecIdx < nextSecIdx) {
                size_t nextSubSecIdx = nextSecIdx;
                IniSection *nextSubSec = FindSection(ini, name, subSecIdx + 1, nextSecIdx, &nextSubSecIdx);
                array->Append(DeserializeRec(ini, NULL, GetStructDef(field), name, subSecIdx, nextSubSecIdx));
                subSection = nextSubSec; subSecIdx = nextSubSecIdx;
            }
        }
        else {
            IniLine *line = section ? section->FindLine(fieldName) : NULL;
            DeserializeField(data, field, line ? line->value : NULL);
        }
        fieldName += str::Len(fieldName) + 1;
    }
    return data;
}

uint8_t *DeserializeWithDefaultIni(char *data, size_t dataSize, char *defaultData, size_t defaultDataSize, const StructMetadata *def)
{
    void *base = NULL;
    if (defaultData) {
        CrashIf(str::Len(defaultData) != defaultDataSize);
        IniFile iniDef(defaultData);
        base = DeserializeRec(iniDef, base, def);
    }
    CrashIf(str::Len(data) != dataSize);
    IniFile ini(data);
    return (uint8_t *)DeserializeRec(ini, base, def);
}

static void SerializeRecIni(str::Str<char>& out, const uint8_t *data, const StructMetadata *def, const char *sectionName=NULL)
{
    if (sectionName) {
        out.Append("\r\n[");
        out.Append(sectionName);
        out.Append("]\r\n");
    }

    const char *fieldName = def->fieldNames;
    for (size_t i = 0; i < def->nFields; i++) {
        CrashIf(str::FindChar(fieldName, '=') || str::FindChar(fieldName, ':') || NeedsEscaping(fieldName));
        ScopedMem<char> value(SerializeField(data, def->fields[i]));
        if (value) {
            out.Append(fieldName);
            out.Append(" = ");
            out.Append(value);
            out.Append("\r\n");
        }
        fieldName += str::Len(fieldName) + 1;
    }

    fieldName = def->fieldNames;
    for (size_t i = 0; i < def->nFields; i++) {
        const FieldMetadata& field = def->fields[i];
        if (TYPE_STRUCT_PTR == field.type) {
            ScopedMem<char> name(sectionName ? str::Format("%s.%s", sectionName, fieldName) : str::Dup(fieldName));
            SerializeRecIni(out, *(const uint8_t **)(data + field.offset), GetStructDef(field), name);
        }
        else if (TYPE_ARRAY == field.type) {
            ScopedMem<char> name(sectionName ? str::Format("%s.%s", sectionName, fieldName) : str::Dup(fieldName));
            Vec<void *> *array = *(Vec<void *> **)(data + field.offset);
            for (size_t j = 0; j < array->Count(); j++) {
                SerializeRecIni(out, (const uint8_t *)array->At(j), GetStructDef(field), name);
            }
        }
        fieldName += str::Len(fieldName) + 1;
    }
}

uint8_t *SerializeIni(const uint8_t *data, const StructMetadata *def, size_t *sizeOut)
{
    str::Str<char> out;
    out.Append(UTF8_BOM "; this file will be overwritten - modify at your own risk\r\n");
    SerializeRecIni(out, data, def);
    if (sizeOut)
        *sizeOut = out.Size();
    return (uint8_t *)out.StealData();
}

static void *DeserializeRec(SquareTreeNode *node, void *base, const StructMetadata *def)
{
    uint8_t *data = (uint8_t *)base;
    if (!data)
        data = AllocArray<uint8_t>(def->size);

    const char *fieldName = def->fieldNames;
    for (size_t i = 0; i < def->nFields; i++) {
        const FieldMetadata& field = def->fields[i];
        if (TYPE_STRUCT_PTR == field.type) {
            SquareTreeNode *child = node ? node->GetChild(fieldName) : NULL;
            *(void **)(data + field.offset) = DeserializeRec(child, *(void **)(data + field.offset), GetStructDef(field));
        }
        else if (TYPE_ARRAY == field.type) {
            Vec<void *> *array = new Vec<void *>();
            FreeListNode(*(Vec<void *> **)(data + field.offset), GetStructDef(field));
            *(Vec<void *> **)(data + field.offset) = array;
            SquareTreeNode *child;
            size_t idx = 0;
#if defined(ENABLE_FORMAT_TXT_SQT) || defined(ENABLE_FORMAT_TXT2)
            if (node && node->GetChild(fieldName) && node->GetChild(fieldName)->GetChild("")) {
                node = node->GetChild(fieldName);
                fieldName += str::Len(fieldName);
            }
#endif
            while (node && (child = node->GetChild(fieldName, &idx)) != NULL) {
                array->Append(DeserializeRec(child, NULL, GetStructDef(field)));
            }
        }
        else {
            const char *value = node ? node->GetValue(fieldName) : NULL;
            DeserializeField(data, field, value);
        }
        fieldName += str::Len(fieldName) + 1;
    }

    return data;
}

uint8_t *DeserializeWithDefaultSqt(char *data, size_t dataSize, char *defaultData, size_t defaultDataSize, const StructMetadata *def)
{
    void *base = NULL;
    if (defaultData) {
        CrashIf(str::Len(defaultData) != defaultDataSize);
        SquareTree sqtDef(defaultData);
        base = DeserializeRec(sqtDef.root, base, def);
    }
    CrashIf(str::Len(data) != dataSize);
    SquareTree sqt(data);
    return (uint8_t *)DeserializeRec(sqt.root, base, def);
}

static void FixupStringDecoding(uint8_t *data, const StructMetadata *def)
{
    if (!data)
        return;
    for (size_t i = 0; i < def->nFields; i++) {
        const FieldMetadata& field = def->fields[i];
        if (TYPE_STRUCT_PTR == field.type)
            FixupStringDecoding(*(uint8_t **)(data + field.offset), GetStructDef(field));
        else if (TYPE_ARRAY == field.type) {
            Vec<void *> *array = *(Vec<void *> **)(data + field.offset);
            for (size_t j = 0; j < array->Count(); j++) {
                FixupStringDecoding((uint8_t *)array->At(j), GetStructDef(field));
            }
        }
        else if (TYPE_STR == field.type && *(char **)(data + field.offset)) {
            char *str = *(char **)(data + field.offset);
            *UnescapeLineInPlace(str, str + str::Len(str), '$') = '\0';
        }
        else if (TYPE_WSTR == field.type && *(WCHAR **)(data + field.offset)) {
            ScopedMem<char> ustr(str::conv::ToUtf8(*(WCHAR **)(data + field.offset)));
            char *str = ustr.Get();
            *UnescapeLineInPlace(str, str + str::Len(str), '$') = '\0';
            free(*(WCHAR **)(data + field.offset));
            *(WCHAR **)(data + field.offset) = str::conv::FromUtf8(ustr);
        }
    }
}

uint8_t *DeserializeWithDefaultTxtSqt(char *data, size_t dataSize, char *defaultData, size_t defaultDataSize, const StructMetadata *def)
{
    void *base = NULL;
    if (defaultData) {
        CrashIf(str::Len(defaultData) != defaultDataSize);
        SquareTree sqtDef(defaultData);
        base = DeserializeRec(sqtDef.root, base, def);
    }
    CrashIf(str::Len(data) != dataSize);
    SquareTree sqt(data);
    base = DeserializeRec(sqt.root, base, def);
    FixupStringDecoding((uint8_t *)base, def);
    return (uint8_t *)base;
}

static inline void Indent(str::Str<char>& out, int indent)
{
    while (indent-- > 0)
        out.Append('\t');
}

static void SerializeRecSqt(str::Str<char>& out, const uint8_t *data, const StructMetadata *def, int indent=0)
{
    const char *fieldName = def->fieldNames;
    for (size_t i = 0; i < def->nFields; i++) {
        const FieldMetadata& field = def->fields[i];
        CrashIf(str::FindChar(fieldName, '=') || str::FindChar(fieldName, ':') ||
                str::FindChar(fieldName, '[') || str::FindChar(fieldName, ']') ||
                NeedsEscaping(fieldName));
        if (TYPE_STRUCT_PTR == field.type) {
            Indent(out, indent);
            out.Append(fieldName);
            out.Append(" [\r\n");
            SerializeRecSqt(out, *(const uint8_t **)(data + field.offset), GetStructDef(field), indent + 1);
            Indent(out, indent);
            out.Append("]\r\n");
        }
        else if (TYPE_ARRAY == field.type) {
            Indent(out, indent);
            out.Append(fieldName);
            Vec<void *> *array = *(Vec<void *> **)(data + field.offset);
            for (size_t j = 0; j < array->Count(); j++) {
                out.Append(" [\r\n");
                SerializeRecSqt(out, (const uint8_t *)array->At(j), GetStructDef(field), indent + 1);
                Indent(out, indent);
                out.Append("]");
            }
            out.Append("\r\n");
        }
        else {
            ScopedMem<char> value(SerializeField(data, field));
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

uint8_t *SerializeSqt(const uint8_t *data, const StructMetadata *def, size_t *sizeOut)
{
    str::Str<char> out;
    out.Append(UTF8_BOM "# this file will be overwritten - modify at your own risk\r\n");
    SerializeRecSqt(out, data, def);
    if (sizeOut)
        *sizeOut = out.Size();
    return (uint8_t *)out.StealData();
}

static inline void Indent2(str::Str<char>& out, int indent)
{
    while (indent-- > 0)
        out.Append("  ");
}

static void SerializeRecTxt2(str::Str<char>& out, const uint8_t *data, const StructMetadata *def, int indent=0)
{
    const char *fieldName = def->fieldNames;
    for (size_t i = 0; i < def->nFields; i++) {
        const FieldMetadata& field = def->fields[i];
        CrashIf(str::FindChar(fieldName, '=') || str::FindChar(fieldName, ':') ||
                str::FindChar(fieldName, '[') || str::FindChar(fieldName, ']') ||
                NeedsEscaping(fieldName));
        if (TYPE_STRUCT_PTR == field.type) {
            Indent2(out, indent);
            out.Append(fieldName);
            out.Append(" [\r\n");
            SerializeRecTxt2(out, *(const uint8_t **)(data + field.offset), GetStructDef(field), indent + 1);
            Indent2(out, indent);
            out.Append("]\r\n");
        }
        else if (TYPE_ARRAY == field.type) {
            Indent2(out, indent);
            out.Append(fieldName);
            out.Append(" [\r\n");
            Vec<void *> *array = *(Vec<void *> **)(data + field.offset);
            for (size_t j = 0; j < array->Count(); j++) {
                Indent2(out, indent + 1);
                out.Append("[\r\n");
                SerializeRecTxt2(out, (const uint8_t *)array->At(j), GetStructDef(field), indent + 2);
                Indent2(out, indent + 1);
                out.Append("]\r\n");
            }
            Indent2(out, indent);
            out.Append("]\r\n");
        }
        else {
            ScopedMem<char> value(SerializeField(data, field));
            if (value) {
                Indent2(out, indent);
                out.Append(fieldName);
                out.Append(": ");
                out.Append(value);
                out.Append("\r\n");
            }
        }
        fieldName += str::Len(fieldName) + 1;
    }
}

uint8_t *SerializeTxt2(const uint8_t *data, const StructMetadata *def, size_t *sizeOut)
{
    str::Str<char> out;
    out.Append(UTF8_BOM "; see http://blog.kowalczyk.info/software/sumatrapdf/settings.html for documentation\r\n");
    SerializeRecTxt2(out, data, def);
    if (sizeOut)
        *sizeOut = out.Size();
    return (uint8_t *)out.StealData();
}

#define DeserializeWithDefault DeserializeWithDefaultTxt
#define FreeStruct FreeStructTxt
#define AppendNest AppendNestTxt
#include "../sertxt_test/SerializeTxt.cpp"
#undef AppendNest
#undef FreeStruct
#undef DeserializeWithDefault

static SerializationFormat gFormat = Format_Ini;

void SetSerializeTxtFormat(SerializationFormat format)
{
    gFormat = format;
}

uint8_t *DeserializeWithDefault(char *data, size_t dataSize, char *defaultData, size_t defaultDataSize, const StructMetadata *def)
{
    switch (gFormat) {
#ifdef ENABLE_FORMAT_INI
    case Format_Ini: return DeserializeWithDefaultIni(data, dataSize, defaultData, defaultDataSize, def);
#endif
#ifdef ENABLE_FORMAT_SQT
    case Format_Sqt: return DeserializeWithDefaultSqt(data, dataSize, defaultData, defaultDataSize, def);
#endif
#ifdef ENABLE_FORMAT_TXT
    case Format_Txt: return sertxt::DeserializeWithDefaultTxt(data, dataSize, defaultData, defaultDataSize, def);
#endif
#ifdef ENABLE_FORMAT_TXT_SQT
    case Format_Txt_Sqt: return DeserializeWithDefaultTxtSqt(data, dataSize, defaultData, defaultDataSize, def);
#endif
#ifdef ENABLE_FORMAT_TXT2
    case Format_Txt2: return DeserializeWithDefaultSqt(data, dataSize, defaultData, defaultDataSize, def);
#endif
    default: CrashIf(true); return NULL;
    }
}

uint8_t *Deserialize(char *data, size_t dataSize, const StructMetadata *def)
{
    return DeserializeWithDefault(data, dataSize, NULL, 0, def);
}

uint8_t *Serialize(const uint8_t *data, const StructMetadata *def, size_t *sizeOut)
{
    switch (gFormat) {
#ifdef ENABLE_FORMAT_INI
    case Format_Ini: return SerializeIni(data, def, sizeOut);
#endif
#ifdef ENABLE_FORMAT_SQT
    case Format_Sqt: return SerializeSqt(data, def, sizeOut);
#endif
#ifdef ENABLE_FORMAT_TXT
    case Format_Txt: return sertxt::Serialize(data, def, sizeOut);
#endif
#ifdef ENABLE_FORMAT_TXT_SQT
    case Format_Txt_Sqt: return sertxt::Serialize(data, def, sizeOut);
#endif
#ifdef ENABLE_FORMAT_TXT2
    case Format_Txt2: return SerializeTxt2(data, def, sizeOut);
#endif
    default: CrashIf(true); return NULL;
    }
}

void FreeStruct(uint8_t *data, const StructMetadata *def)
{
    if (!data)
        return;
    for (size_t i = 0; i < def->nFields; i++) {
        const FieldMetadata& field = def->fields[i];
        if (TYPE_STRUCT_PTR == (field.type & TYPE_MASK))
            FreeStruct(*(uint8_t **)(data + field.offset), GetStructDef(field));
        else if (TYPE_ARRAY == field.type)
            FreeListNode(*(Vec<void *> **)(data + field.offset), GetStructDef(field));
        else if (TYPE_WSTR == field.type || TYPE_STR == field.type)
            free(*(void **)(data + field.offset));
    }
    free(data);
}

};
