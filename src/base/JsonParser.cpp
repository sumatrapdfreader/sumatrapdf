/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/JsonParser.h"

// Simple pull parser for JSON (cf. http://www.json.org/).
//
// Reader hands out every primitive (string, number, bool, null) in document
// order, one per Next(), with the path to it; break out of the loop to stop
// early. Ok() is false once the input proved invalid.
//
// Path is a StrNode list (outermost first). Each segment's first char is the
// kind (kSegKey '/' + key, kSegIdx 'i' + decimal index). Example for
// { "key": [false, { "name": "valu\u0065" }] }:
//   1. path "/key" -> "i0", value "false", Type::Bool
//   2. path "/key" -> "i1" -> "/name", value "value", Type::String
// Keys are not escaped; a key may contain '/' or digits without ambiguity
// because each segment carries an explicit kind byte.
//
// path nodes live on the temp arena while the reader runs (and are mutated
// when the parser pops a segment on the next step). value is a temp-arena copy
// valid until the temp arena resets. Keep a str::Dup of value if you need it
// longer; do not store path pointers past the step that produced them.

namespace json {

constexpr int kParseFail = -1;

static inline int SkipWS(Str data, int off) {
    Str s = Str(data.s + off, data.len - off);
    return off + str::TrimWs(s);
}

static inline int SkipDigits(Str data, int off) {
    while (off < data.len && str::IsDigit(data.s[off])) {
        off++;
    }
    return off;
}

// the end of the number at off, kParseFail if it isn't one
static int ScanNumber(Str data, int off) {
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
    return off;
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

constexpr int kMaxDepth = 128;

Reader::Reader(Str data) : data(data), arena(GetTempArena()) {}

bool Reader::Ok() const {
    return !failed;
}

bool Reader::Fail() {
    failed = true;
    done = true;
    return false;
}

void Reader::PushKey(Str key) {
    char scratch[512]{};
    str::Builder b;
    b.UseExternalBuffer(Str(scratch, sizeofi(scratch)));
    b.AppendChar(kSegKey);
    b.Append(key);
    StrNodeListPush(&path, AllocStrNode(arena, ToStr(b)));
}

void Reader::PushIdx(int idx) {
    // "i" + decimal digits
    StrNodeListPush(&path, AllocStrNode(arena, fmt("%c%d", kSegIdx, idx)));
}

// an object member's "key": and its path segment
bool Reader::ReadKey() {
    off = SkipWS(data, off);
    if (off >= data.len || '"' != data.s[off]) {
        return Fail();
    }
    char keyScratch[512]{};
    str::Builder key;
    key.UseExternalBuffer(Str(keyScratch, sizeofi(keyScratch)));
    off = ExtractString(key, data, off);
    if (off < 0) {
        return Fail();
    }
    off = SkipWS(data, off);
    if (off >= data.len || ':' != data.s[off]) {
        return Fail();
    }
    off++;
    PushKey(ToStr(key));
    return true;
}

// the value at off: a primitive becomes the current value; a container is
// entered and its first element read, an empty one ends like a value
bool Reader::StepValue() {
    for (;;) {
        if (len(stack) >= kMaxDepth) {
            return Fail();
        }
        off = SkipWS(data, off);
        if (off >= data.len) {
            return Fail();
        }
        char c = data.s[off];
        if (c == '{' || c == '[') {
            bool isObject = c == '{';
            off = SkipWS(data, off + 1);
            if (off < data.len && data.s[off] == (isObject ? '}' : ']')) {
                off++;
                return StepAfterValue();
            }
            VecAppend(stack, {isObject, 0});
            if (isObject) {
                if (!ReadKey()) {
                    return false;
                }
            } else {
                PushIdx(0);
            }
            continue;
        }
        if (c == '"') {
            // most strings fit in a few hundred bytes; grow to heap if not
            char scratch[512]{};
            str::Builder string;
            string.UseExternalBuffer(Str(scratch, sizeofi(scratch)));
            off = ExtractString(string, data, off);
            if (off < 0) {
                return Fail();
            }
            value.value = str::DupTemp(ToStr(string));
            value.type = Type::String;
        } else if (c == '-' || str::IsDigit(c)) {
            int end = ScanNumber(data, off);
            if (end < 0) {
                return Fail();
            }
            value.value = str::DupTemp(Str(data.s + off, end - off));
            value.type = Type::Number;
            off = end;
        } else {
            Str keyword;
            if (c == 't') {
                keyword = StrL("true");
                value.type = Type::Bool;
            } else if (c == 'f') {
                keyword = StrL("false");
                value.type = Type::Bool;
            } else if (c == 'n') {
                keyword = StrL("null");
                value.type = Type::Null;
            } else {
                return Fail();
            }
            if (!str::StartsWith(Str(data.s + off, data.len - off), keyword)) {
                return Fail();
            }
            value.value = str::DupTemp(keyword);
            off += keyword.len;
        }
        value.path = path.head;
        return true;
    }
}

// past the value just read: close the containers that end here, then go to
// the next element of the innermost open one
bool Reader::StepAfterValue() {
    for (;;) {
        if (len(stack) == 0) {
            // the root value is done; only whitespace may follow
            off = SkipWS(data, off);
            done = true;
            if (off < data.len) {
                return Fail();
            }
            return false;
        }
        StrNodeListPop(&path);
        off = SkipWS(data, off);
        if (off >= data.len) {
            return Fail();
        }
        Frame& f = stack[len(stack) - 1];
        char c = data.s[off];
        if (c == (f.isObject ? '}' : ']')) {
            off++;
            VecPop(stack);
            continue;
        }
        if (c != ',') {
            return Fail();
        }
        off++;
        if (f.isObject) {
            if (!ReadKey()) {
                return false;
            }
        } else {
            PushIdx(++f.idx);
        }
        return StepValue();
    }
}

bool Reader::Next() {
    if (done) {
        return false;
    }
    if (started) {
        if (!StepAfterValue()) {
            return false;
        }
        return true;
    }
    started = true;
    if (data.len >= 3 && str::StartsWith(data, Str(kUtf8Bom))) {
        off = 3;
    }
    return StepValue();
}

Value& Reader::iterator::operator*() const {
    return r->value;
}

Reader::iterator& Reader::iterator::operator++() {
    atEnd = !r->Next();
    return *this;
}

bool operator==(const Reader::iterator& a, const Reader::iterator& b) {
    return a.atEnd == b.atEnd;
}

bool operator!=(const Reader::iterator& a, const Reader::iterator& b) {
    return a.atEnd != b.atEnd;
}

Reader::iterator Reader::begin() {
    return {this, !Next()};
}

Reader::iterator Reader::end() {
    return {this, true};
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
                b.Append(StrL("\\\""));
                break;
            case '\\':
                b.Append(StrL("\\\\"));
                break;
            case '\n':
                b.Append(StrL("\\n"));
                break;
            case '\r':
                b.Append(StrL("\\r"));
                break;
            case '\t':
                b.Append(StrL("\\t"));
                break;
            case '\b':
                b.Append(StrL("\\b"));
                break;
            case '\f':
                b.Append(StrL("\\f"));
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
