/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "SerializeIni.h"

#ifndef TEST_SERIALIZE_TXT

#ifndef TEST_SERIALIZE_SQT
#include "IniParser.h"
// only escape characters which are significant to IniParser:
// newlines and heading/trailing whitespace
static bool NeedsEscaping(const char *s)
{
    return str::IsWs(*s) || *s && str::IsWs(*(s + str::Len(s) - 1)) ||
           str::FindChar(s, '\n') || str::FindChar(s, '\r');
}
#else
#include "SquareTreeParser.h"
// only escape characters which are significant to SquareTreeParser:
// newlines, heading/trailing whitespace and single square brackets
static bool NeedsEscaping(const char *s)
{
    return str::IsWs(*s) || *s && str::IsWs(*(s + str::Len(s) - 1)) ||
           str::FindChar(s, '\n') || str::FindChar(s, '\r') ||
           str::Eq(s, "[") || str::Eq(s, "]");
}
#endif // TEST_SERIALIZE_SQT

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
    int r, g, b, a;

    switch (field.type | 0) {
    case TYPE_BOOL:
        *(bool *)(data + field.offset) = str::StartsWithI(value, "true") && (!value[4] || str::IsWs(value[4])) || ParseBencInt(value) != 0;
        break;
    // TODO: are all these int-types really needed?
    case TYPE_I16:
        *(int16_t *)(data + field.offset) = (int16_t)ParseBencInt(value);
        break;
    case TYPE_U16:
        *(uint16_t *)(data + field.offset) = (uint16_t)ParseBencInt(value);
        break;
    case TYPE_I32:
        *(int32_t *)(data + field.offset) = (int32_t)ParseBencInt(value);
        break;
    case TYPE_U32:
        *(uint32_t *)(data + field.offset) = (uint32_t)ParseBencInt(value);
        break;
    case TYPE_U64:
        *(uint64_t *)(data + field.offset) = (uint64_t)ParseBencInt(value);
        break;
    case TYPE_FLOAT:
        if (!str::Parse(value, "%f", (float *)(data + field.offset)))
            *(float *)(data + field.offset) = 0.f;
        break;
    case TYPE_COLOR:
        if (str::Parse(value, "#%2x%2x%2x%2x", &a, &r, &g, &b))
            *(COLORREF *)(data + field.offset) = RGB(r, g, b) | (a << 24);
        else if (str::Parse(value, "#%2x%2x%2x", &r, &g, &b))
            *(COLORREF *)(data + field.offset) = RGB(r, g, b);
        break;
    case TYPE_STR:
        free(*(char **)(data + field.offset));
        *(char **)(data + field.offset) = UnescapeStr(value);
        break;
    case TYPE_WSTR:
        free(*(WCHAR **)(data + field.offset));
        *(WCHAR **)(data + field.offset) = str::conv::FromUtf8(ScopedMem<char>(UnescapeStr(value)));
        break;
    case (TYPE_STRUCT_PTR | TYPE_STORE_COMPACT_MASK):
        if (!*(uint8_t **)(data + field.offset))
            *(uint8_t **)(data + field.offset) = AllocArray<uint8_t>(field.def->size);
        for (size_t i = 0; i < field.def->nFields; i++) {
            DeserializeField(*(uint8_t **)(data + field.offset), field.def->fields[i], value);
            CrashIf(TYPE_STR == field.def->fields[i].type || TYPE_WSTR == field.def->fields[i].type);
            for (; *value && !str::IsWs(*value); value++);
            for (; str::IsWs(*value); value++);
        }
        break;
    default:
        CrashIf(true);
    }
}

