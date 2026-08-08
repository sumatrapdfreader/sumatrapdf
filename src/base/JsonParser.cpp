/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/JsonParser.h"

// Simple push parser for JSON (cf. http://www.json.org/).
//
// Parse walks the input and calls ValueVisitor::Visit for every primitive
// (string, number, bool, null) with a path and the value's string form.
// Return false from Visit to stop early; Parse then returns true (cancel is
// not an error). Parse returns false only on invalid JSON when not canceled.
//
// Path examples for { "key": [false, { "name": "valu\u0065" }] }:
//   1. "/key[0]", "false", Type::Bool
//   2. "/key[1]/name", "value", Type::String
// Object keys are appended raw after '/'; array indices as "[n]". Keys that
// contain '/' or '[' are not escaped, so path matching should use full strings.
//
// path and value passed to Visit are temp-arena copies (valid until the temp
// arena is reset, typically at the end of the message loop). Keep a str::Dup
// if you need them longer.

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
    // JSON paths are usually short ("/foo/bar/0/name"); grow to heap if deeper.
    char pathScratch[256]{};
    str::Builder path;
    bool canceled = false;
    ValueVisitor* visitor = nullptr;

    explicit ParseArgs(ValueVisitor* visitor) : path(Str(pathScratch, sizeofi(pathScratch))), visitor(visitor) {}
};

static int ParseValue(ParseArgs& args, Str data, int off, int depth);

// Hand Visit temp-arena copies so path/value share one lifetime (until the
// temp arena resets). Builders and stack scratch are never exposed to callers.
static void VisitValue(ParseArgs& args, Str value, Type type) {
    TempStr path = str::DupTemp(ToStr(args.path));
    TempStr valueTemp = str::DupTemp(value);
    args.canceled = !args.visitor->Visit(path, valueTemp, type);
}

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
                // \u0000 is valid JSON; accept the full BMP range 0..0xFFFF.
                if (off + 4 < data.len && !str::IsNull(str::Parse(Str(data.s + off + 1, 4), "%4x", &i)) &&
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
    // Most JSON string values fit in a few hundred bytes; grow to heap if not.
    char stringScratch[512]{};
    str::Builder string(Str(stringScratch, sizeofi(stringScratch)));
    int end = ExtractString(string, data, off);
    if (end >= 0) {
        VisitValue(args, ToStr(string), Type::String);
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
    // fractional part: '.' must be followed by at least one digit
    if (off < data.len && '.' == data.s[off]) {
        int fracStart = off + 1;
        off = SkipDigits(data, fracStart);
        if (off == fracStart) {
            return kParseFail;
        }
    }
    // magnitude: 'e'/'E' and optional sign must be followed by at least one digit
    if (off < data.len && ('e' == data.s[off] || 'E' == data.s[off])) {
        off++;
        if (off < data.len && ('+' == data.s[off] || '-' == data.s[off])) {
            off++;
        }
        int expStart = off;
        off = SkipDigits(data, off);
        if (off == expStart) {
            return kParseFail;
        }
    }
    // reject empty match and a digit run that continues past our end (e.g. after a failed path)
    if (off <= start || (off < data.len && str::IsDigit(data.s[off]))) {
        return kParseFail;
    }

    VisitValue(args, Str(data.s + start, off - start), Type::Number);
    return off;
}

static int ParseObject(ParseArgs& args, Str data, int off, int depth) {
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

        off = ParseValue(args, data, off + 1, depth + 1);
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

static int ParseArray(ParseArgs& args, Str data, int off, int depth) {
    off = SkipWS(data, off + 1);
    if (off < data.len && ']' == data.s[off]) {
        return off + 1;
    }

    int pathIdx = len(args.path);
    for (int idx = 0;; idx++) {
        args.path.Append(fmt("[%d]", idx));
        off = ParseValue(args, data, off, depth + 1);
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
    VisitValue(args, keyword, type);
    return off + keyword.len;
}

static int ParseValue(ParseArgs& args, Str data, int off, int depth) {
    if (depth >= 128) {
        return kParseFail;
    }
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
            return ParseObject(args, data, off, depth);
        case '[':
            return ParseArray(args, data, off, depth);
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

// data must be UTF-8 encoded. Returns false on invalid JSON; returns true on
// a full successful parse or if the visitor canceled early (even if trailing
// input was not fully validated).
bool Parse(Str data, ValueVisitor* visitor) {
    ParseArgs args(visitor);
    int off = 0;
    if (data.len >= 3 && str::StartsWith(data, Str(UTF8_BOM))) {
        off = 3;
    }
    int end = ParseValue(args, data, off, 0);
    if (end < 0) {
        return false;
    }
    end = SkipWS(data, end);
    return args.canceled || end >= data.len;
}

// Escapes s so it can sit inside a double-quoted JSON string; the surrounding
// quotes are not added. Text that gets pasted into a JSON body routinely
// contains quotes and newlines, and either produces invalid JSON if copied in
// raw.
//
// utf-8 continuation bytes pass through untouched: JSON is utf-8, so multi-byte
// characters need no escaping. Everything below 0x20 must be escaped, and the
// ones without a short form get \u00XX.
//
// Also safe for a double-quoted JavaScript string literal (WebView.cpp uses
// it that way): U+2028 LINE SEPARATOR and U+2029 PARAGRAPH SEPARATOR are valid
// unescaped in JSON but are line terminators in JS, so they are escaped here.
TempStr EscapeStrTemp(Str s) {
    str::Builder b;
    int n = len(s);
    for (int i = 0; i < n; i++) {
        u8 c = (u8)s.s[i];
        // U+2028 = e2 80 a8, U+2029 = e2 80 a9
        if (c == 0xE2 && i + 2 < n && (u8)s.s[i + 1] == 0x80 && ((u8)s.s[i + 2] == 0xA8 || (u8)s.s[i + 2] == 0xA9)) {
            b.Append(fmt("\\u%04x", (u8)s.s[i + 2] == 0xA8 ? 0x2028 : 0x2029));
            i += 2;
            continue;
        }
        switch (c) {
            case '"':
                b.Append("\\\"");
                break;
            case '\\':
                b.Append("\\\\");
                break;
            case '\n':
                b.Append("\\n");
                break;
            case '\r':
                b.Append("\\r");
                break;
            case '\t':
                b.Append("\\t");
                break;
            case '\b':
                b.Append("\\b");
                break;
            case '\f':
                b.Append("\\f");
                break;
            default:
                if (c < 0x20) {
                    b.Append(fmt("\\u%04x", (int)c));
                } else {
                    b.AppendChar((char)c);
                }
                break;
        }
    }
    return ToStrTemp(b);
}

} // namespace json
