#include "BaseUtil.h"
#include "FileUtil.h"
#include "IniParser.h"
#include "../sertxt_test/SerializeTxt.h"
#include "../sertxt_test/SettingsSumatra.h"
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

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

void *DeserializeRec(IniFile& p, StructMetadata *def, const char *sectionName=NULL)
{
    IniSection *section = p.FindSection(sectionName);
    IniLine *line = NULL;
    int r, g, b;
    float f;

    uint8_t *data = (uint8_t *)calloc(1, def->size);
    for (size_t i = 0; i < def->nFields; i++) {
        FieldMetadata& field = def->fields[i];
        if (TYPE_STRUCT_PTR == field.type) {
            ScopedMem<char> name(sectionName ? str::Join(sectionName, ".", field.name) : str::Dup(field.name));
            *(void **)(data + field.offset) = DeserializeRec(p, field.def, name);
            continue;
        }
        if (!section || !(line = section->FindLine(field.name))) {
            printf("couldn't find line for %s (%s)\n", field.name, sectionName);
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
            if (str::Parse(line->value, "%f", &f))
                *(float *)(data + field.offset) = f;
            break;
        case TYPE_COLOR:
            if (str::Parse(line->value, "#%2x%2x%2x", &r, &g, &b))
                *(COLORREF *)(data + field.offset) = RGB(r, g, b);
            break;
        case TYPE_STR:
            *(char **)(data + field.offset) = str::Dup(line->value);
            break;
        case TYPE_WSTR:
            *(WCHAR **)(data + field.offset) = str::conv::FromUtf8(line->value);
            break;
        default:
            CrashIf(true);
        }
    }
    return data;
}

uint8_t *Deserialize(const uint8_t *data, int dataSize, const char *version, StructMetadata *def)
{
    CrashIf(!data); // TODO: where to get defaults from?
    CrashIf(str::Len((const char *)data) != (size_t)dataSize);
    IniFile p((const char *)data);
    return (uint8_t *)DeserializeRec(p, def);
}

void SerializeRec(str::Str<char>& out, const uint8_t *data, StructMetadata *def, const char *sectionName=NULL)
{
    if (sectionName)
        out.AppendFmt("[%s]\r\n", sectionName);

    COLORREF c;
    for (size_t i = 0; i < def->nFields; i++) {
        FieldMetadata& field = def->fields[i];
        switch (field.type) {
        case TYPE_BOOL:
            out.AppendFmt("%s = %s\r\n", field.name, *(bool *)(data + field.offset) ? "true" : "false");
            break;
        case TYPE_I16:
            out.AppendFmt("%s = %d\r\n", field.name, (int32_t)*(int16_t *)(data + field.offset));
            break;
        case TYPE_U16:
            out.AppendFmt("%s = %u\r\n", field.name, (uint32_t)*(int16_t *)(data + field.offset));
            break;
        case TYPE_I32:
            out.AppendFmt("%s = %d\r\n", field.name, *(int32_t *)(data + field.offset));
            break;
        case TYPE_U32:
            out.AppendFmt("%s = %u\r\n", field.name, *(uint32_t *)(data + field.offset));
            break;
        case TYPE_U64:
            out.AppendFmt("%s = %" PRIu64 "\r\n", field.name, *(uint64_t *)(data + field.offset));
            break;
        case TYPE_FLOAT:
            out.AppendFmt("%s = %f\r\n", field.name, *(float *)(data + field.offset));
            break;
        case TYPE_COLOR:
            c = *(COLORREF *)(data + field.offset);
            out.AppendFmt("%s = #%02x%02x%02x\r\n", field.name, GetRValue(c), GetGValue(c), GetBValue(c));
            break;
        case TYPE_STR:
            if (*(const char **)(data + field.offset))
                out.AppendFmt("%s = %s\r\n", field.name, *(const char **)(data + field.offset));
            break;
        case TYPE_WSTR:
            if (*(const WCHAR **)(data + field.offset)) {
                ScopedMem<char> value(str::conv::ToUtf8(*(const WCHAR **)(data + field.offset)));
                out.AppendFmt("%s = %s\r\n", field.name, value);
            }
            break;
        case TYPE_STRUCT_PTR:
            break;
        default:
            CrashIf(true);
        }
    }

    for (size_t i = 0; i < def->nFields; i++) {
        FieldMetadata& field = def->fields[i];
        if (TYPE_STRUCT_PTR != field.type)
            continue;
        ScopedMem<char> name(sectionName ? str::Join(sectionName, ".", field.name) : str::Dup(field.name));
        SerializeRec(out, *(const uint8_t **)(data + field.offset), field.def, name);
    }
}

uint8_t *Serialize(const uint8_t *data, const char *version, StructMetadata *def, int *sizeOut)
{
    str::Str<char> out;
    out.Append(UTF8_BOM "; this file will be overwritten - modify at your own risk\r\n");
    SerializeRec(out, data, def);
    if (sizeOut)
        *sizeOut = (int)out.Size();
    return (uint8_t *)out.StealData();
}

void FreeStruct(uint8_t *data, StructMetadata *def)
{
    for (size_t i = 0; i < def->nFields; i++) {
        FieldMetadata& field = def->fields[i];
        if (TYPE_STRUCT_PTR == field.type) {
            FreeStruct(*(uint8_t **)(data + field.offset), field.def);
        }
        else if (TYPE_STR == field.type) {
            free(*(char **)(data + field.offset));
        }
        else if (TYPE_WSTR == field.type) {
            free(*(WCHAR **)(data + field.offset));
        }
    }
    free(data);
}

};

int main(int argc, char **argv)
{
#ifdef DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    const WCHAR *path = L"..\\tools\\serini_test\\data.ini";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    if (!data) {
        printf("failed to read '%S'\n", path);
        system("pause");
        return 1;
    }
    bool usedDefault;
    Settings *s = DeserializeSettings((const uint8_t *)data.Get(), str::Len(data), &usedDefault);
    if (!s) {
        printf("failed to parse '%S'\n", path);
        system("pause");
        return 1;
    }
    CrashIf(usedDefault);

    int len;
    ScopedMem<char> ser((char *)SerializeSettings(s, &len));
    CrashIf(str::Len(ser) != (size_t)len);
    CrashIf(!str::Eq(data, ser));
    FreeSettings(s);

    system("pause");
    return 0;
}
