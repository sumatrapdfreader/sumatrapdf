/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "IniParser.h"

#include "FileUtil.h"

static inline char *SkipWs(char *s)
{
    for (; str::IsWs(*s); s++);
    return s;
}
static inline char *SkipWsRev(char *begin, char *s)
{
    for (; s > begin && str::IsWs(*(s - 1)); s--);
    return s;
}

IniLine *IniSection::FindLine(const char *key)
{
    for (size_t i = 0; i < lines.Count(); i++) {
        if (str::EqI(lines.At(i).key, key))
            return &lines.At(i);
    }
    return NULL;
}

void IniFile::ParseData()
{
    // convert the file content to UTF-8
    if (str::StartsWith(data.Get(), UTF8_BOM))
        memcpy(data, data + 3, str::Len(data + 3) + 1);
    else if (str::StartsWith(data.Get(), UTF16_BOM))
        data.Set(str::conv::ToUtf8((const WCHAR *)(data + 2)));
    else if (data)
        data.Set(str::conv::ToUtf8(ScopedMem<WCHAR>(str::conv::FromAnsi(data))));
    if (!data)
        return;

    IniSection *section = new IniSection(NULL);
    char *next = data;
    while (*next) {
        char *key = SkipWs(next);
        for (next = key; *next; next++) {
            if ('\n' == *next) {
                *next++ = '\0';
                break;
            }
        }
        if ('[' == *key || ']' == *key) {
            // section header
            char *lineEnd = SkipWsRev(key + 1, next - 1);
            if (lineEnd - 1 > key && (']' == *(lineEnd - 1) || '[' == *(lineEnd - 1)))
                lineEnd--;
            *lineEnd = '\0';
            sections.Append(section);
            section = new IniSection(key + 1);
        }
        else if (';' == *key || '#' == *key || !*key) {
            // ignore comments and empty lines
        }
        else {
            // key-value pair, separated by '=', ':' or whitepace
            char *sep;
            for (sep = key; *sep && *sep != '=' && *sep != ':'; sep++);
            if (!*sep)
                for (sep = key; !str::IsWs(*sep); sep++);
            char *value = SkipWs(sep + (*sep ? 1 : 0));
            // trim trailing whitespace
            *SkipWsRev(key, sep) = '\0';
            *SkipWsRev(value, next - 1) = '\0';
            section->lines.Append(IniLine(key, value));
        }
    }
    sections.Append(section);
}

IniSection *IniFile::FindSection(const char *name, size_t idx)
{
    for (size_t i = idx; i < sections.Count(); i++) {
        if (str::EqI(sections.At(i)->name, name))
            return sections.At(i);
    }
    return NULL;
}

IniFile::IniFile(const WCHAR *path) : data(file::ReadAll(path, NULL))
{
    ParseData();
}

IniFile::IniFile(const char *data) : data(str::Dup(data))
{
    ParseData();
}
