/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "JsonParser.h"

namespace json {

constexpr int kParseFail = -1;

static inline int SkipWS(Str data, int off) {
    while (off < data.len && str::IsWs(data.s[off])) {
        off++;
    }
    return off;
}

static inline int SkipDigits(Str data, int off) {
    while (off < data.len && str::IsDigit(data.s[off])) {
        off++;
    }
    return off;
}

class ParseArgs {
  public:
    str::Builder path;
    bool canceled = false;
    ValueVisitor* visitor = nullptr;

    explicit ParseArgs(ValueVisitor* visitor) : visitor(visitor) {}
};

static int ParseValue(ParseArgs& args, Str data, int off);

static int ExtractString(str::Builder& string, Str data, int off) {
    ReportIf(off >= data.len || data.s[off] != '"');
    off++;
    while (off < data.len) {
        char c = data.s[off];
        if ('"' == c) {
            return off + 1;
        }
        if ('\\' != c) {
            string.AppendChar(c);
            off++;
            continue;
        }
        // parse escape sequence
        off++;
        if (off >= data.len) {
            return kParseFail;
        }
        int i;
        switch (data.s[off]) {
            case '"':
            case '\\':
            case '/':
                string.AppendChar(data.s[off]);
                break;
            case 'b':
                string.AppendChar('\b');
                break;
            case 'f':
                string.AppendChar('\f');
                break;
            case 'n':
                string.AppendChar('\n');
                break;
            case 'r':
                string.AppendChar('\r');
                break;
            case 't':
                string.AppendChar('\t');
                break;
            case 'u':
                if (off + 4 < data.len && !str::IsNull(str::Parse(Str(data.s + off + 1, 4), "%4x", &i)) && 0 < i &&
                    i < 0x10000) {
                    char buf[4]{};
                    int n = 0;
                    str::Utf8Encode(buf, n, i);
                    string.Append(Str(buf, n));
                    off += 4;
                    break;
                }
                return kParseFail;
            default:
                return kParseFail;
        }
        off++;
    }
    return kParseFail;
}

static int ParseString(ParseArgs& args, Str data, int off) {
    str::Builder string;
    int end = ExtractString(string, data, off);
    if (end >= 0) {
        Str path = ToStr(args.path);
        Str value = ToStr(string);
        args.canceled = !args.visitor->Visit(path, value, Type::String);
    }
    return end;
}

static int ParseNumber(ParseArgs& args, Str data, int off) {
    int start = off;
    // integer part
    if ('-' == data.s[off]) {
        off++;
    }
    if (off >= data.len) {
        return kParseFail;
    }
    if ('0' == data.s[off]) {
        off++;
    } else if (str::IsDigit(data.s[off])) {
        off = SkipDigits(data, off + 1);
    } else {
        return kParseFail;
    }
    // fractional part
    if (off < data.len && '.' == data.s[off]) {
        off = SkipDigits(data, off + 1);
    }
    // magnitude
    if (off < data.len && ('e' == data.s[off] || 'E' == data.s[off])) {
        off++;
        if (off < data.len && ('+' == data.s[off] || '-' == data.s[off])) {
            off++;
        }
        off = SkipDigits(data, off);
    }
    // validity check
    if (off <= start || !str::IsDigit(data.s[off - 1]) || (off < data.len && str::IsDigit(data.s[off]))) {
        return kParseFail;
    }

    TempStr number = str::DupTemp(Str(data.s + start, off - start));
    Str path = ToStr(args.path);
    args.canceled = !args.visitor->Visit(path, number, Type::Number);
    return off;
}

static int ParseObject(ParseArgs& args, Str data, int off) {
    off = SkipWS(data, off + 1);
    if (off < data.len && '}' == data.s[off]) {
        return off + 1;
    }

    int pathIdx = len(args.path);
    for (;;) {
        off = SkipWS(data, off);
        if (off >= data.len || '"' != data.s[off]) {
            return kParseFail;
        }
        args.path.AppendChar('/');
        off = ExtractString(args.path, data, off);
        if (off < 0) {
            return kParseFail;
        }
        off = SkipWS(data, off);
        if (off >= data.len || ':' != data.s[off]) {
            return kParseFail;
        }

        off = ParseValue(args, data, off + 1);
        if (args.canceled || off < 0) {
            return off;
        }
        args.path.RemoveAt(pathIdx, len(args.path) - pathIdx);

        off = SkipWS(data, off);
        if (off < data.len && '}' == data.s[off]) {
            return off + 1;
        }
        if (off >= data.len || ',' != data.s[off]) {
            return kParseFail;
        }
        off++;
    }
}

static int ParseArray(ParseArgs& args, Str data, int off) {
    off = SkipWS(data, off + 1);
    if (off < data.len && ']' == data.s[off]) {
        return off + 1;
    }

    int pathIdx = len(args.path);
    for (int idx = 0;; idx++) {
        args.path.Append(fmt("[%d]", idx));
        off = ParseValue(args, data, off);
        if (args.canceled || off < 0) {
            return off;
        }
        int n = len(args.path);
        args.path.RemoveAt(pathIdx, n - pathIdx);

        off = SkipWS(data, off);
        if (off < data.len && ']' == data.s[off]) {
            return off + 1;
        }
        if (off >= data.len || ',' != data.s[off]) {
            return kParseFail;
        }
        off++;
    }
}

static int ParseKeyword(ParseArgs& args, Str data, int off, Str keyword, Type type) {
    Str rest = Str(data.s + off, data.len - off);
    if (!str::StartsWith(rest, keyword)) {
        return kParseFail;
    }
    Str path = ToStr(args.path);
    args.canceled = !args.visitor->Visit(path, keyword, type);
    return off + keyword.len;
}

static int ParseValue(ParseArgs& args, Str data, int off) {
    off = SkipWS(data, off);
    if (off >= data.len) {
        return kParseFail;
    }
    switch (data.s[off]) {
        case '"':
            return ParseString(args, data, off);
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '-':
            return ParseNumber(args, data, off);
        case '{':
            return ParseObject(args, data, off);
        case '[':
            return ParseArray(args, data, off);
        case 't':
            return ParseKeyword(args, data, off, StrL("true"), Type::Bool);
        case 'f':
            return ParseKeyword(args, data, off, StrL("false"), Type::Bool);
        case 'n':
            return ParseKeyword(args, data, off, StrL("null"), Type::Null);
        default:
            return kParseFail;
    }
}

// data must be UTF-8 encoded
// returns false on error
// return false if invalid JSON
bool Parse(Str data, ValueVisitor* visitor) {
    ParseArgs args(visitor);
    int off = 0;
    if (data.len >= 3 && str::StartsWith(data, Str(UTF8_BOM))) {
        off = 3;
    }
    int end = ParseValue(args, data, off);
    if (end < 0) {
        return false;
    }
    end = SkipWS(data, end);
    return args.canceled || end >= data.len;
}

} // namespace json