static char *SerializeField(const uint8_t *data, const FieldMetadata& field)
{
    ScopedMem<char> value;
    COLORREF c;

    switch (field.type | 0) {
    case TYPE_BOOL: return str::Dup(*(bool *)(data + field.offset) ? "true" : "false");
    case TYPE_I16: return str::Format("%d", (int32_t)*(int16_t *)(data + field.offset));
    case TYPE_U16: return str::Format("%u", (uint32_t)*(uint16_t *)(data + field.offset));
    case TYPE_I32: return str::Format("%d", *(int32_t *)(data + field.offset));
    case TYPE_U32: return str::Format("%u", *(uint32_t *)(data + field.offset));
    case TYPE_U64: return str::Format("%I64u", *(uint64_t *)(data + field.offset));
    case TYPE_FLOAT: return str::Format("%g", *(float *)(data + field.offset));
    case TYPE_COLOR:
        c = *(COLORREF *)(data + field.offset);
        // TODO: COLORREF doesn't really have an alpha value
        if (((c >> 24) & 0xff))
            return str::Format("#%02x%02x%02x%02x", (c >> 24) & 0xff, GetRValue(c), GetGValue(c), GetBValue(c));
        return str::Format("#%02x%02x%02x", GetRValue(c), GetGValue(c), GetBValue(c));
    case TYPE_STR:
        if (!*(const char **)(data + field.offset))
            return NULL; // skip empty strings
        if (!NeedsEscaping(*(const char **)(data + field.offset)))
            return str::Dup(*(const char **)(data + field.offset));
        return EscapeStr(*(const char **)(data + field.offset));
    case TYPE_WSTR:
        if (!*(const WCHAR **)(data + field.offset))
            return NULL; // skip empty strings
        value.Set(str::conv::ToUtf8(*(const WCHAR **)(data + field.offset)));
        if (NeedsEscaping(value))
            return EscapeStr(value);
        return value.StealData();
    case (TYPE_STRUCT_PTR | TYPE_STORE_COMPACT_MASK):
        for (size_t i = 0; i < field.def->nFields; i++) {
            ScopedMem<char> val(SerializeField(*(const uint8_t **)(data + field.offset), field.def->fields[i]));
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

#ifndef TEST_SERIALIZE_SQT

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
    IniLine *line;

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
            *(void **)(data + field.offset) = DeserializeRec(ini, *(void **)(data + field.offset), field.def, name, secIdx + 1, endIdx);
        }
        else if (TYPE_ARRAY == field.type) {
            ScopedMem<char> name(sectionName ? str::Format("%s.%s", sectionName, fieldName) : str::Dup(fieldName));
            Vec<void *> *array = new Vec<void *>();
            FreeListNode(*(Vec<void *> **)(data + field.offset), field.def);
            *(Vec<void *> **)(data + field.offset) = array;
            size_t nextSecIdx = endIdx;
            FindSection(ini, sectionName, secIdx + 1, endIdx, &nextSecIdx);
            size_t subSecIdx = nextSecIdx;
            IniSection *subSection = FindSection(ini, name, secIdx + 1, nextSecIdx, &subSecIdx);
            while (subSection && subSecIdx < nextSecIdx) {
                size_t nextSubSecIdx = nextSecIdx;
                IniSection *nextSubSec = FindSection(ini, name, subSecIdx + 1, nextSecIdx, &nextSubSecIdx);
                array->Append(DeserializeRec(ini, NULL, field.def, name, subSecIdx, nextSubSecIdx));
                subSection = nextSubSec; subSecIdx = nextSubSecIdx;
            }
        }
        else if (section && (line = section->FindLine(fieldName)) != NULL)
            DeserializeField(data, field, line->value);
        // else printf("couldn't find line for %s (%s)\n", field.name, sectionName);
        fieldName += str::Len(fieldName) + 1;
    }
    return data;
}

uint8_t *DeserializeWithDefault(char *data, size_t dataSize, char *defaultData, size_t defaultDataSize, const StructMetadata *def)
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

static void SerializeRec(str::Str<char>& out, const uint8_t *data, const StructMetadata *def, const char *sectionName=NULL)
{
    if (sectionName) {
        out.Append("[");
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
            SerializeRec(out, *(const uint8_t **)(data + field.offset), field.def, name);
        }
        else if (TYPE_ARRAY == field.type) {
            ScopedMem<char> name(sectionName ? str::Format("%s.%s", sectionName, fieldName) : str::Dup(fieldName));
            Vec<void *> *array = *(Vec<void *> **)(data + field.offset);
            for (size_t j = 0; j < array->Count(); j++) {
                SerializeRec(out, (const uint8_t *)array->At(j), field.def, name);
            }
        }
        fieldName += str::Len(fieldName) + 1;
    }
}

