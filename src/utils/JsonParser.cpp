/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
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
    StrBuilder path;
    bool canceled = false;
    ValueVisitor* visitor = nullptr;

    explicit ParseArgs(ValueVisitor* visitor) : visitor(visitor) {}
};

static int ParseValue(ParseArgs& args, Str data, int off);

static int ExtractString(StrBuilder& string, Str data, int off) {
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
                    char buf[5]{};
                    wchar_t wc = (wchar_t)i;
                    WideCharToMultiByte(CP_UTF8, 0, &wc, 1, buf, dimof(buf), nullptr, nullptr);
                    string.Append(buf);
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
    StrBuilder string;
    int end = ExtractString(string, data, off);
    if (end >= 0) {
        Str path = args.path.Get();
        Str value = string.Get();
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
    Str path = args.path.Get();
    args.canceled = !args.visitor->Visit(path, number, Type::Number);
    return off;
}

static int ParseObject(ParseArgs& args, Str data, int off) {
    off = SkipWS(data, off + 1);
    if (off < data.len && '}' == data.s[off]) {
        return off + 1;
    }

    size_t pathIdx = args.path.size();
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
        args.path.RemoveAt(pathIdx, args.path.size() - pathIdx);

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

    size_t pathIdx = args.path.size();
    for (int idx = 0;; idx++) {
        args.path.AppendFmt("[%d]", idx);
        off = ParseValue(args, data, off);
        if (args.canceled || off < 0) {
            return off;
        }
        size_t n = args.path.size();
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
    Str path = args.path.Get();
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
