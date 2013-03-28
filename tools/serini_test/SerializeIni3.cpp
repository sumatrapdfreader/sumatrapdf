/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "SerializeIni3.h"

#include "AppPrefs3.h"
#include "IniParser.h"

namespace serini3 {

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

#ifndef NDEBUG
static bool IsCompactable(SettingInfo *meta)
{
    for (size_t i = 1; i <= GetFieldCount(meta); i++) {
        switch (meta[i].type) {
        case Type_Bool: case Type_Int: case Type_Int64: case Type_Float: case Type_Color:
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
    int r, g, b, a;

    switch (field.type) {
    case Type_Bool:
        *(bool *)(base + field.offset) = value ? str::EqI(value, "true") || ParseBencInt(value) != 0 : field.def != 0;
        break;
    case Type_Int:
        *(int *)(base + field.offset) = (int)(value ? ParseBencInt(value) : field.def);
        break;
    case Type_Int64:
        *(int64_t *)(base + field.offset) = value ? ParseBencInt(value) : field.def;
        break;
    case Type_Float:
        if (!value || !str::Parse(value, "%f", (float *)(base + field.offset)))
            *(float *)(base + field.offset) = (float)field.def;
        break;
    case Type_Color:
        if (value && str::Parse(value, "#%2x%2x%2x%2x", &a, &r, &g, &b))
            *(COLORREF *)(base + field.offset) = RGB(r, g, b) | (a << 24);
        else if (value && str::Parse(value, "#%2x%2x%2x", &r, &g, &b))
            *(COLORREF *)(base + field.offset) = RGB(r, g, b);
        else
            *(COLORREF *)(base + field.offset) = (COLORREF)field.def;
        break;
    case Type_String:
        if (value)
            *(WCHAR **)(base + field.offset) = str::conv::FromUtf8(ScopedMem<char>(UnescapeStr(value)));
        else
            *(WCHAR **)(base + field.offset) = str::Dup((const WCHAR *)field.def);
        break;
    case Type_Utf8String:
        if (value)
            *(char **)(base + field.offset) = UnescapeStr(value);
        else
            *(char **)(base + field.offset) = str::Dup((const char *)field.def);
        break;
    case Type_Compact:
        assert(IsCompactable(field.substruct));
        for (size_t i = 1; i <= GetFieldCount(field.substruct); i++) {
            if (value) {
                for (; str::IsWs(*value); value++);
                if (!*value)
                    value = NULL;
            }
            DeserializeField(base + field.offset, field.substruct[i], value);
            if (value)
                for (; *value && !str::IsWs(*value); value++);
        }
        break;
    default:
        CrashIf(true);
    }
}

static void *DeserializeRec(IniFile& ini, SettingInfo *meta, uint8_t *base=NULL,
                            const char *sectionName=NULL, size_t startIdx=0, size_t endIdx=-1)
{
    if ((size_t)-1 == endIdx)
        endIdx = ini.sections.Count();

    size_t secIdx = startIdx;
    IniSection *section = FindSection(ini, sectionName, startIdx, endIdx, &secIdx);

    if (!base)
        base = AllocArray<uint8_t>(GetStructSize(meta));
    if (secIdx >= endIdx) {
        section = NULL;
        secIdx = startIdx - 1;
    }

    for (size_t i = 1; i <= GetFieldCount(meta); i++) {
        if (Type_Struct == meta[i].type) {
            ScopedMem<char> name(sectionName ? str::Format("%s.%s", sectionName, meta[i].name) : str::Dup(meta[i].name));
            DeserializeRec(ini, meta[i].substruct, base + meta[i].offset, name, secIdx + 1, endIdx);
        }
        else if (Type_Array == meta[i].type) {
            ScopedMem<char> name(sectionName ? str::Format("%s.%s", sectionName, meta[i].name) : str::Dup(meta[i].name));
            str::Str<uint8_t> array;
            size_t nextSecIdx = endIdx;
            FindSection(ini, sectionName, secIdx + 1, endIdx, &nextSecIdx);
            size_t subSecIdx = nextSecIdx;
            IniSection *subSection = FindSection(ini, name, secIdx + 1, nextSecIdx, &subSecIdx);
            while (subSection && subSecIdx < nextSecIdx) {
                size_t nextSubSecIdx = nextSecIdx;
                IniSection *nextSubSec = FindSection(ini, name, subSecIdx + 1, nextSecIdx, &nextSubSecIdx);
                uint8_t *subbase = array.AppendBlanks(GetStructSize(meta[i].substruct));
                DeserializeRec(ini, meta[i].substruct, subbase, name, subSecIdx, nextSubSecIdx);
                subSection = nextSubSec; subSecIdx = nextSubSecIdx;
            }
            *(size_t *)(base + meta[i+1].offset) = array.Size() / GetStructSize(meta[i].substruct);
            *(uint8_t **)(base + meta[i].offset) = array.StealData();
        }
        else if (Type_Meta != meta[i].type) {
            IniLine *line = section ? section->FindLine(meta[i].name) : NULL;
            DeserializeField(base, meta[i], line ? line->value : NULL);
        }
    }
    return base;
}

void *Deserialize(const char *data, size_t dataLen, SettingInfo *def)
{
    CrashIf(str::Len(data) != dataLen);
    IniFile ini(data);
    return DeserializeRec(ini, def);
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

static char *SerializeField(const uint8_t *base, SettingInfo& field)
{
    ScopedMem<char> value;
    COLORREF c;

    switch (field.type) {
    // TODO: only write non-default values?
    case Type_Bool: return str::Dup(*(bool *)(base + field.offset) ? "true" : "false");
    case Type_Int: return str::Format("%d", *(int *)(base + field.offset));
    case Type_Int64: return str::Format("%I64d", *(int64_t *)(base + field.offset));
    case Type_Float: return str::Format("%g", *(float *)(base + field.offset));
    case Type_Color:
        c = *(COLORREF *)(base + field.offset);
        // TODO: COLORREF doesn't really have an alpha value
        if (((c >> 24) & 0xff))
            return str::Format("#%02x%02x%02x%02x", (c >> 24) & 0xff, GetRValue(c), GetGValue(c), GetBValue(c));
        return str::Format("#%02x%02x%02x", GetRValue(c), GetGValue(c), GetBValue(c));
    case Type_String:
        if (!*(const WCHAR **)(base + field.offset))
            return NULL; // skip empty strings
        value.Set(str::conv::ToUtf8(*(const WCHAR **)(base + field.offset)));
        if (NeedsEscaping(value))
            return EscapeStr(value);
        return value.StealData();
    case Type_Utf8String:
        if (!*(const char **)(base + field.offset))
            return NULL; // skip empty strings
        if (!NeedsEscaping(*(const char **)(base + field.offset)))
            return str::Dup(*(const char **)(base + field.offset));
        return EscapeStr(*(const char **)(base + field.offset));
    case Type_Compact:
        assert(IsCompactable(field.substruct));
        for (size_t i = 1; i <= GetFieldCount(field.substruct); i++) {
            ScopedMem<char> val(SerializeField(base + field.offset, field.substruct[i]));
            if (!value)
                value.Set(str::Format("%s", val));
            else
                value.Set(str::Format("%s %s", value, val));
        }
        return value.StealData();
    default:
        CrashIf(true);
    }
    return NULL;
}

static void SerializeRec(str::Str<char>& out, const void *data, SettingInfo *meta, const char *sectionName=NULL)
{
    if (sectionName) {
        out.Append("[");
        out.Append(sectionName);
        out.Append("]\r\n");
    }

    const uint8_t *base = (const uint8_t *)data;
    for (size_t i = 1; i <= GetFieldCount(meta); i++) {
        // nested structs are serialized after all other values
        if (Type_Meta == meta[i].type || Type_Struct == meta[i].type || Type_Array == meta[i].type)
            continue;
        CrashIf(str::FindChar(meta[i].name, '=') || NeedsEscaping(meta[i].name));
        ScopedMem<char> value(SerializeField(base, meta[i]));
        if (value) {
            out.Append(meta[i].name);
            out.Append(" = ");
            out.Append(value);
            out.Append("\r\n");
        }
    }

    for (size_t i = 1; i <= GetFieldCount(meta); i++) {
        if (Type_Struct == meta[i].type) {
            ScopedMem<char> name(sectionName ? str::Format("%s.%s", sectionName, meta[i].name) : str::Dup(meta[i].name));
            SerializeRec(out, base + meta[i].offset, meta[i].substruct, name);
        }
        else if (Type_Array == meta[i].type) {
            ScopedMem<char> name(sectionName ? str::Format("%s.%s", sectionName, meta[i].name) : str::Dup(meta[i].name));
            size_t count = *(size_t *)(base + meta[i+1].offset);
            uint8_t *subbase = *(uint8_t **)(base + meta[i].offset);
            for (size_t j = 0; j < count; j++) {
                SerializeRec(out, subbase + j * GetStructSize(meta[i].substruct), meta[i].substruct, name);
            }
        }
    }
}

char *Serialize(const void *data, SettingInfo *def, size_t *sizeOut, const char *comment)
{
    str::Str<char> out;
    if (comment) {
        out.Append(UTF8_BOM "; ");
        out.Append(comment);
        out.Append("\r\n");
    }
    else {
        out.Append(UTF8_BOM "; this file will be overwritten - modify at your own risk\r\n");
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
            FreeStructData(base + meta[i].offset, meta[i].substruct);
        else if (Type_Array == meta[i].type) {
            size_t count = *(size_t *)(base + meta[i+1].offset);
            uint8_t *subbase = *(uint8_t **)(base + meta[i].offset);
            for (size_t j = 0; j < count; j++) {
                FreeStructData(subbase + j * GetStructSize(meta[i].substruct), meta[i].substruct);
            }
            free(subbase);
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
