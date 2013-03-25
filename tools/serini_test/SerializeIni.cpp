/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "../sertxt_test/SerializeTxt.h"

#include "IniParser.h"

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

static void *DeserializeRec(IniFile& ini, StructMetadata *def, const char *fieldNamesSeq, const char *sectionName=NULL, size_t startIdx=0, size_t endIdx=-1)
{
    if ((size_t)-1 == endIdx)
        endIdx = ini.sections.Count();

    size_t secIdx = startIdx;
    IniSection *section = FindSection(ini, sectionName, startIdx, endIdx, &secIdx);
    IniLine *line;
    int r, g, b, a;

    uint8_t *data = (uint8_t *)calloc(1, def->size);
    if (secIdx >= endIdx) {
        section = NULL;
        secIdx = startIdx - 1;
    }

    for (size_t i = 0; i < def->nFields; i++) {
        FieldMetadata& field = def->fields[i];
        const char *fieldName = fieldNamesSeq + field.nameOffset;
        if (TYPE_STRUCT_PTR == field.type) {
            ScopedMem<char> name(sectionName ? str::Format("%s.%s", sectionName, fieldName) : str::Dup(fieldName));
            *(void **)(data + field.offset) = DeserializeRec(ini, field.def, fieldNamesSeq, name, secIdx + 1, endIdx);
            continue;
        }
        if (TYPE_ARRAY == field.type) {
            ScopedMem<char> name(sectionName ? str::Format("%s.%s", sectionName, fieldName) : str::Dup(fieldName));
            ListNode<void> *root = NULL, **next = &root;
            size_t nextSecIdx = endIdx;
            FindSection(ini, sectionName, secIdx + 1, endIdx, &nextSecIdx);
            size_t subSecIdx = nextSecIdx;
            IniSection *subSection = FindSection(ini, name, secIdx + 1, nextSecIdx, &subSecIdx);
            while (subSection && subSecIdx < nextSecIdx) {
                size_t nextSubSecIdx = nextSecIdx;
                IniSection *nextSubSec = FindSection(ini, name, subSecIdx + 1, nextSecIdx, &nextSubSecIdx);
                *next = AllocStruct<ListNode<void>>();
                (*next)->val = DeserializeRec(ini, field.def, fieldNamesSeq, name, subSecIdx, nextSubSecIdx);
                next = &(*next)->next;
                subSection = nextSubSec; subSecIdx = nextSubSecIdx;
            }
            *(ListNode<void> **)(data + field.offset) = root;
            continue;
        }
        if (!section || !(line = section->FindLine(fieldName))) {
            // printf("couldn't find line for %s (%s)\n", field.name, sectionName);
            continue;
        }
        switch (field.type) {
        case TYPE_BOOL:
            *(bool *)(data + field.offset) = str::EqI(line->value, "true") || ParseBencInt(line->value) != 0;
            break;
        // TODO: are all these int-types really needed?
        case TYPE_I16:
            *(int16_t *)(data + field.offset) = (int16_t)ParseBencInt(line->value);
            break;
        case TYPE_U16:
            *(uint16_t *)(data + field.offset) = (uint16_t)ParseBencInt(line->value);
            break;
        case TYPE_I32:
            *(int32_t *)(data + field.offset) = (int32_t)ParseBencInt(line->value);
            break;
        case TYPE_U32:
            *(uint32_t *)(data + field.offset) = (uint32_t)ParseBencInt(line->value);
            break;
        case TYPE_U64:
            *(uint64_t *)(data + field.offset) = (uint64_t)ParseBencInt(line->value);
            break;
        case TYPE_FLOAT:
            if (!str::Parse(line->value, "%f", (float *)(data + field.offset)))
                *(float *)(data + field.offset) = 0.f;
            break;
        case TYPE_COLOR:
            if (str::Parse(line->value, "#%2x%2x%2x%2x", &a, &r, &g, &b))
                *(COLORREF *)(data + field.offset) = RGB(r, g, b) | (a << 24);
            else if (str::Parse(line->value, "#%2x%2x%2x", &r, &g, &b))
                *(COLORREF *)(data + field.offset) = RGB(r, g, b);
            else
                *(COLORREF *)(data + field.offset) = (COLORREF)0;
            break;
        case TYPE_STR:
            *(char **)(data + field.offset) = UnescapeStr(line->value);
            break;
        case TYPE_WSTR:
            *(WCHAR **)(data + field.offset) = str::conv::FromUtf8(ScopedMem<char>(UnescapeStr(line->value)));
            break;
        default:
            CrashIf(true);
        }
    }
    return data;
}

uint8_t *Deserialize(const uint8_t *data, int dataSize, StructMetadata *def, const char *fieldNamesSeq)
{
    CrashIf(!data); // TODO: where to get defaults from?
    CrashIf(str::Len((const char *)data) != (size_t)dataSize);
    IniFile ini((const char *)data);
    return (uint8_t *)DeserializeRec(ini, def, fieldNamesSeq);
}

