/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/JsonParser.h"

// Simple push parser for JSON (cf. http://www.json.org/).
//
// Parse walks the input and calls onValue (Func1<Value*>) for every primitive
// (string, number, bool, null). Put state in userData via MkFunc1 / MkMethod1.
// Set Value::stop to cancel early; Parse then returns true (cancel is not an
// error). Parse returns false only on invalid JSON when not canceled.
//
// Path is a StrNode list (outermost first). Each segment's first char is the
// kind (kSegKey '/' + key, kSegIdx 'i' + decimal index). Example for
// { "key": [false, { "name": "valu\u0065" }] }:
//   1. path "/key" -> "i0", value "false", Type::Bool
//   2. path "/key" -> "i1" -> "/name", value "value", Type::String
// Keys are not escaped; a key may contain '/' or digits without ambiguity
// because each segment carries an explicit kind byte.
//
// path nodes live on the temp arena for the duration of Parse (and may be
// mutated after the callback returns when the parser pops a segment). value is
// a temp-arena copy valid until the temp arena resets. Keep a str::Dup of
// value if you need it longer; do not store path pointers past the callback.

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
    Arena* arena = nullptr;
    // path as outer→inner StrNode list (nodes live on arena)
    StrNodeList path;
    bool canceled = false;
    VisitFn onValue;

    explicit ParseArgs(const VisitFn& onValue) : arena(GetTempArena()), onValue(onValue) {}

    void PushKey(Str key) {
        char scratch[512]{};
        str::Builder b(Str(scratch, sizeofi(scratch)));
        b.AppendChar(kSegKey);
        b.Append(key);
        StrNodeListPush(&path, AllocStrNode(arena, ToStr(b)));
    }

    void PushIdx(int idx) {
        // "i" + decimal digits
        StrNodeListPush(&path, AllocStrNode(arena, fmt("%c%d", kSegIdx, idx)));
    }

    void Pop() { StrNodeListPop(&path); }
};

static int ParseValue(ParseArgs& args, Str data, int off, int depth);

static void VisitValue(ParseArgs& args, Str value, Type type) {
    Value v;
    v.path = args.path.head;
    v.value = str::DupTemp(value);
    v.type = type;
    args.onValue.Call(&v);
    args.canceled = v.stop;
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
    // reject empty match and a digit run that continues past our end
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

    for (;;) {
        off = SkipWS(data, off);
        if (off >= data.len || '"' != data.s[off]) {
            return kParseFail;
        }
        char keyScratch[512]{};
        str::Builder key(Str(keyScratch, sizeofi(keyScratch)));
        off = ExtractString(key, data, off);
        if (off < 0) {
            return kParseFail;
        }
        off = SkipWS(data, off);
        if (off >= data.len || ':' != data.s[off]) {
            return kParseFail;
        }
        args.PushKey(ToStr(key));
        off = ParseValue(args, data, off + 1, depth + 1);
        args.Pop();
        if (args.canceled || off < 0) {
            return off;
        }

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

    for (int idx = 0;; idx++) {
        args.PushIdx(idx);
        off = ParseValue(args, data, off, depth + 1);
        args.Pop();
        if (args.canceled || off < 0) {
            return off;
        }

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
// a full successful parse or if the callback set Value::stop (even if trailing
// input was not fully validated).
bool Parse(Str data, const VisitFn& onValue) {
    ParseArgs args(onValue);
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

static bool SegMatches(StrNode* seg, Str pat) {
    if (!seg || len(pat) == 0) {
        return false;
    }
    if (pat.s[0] == kSegAny) {
        return true;
    }
    return str::Eq(seg->s, pat);
}

// path matches the pattern segments exactly (same length). Empty args end the
// pattern; PathMatch(nullptr) is true (root value). Pattern segments use the
// same encoding as path nodes ('/'+key, 'i'+digits, or '*').
bool PathMatch(StrNode* path, Str a, Str b, Str c, Str d, Str e, Str f) {
    Str pats[6] = {a, b, c, d, e, f};
    int n = 0;
    while (n < 6 && len(pats[n]) > 0) {
        n++;
    }
    if (n == 0) {
        return path == nullptr;
    }
    StrNode* p = path;
    for (int i = 0; i < n; i++) {
        if (!SegMatches(p, pats[i])) {
            return false;
        }
        p = p->next;
    }
    return p == nullptr;
}

StrNode* PathBuildTemp(Str a, Str b, Str c, Str d, Str e, Str f) {
    Str pats[6] = {a, b, c, d, e, f};
    Arena* arena = GetTempArena();
    StrNode* head = nullptr;
    StrNode* tail = nullptr;
    for (int i = 0; i < 6 && len(pats[i]) > 0; i++) {
        StrNode* n = AllocStrNode(arena, pats[i]);
        if (!head) {
            head = n;
        } else {
            tail->next = n;
        }
        tail = n;
    }
    return head;
}

// Legacy string form for tests/logging: /key[0]/name
TempStr PathFormatTemp(StrNode* path) {
    str::Builder b;
    for (StrNode* p = path; p; p = p->next) {
        if (len(p->s) == 0) {
            continue;
        }
        char kind = p->s.s[0];
        Str body = Str(p->s.s + 1, p->s.len - 1);
        if (kind == kSegKey) {
            b.AppendChar(kSegKey);
            b.Append(body);
        } else if (kind == kSegIdx) {
            b.Append(fmt("[%s]", body));
        } else if (kind == kSegAny) {
            b.Append(StrL("[*]"));
        }
    }
    return ToStrTemp(b);
}

StrNode* PathNth(StrNode* path, int n) {
    StrNode* p = path;
    for (int i = 0; p && i < n; i++) {
        p = p->next;
    }
    return p;
}

int PathSegIndex(StrNode* seg) {
    if (!seg || len(seg->s) < 2 || seg->s.s[0] != kSegIdx) {
        return -1;
    }
    int v = -1;
    Str rest = Str(seg->s.s + 1, seg->s.len - 1);
    if (str::IsNull(str::Parse(rest, "%d", &v))) {
        return -1;
    }
    return v;
}

Str PathSegKey(StrNode* seg) {
    if (!seg || len(seg->s) < 1 || seg->s.s[0] != kSegKey) {
        return {};
    }
    return Str(seg->s.s + 1, seg->s.len - 1);
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