#else

static void *DeserializeRec(SquareTreeNode *node, void *base, const StructMetadata *def)
{
    uint8_t *data = (uint8_t *)base;
    if (!data)
        data = AllocArray<uint8_t>(def->size);

    const char *fieldName = def->fieldNames;
    for (size_t i = 0; i < def->nFields; i++) {
        const FieldMetadata& field = def->fields[i];
        const char *value;
        if (TYPE_STRUCT_PTR == field.type) {
            SquareTreeNode *child = node ? node->GetChild(fieldName) : NULL;
            *(void **)(data + field.offset) = DeserializeRec(child, *(void **)(data + field.offset), field.def);
        }
        else if (TYPE_ARRAY == field.type) {
            Vec<void *> *array = new Vec<void *>();
            FreeListNode(*(Vec<void *> **)(data + field.offset), field.def);
            *(Vec<void *> **)(data + field.offset) = array;
            SquareTreeNode *child;
            for (size_t j = 0; node && (child = node->GetChild(fieldName, j)) != NULL; j++) {
                array->Append(DeserializeRec(child, NULL, field.def));
            }
        }
        else if (node && (value = node->GetValue(fieldName)) != NULL)
            DeserializeField(data, field, value);
        // else printf("couldn't find line for %s (%s)\n", field.name, sectionName);
        fieldName += str::Len(fieldName) + 1;
    }

    return data;
}

uint8_t *DeserializeWithDefault(char *data, size_t dataSize, char *defaultData, size_t defaultDataSize, const StructMetadata *def)
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

static inline void Indent(str::Str<char>& out, int indent)
{
    while (indent-- > 0)
        out.Append('\t');
}

static void SerializeRec(str::Str<char>& out, const uint8_t *data, const StructMetadata *def, int indent=0)
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
            SerializeRec(out, *(const uint8_t **)(data + field.offset), field.def, indent + 1);
            Indent(out, indent);
            out.Append("]\r\n");
        }
        else if (TYPE_ARRAY == field.type) {
            Indent(out, indent);
            out.Append(fieldName);
            Vec<void *> *array = *(Vec<void *> **)(data + field.offset);
            for (size_t j = 0; j < array->Count(); j++) {
                out.Append(" [\r\n");
                SerializeRec(out, (const uint8_t *)array->At(j), field.def, indent + 1);
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

#endif // TEST_SERIALIZE_SQT

uint8_t *Deserialize(char *data, size_t dataSize, const StructMetadata *def)
{
    return DeserializeWithDefault(data, dataSize, NULL, 0, def);
}

uint8_t *Serialize(const uint8_t *data, const StructMetadata *def, size_t *sizeOut)
{
    str::Str<char> out;
    out.Append(UTF8_BOM "; this file will be overwritten - modify at your own risk\r\n");
    SerializeRec(out, data, def);
    if (sizeOut)
        *sizeOut = out.Size();
    return (uint8_t *)out.StealData();
}

void FreeStruct(uint8_t *data, const StructMetadata *def)
{
    if (!data)
        return;
    for (size_t i = 0; i < def->nFields; i++) {
        const FieldMetadata& field = def->fields[i];
        if (TYPE_STRUCT_PTR == (field.type & TYPE_MASK))
            FreeStruct(*(uint8_t **)(data + field.offset), field.def);
        else if (TYPE_ARRAY == field.type)
            FreeListNode(*(Vec<void *> **)(data + field.offset), field.def);
        else if (TYPE_WSTR == field.type || TYPE_STR == field.type)
            free(*(void **)(data + field.offset));
    }
    free(data);
}

};

#else

#include "../../src/utils/StrSlice.cpp"
#include "../sertxt_test/SerializeTxt.cpp"
#include "../sertxt_test/SerializeTxtParser.cpp"

#endif // TEST_SERIALIZE_TXT
