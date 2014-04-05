/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "JsonParser.h"

namespace json {

static inline const char *SkipWS(const char *s)
{
    while (str::IsWs(*s))
        s++;
    return s;
}

static inline const char *SkipDigits(const char *s)
{
    while (str::IsDigit(*s))
        s++;
    return s;
}

class ParseArgs {
public:
    str::Str<char> path;
    bool canceled;
    ValueVisitor *visitor;

    explicit ParseArgs(ValueVisitor *visitor) : canceled(false), visitor(visitor) { }
};

static const char *ParseValue(ParseArgs& args, const char *data);

static const char *ExtractString(str::Str<char>& string, const char *data)
{
    while (*++data) {
        if ('"' == *data)
            return data + 1;
        if ('\\' != *data) {
            string.Append(*data);
            continue;
        }
        // parse escape sequence
        int i;
        switch (*++data) {
            case '"': case '\\': case '/':
                string.Append(*data);
                break;
            case 'b': string.Append('\b'); break;
            case 'f': string.Append('\f'); break;
            case 'n': string.Append('\n'); break;
            case 'r': string.Append('\r'); break;
            case 't': string.Append('\t'); break;
            case 'u':
                if (str::Parse(data + 1, "%4x", &i) && 0 < i && i < 0x10000) {
                    char buf[5] = { 0 };
                    wchar_t c = (wchar_t)i;
                    WideCharToMultiByte(CP_UTF8, 0, &c, 1, buf, dimof(buf), NULL, NULL);
                    string.Append(buf);
                    data += 4;
                    break;
                }
                return NULL;
            default:
                return NULL;
        }
    }
    return NULL;
}

static const char *ParseString(ParseArgs& args, const char *data)
{
    str::Str<char> string;
    data = ExtractString(string, data);
    if (data)
        args.canceled = !args.visitor->Visit(args.path.Get(), string.Get(), Type_String);
    return data;
}

static const char *ParseNumber(ParseArgs& args, const char *data)
{
    const char *start = data;
    // integer part
    if ('-' == *data)
        data++;
    if ('0' == *data)
        data++;
    else if (str::IsDigit(*data))
        data = SkipDigits(data + 1);
    else
        return NULL;
    // fractional part
    if ('.' == *data)
        data = SkipDigits(data + 1);
    // magnitude
    if ('e' == *data || 'E' == *data) {
        data++;
        if ('+' == *data || '-' == *data)
            data++;
        data = SkipDigits(data + 1);
    }
    // validity check
    if (!str::IsDigit(*(data - 1)) || str::IsDigit(*data))
        return NULL;

    char *number = str::DupN(start, data - start);
    args.canceled = !args.visitor->Visit(args.path.Get(), number, Type_Number);
    free(number);
    return data;
}

static const char *ParseObject(ParseArgs& args, const char *data)
{
    data = SkipWS(data + 1);
    if ('}' == *data)
        return data + 1;

    size_t pathIdx = args.path.Size();
    for (;;) {
        data = SkipWS(data);
        if ('"' != *data)
            return NULL;
        args.path.Append('/');
        data = ExtractString(args.path, data);
        if (!data)
            return NULL;
        data = SkipWS(data);
        if (':' != *data)
            return NULL;

        data = ParseValue(args, data + 1);
        if (args.canceled || !data)
            return data;
        args.path.RemoveAt(pathIdx, args.path.Size() - pathIdx);

        data = SkipWS(data);
        if ('}' == *data)
            return data + 1;
        if (',' != *data)
            return NULL;
        data++;
    }
}

static const char *ParseArray(ParseArgs& args, const char *data)
{
    data = SkipWS(data + 1);
    if (']' == *data)
        return data + 1;

    size_t pathIdx = args.path.Size();
    for (int idx = 0; ; idx++) {
        args.path.AppendFmt("[%d]", idx);
        data = ParseValue(args, data);
        if (args.canceled || !data)
            return data;
        args.path.RemoveAt(pathIdx, args.path.Size() - pathIdx);

        data = SkipWS(data);
        if (']' == *data)
            return data + 1;
        if (',' != *data)
            return NULL;
        data++;
    }
}

static const char *ParseKeyword(ParseArgs& args, const char *data,
                                const char *keyword, DataType type)
{
    if (!str::StartsWith(data, keyword))
        return NULL;
    args.canceled = !args.visitor->Visit(args.path.Get(), keyword, type);
    return data + str::Len(keyword);
}

static const char *ParseValue(ParseArgs& args, const char *data)
{
    data = SkipWS(data);
    switch (*data) {
    case '"':
        return ParseString(args, data);
    case '0': case '1': case '2': case '3':
    case '4': case '5': case '6': case '7':
    case '8': case '9': case '-':
        return ParseNumber(args, data);
    case '{':
        return ParseObject(args, data);
    case '[':
        return ParseArray(args, data);
    case 't':
        return ParseKeyword(args, data, "true", Type_Bool);
    case 'f':
        return ParseKeyword(args, data, "false", Type_Bool);
    case 'n':
        return ParseKeyword(args, data, "null", Type_Null);
    default:
        return NULL;
    }
}

bool Parse(const char *data, ValueVisitor *visitor)
{
    ParseArgs args(visitor);
    if (str::StartsWith(data, UTF8_BOM))
        data += 3;
    const char *end = ParseValue(args, data);
    if (!end)
        return false;
    return args.canceled || !*SkipWS(end);
}

}