// only escape characters which are significant to IniParser:
// newlines and heading/trailing whitespace
static bool NeedsEscaping(const char *s)
{
    return str::IsWs(*s) || *s && str::IsWs(*(s + str::Len(s) - 1)) ||
           str::FindChar(s, '\n') || str::FindChar(s, '\r');
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

static void SerializeRec(str::Str<char>& out, const uint8_t *data, StructMetadata *def, const char *fieldNamesSeq, const char *sectionName=NULL)
{
    if (sectionName) {
        out.Append("[");
        out.Append(sectionName);
        out.Append("]\r\n");
    }

    COLORREF c;
    for (size_t i = 0; i < def->nFields; i++) {
        FieldMetadata& field = def->fields[i];
        const char *fieldName = fieldNamesSeq + field.nameOffset;
        CrashIf(str::FindChar(fieldName, '=') || NeedsEscaping(fieldName));
        ScopedMem<char> value;
        switch (field.type) {
        case TYPE_BOOL:
            value.Set(str::Dup(*(bool *)(data + field.offset) ? "true" : "false"));
            break;
        case TYPE_I16:
            value.Set(str::Format("%d", (int32_t)*(int16_t *)(data + field.offset)));
            break;
        case TYPE_U16:
            value.Set(str::Format("%u", (uint32_t)*(uint16_t *)(data + field.offset)));
            break;
        case TYPE_I32:
            value.Set(str::Format("%d", *(int32_t *)(data + field.offset)));
            break;
        case TYPE_U32:
            value.Set(str::Format("%u", *(uint32_t *)(data + field.offset)));
            break;
        case TYPE_U64:
            value.Set(str::Format("%I64u", *(uint64_t *)(data + field.offset)));
            break;
        case TYPE_FLOAT:
            value.Set(str::Format("%g", *(float *)(data + field.offset)));
            break;
        case TYPE_COLOR:
            c = *(COLORREF *)(data + field.offset);
            // TODO: COLORREF doesn't really have an alpha value
            if (((c >> 24) & 0xff))
                value.Set(str::Format("#%02x%02x%02x%02x", (c >> 24) & 0xff, GetRValue(c), GetGValue(c), GetBValue(c)));
            else
                value.Set(str::Format("#%02x%02x%02x", GetRValue(c), GetGValue(c), GetBValue(c)));
            break;
        case TYPE_STR:
            if (!*(const char **)(data + field.offset))
                /* skip empty string */;
            else if (!NeedsEscaping(*(const char **)(data + field.offset)))
                value.Set(str::Dup(*(const char **)(data + field.offset)));
            else
                value.Set(EscapeStr(*(const char **)(data + field.offset)));
            break;
        case TYPE_WSTR:
            if (*(const WCHAR **)(data + field.offset)) {
                value.Set(str::conv::ToUtf8(*(const WCHAR **)(data + field.offset)));
                if (NeedsEscaping(value))
                    value.Set(EscapeStr(value));
            }
            break;
        case TYPE_STRUCT_PTR:
        case TYPE_ARRAY:
            // nested structs are serialized after all other values
            break;
        default:
            CrashIf(!(field.type & TYPE_NO_STORE_MASK));
        }
        if (value) {
            out.Append(fieldName);
            out.Append(" = ");
            out.Append(value);
            out.Append("\r\n");
        }
    }

    for (size_t i = 0; i < def->nFields; i++) {
        FieldMetadata& field = def->fields[i];
        const char *fieldName = fieldNamesSeq + field.nameOffset;
        if (TYPE_STRUCT_PTR == field.type) {
            ScopedMem<char> name(sectionName ? str::Format("%s.%s", sectionName, fieldName) : str::Dup(fieldName));
            SerializeRec(out, *(const uint8_t **)(data + field.offset), field.def, fieldNamesSeq, name);
        }
        else if (TYPE_ARRAY == field.type) {
            ScopedMem<char> name(sectionName ? str::Format("%s.%s", sectionName, fieldName) : str::Dup(fieldName));
            for (ListNode<void> *node = *(ListNode<void> **)(data + field.offset); node; node = node->next) {
                SerializeRec(out, (const uint8_t *)node->val, field.def, fieldNamesSeq, name);
            }
        }
    }
}

uint8_t *Serialize(const uint8_t *data, StructMetadata *def, const char *fieldNamesSeq, int *sizeOut)
{
    str::Str<char> out;
    out.Append(UTF8_BOM "; this file will be overwritten - modify at your own risk\r\n");
    SerializeRec(out, data, def, fieldNamesSeq);
    if (sizeOut)
        *sizeOut = (int)out.Size();
    return (uint8_t *)out.StealData();
}

void FreeStruct(uint8_t *data, StructMetadata *def)
{
    if (!data)
        return;
    for (size_t i = 0; i < def->nFields; i++) {
        FieldMetadata& field = def->fields[i];
        if (TYPE_STRUCT_PTR == field.type)
            FreeStruct(*(uint8_t **)(data + field.offset), field.def);
        else if (TYPE_ARRAY == field.type) {
            ListNode<void> *node = *(ListNode<void> **)(data + field.offset);
            while (node) {
                ListNode<void> *next = node->next;
                FreeStruct((uint8_t *)node->val, field.def);
                free(node);
                node = next;
            }
        }
        else if (TYPE_WSTR == field.type || TYPE_STR == field.type)
            free(*(void **)(data + field.offset));
    }
    free(data);
}

};
