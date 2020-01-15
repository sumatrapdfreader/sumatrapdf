/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"

// utilities for string view
namespace sv {

ParsedKV::~ParsedKV() {
    free(key);
    free(val);
}

static void parsedKVMove(ParsedKV* t, ParsedKV* that) noexcept {
    if (t == that) {
        return;
    }
    t->ok = that->ok;
    t->key = that->key;
    t->val = that->val;
    that->key = nullptr;
    that->val = nullptr;
}

ParsedKV& ParsedKV::operator=(ParsedKV&& that) noexcept {
    parsedKVMove(this, &that);
    return *this;
}

ParsedKV::ParsedKV(ParsedKV&& that) noexcept {
    parsedKVMove(this, &that);
}

bool StartsWith(std::string_view s, std::string_view prefix) {
    auto plen = prefix.length();
    auto slen = s.length();
    if (plen > slen) {
        return false;
    }
    return str::EqN(s.data(), prefix.data(), plen);
}

bool StartsWith(std::string_view s, const char* prefix) {
    auto p = std::string_view(prefix);
    return StartsWith(s, p);
}

// returns a copy of <s> where newlines are normalized to LF ('\n')
// TODO: optimize
std::string_view NormalizeNewlines(std::string_view s) {
    if (s.empty()) {
        return {};
    }
    str::Str tmp(s);
    tmp.Replace("\r\n", "\n");
    tmp.Replace("\r", "\n");
    return tmp.StealAsView();
}

Vec<std::string_view> Split(std::string_view sv, char split, size_t max) {
    Vec<std::string_view> res;
    const char* s = sv.data();
    const char* end = s + sv.size();
    if (max > 0) {
        // we want to stop at max - 1 because we also add
        max--;
    }
    const char* prev = s;
    while (s < end) {
        char c = *s;
        if (c == split) {
            size_t size = s - prev;
            res.push_back({prev, size});
            prev = s + 1;
            if (max != 0 && max == res.size()) {
                break;
            }
        }
        ++s;
    }
    // add the rest if non-empty
    size_t size = end - prev;
    if (size > 0) {
        res.push_back({prev, size});
    }
    return res;
}

std::string_view TrimSpace(std::string_view str) {
    const char* s = str.data();
    const char* end = s + str.size();
    while (s < end && str::IsWs(*s)) {
        ++s;
    }
    while (end > s) {
        char c = end[-1];
        if (!str::IsWs(c)) {
            break;
        }
        --end;
    }
    size_t size = end - s;
    return {s, size};
}

// update sv to skip first n characters
size_t SkipN(std::string_view& sv, size_t n) {
    CrashIf(n > sv.size());
    const char* s = sv.data() + n;
    size_t newSize = sv.size() - n;
    sv = {s, newSize};
    return n;
}

// updat svn to skip until end
size_t SkipTo(std::string_view& sv, const char* end) {
    const char* s = sv.data();
    CrashIf(end < s);
    size_t toSkip = end - s;
    CrashIf(toSkip > sv.size());
    SkipN(sv, toSkip);
    return toSkip;
}

// returns a substring of sv until delim or end of string
// updates sv to reflect the rest of the string
// meant of iterative calls so updates 'sv' in place
// return { nullptr, 0 } to indicate finished iteration
std::string_view ParseUntil(std::string_view& sv, char delim) {
    const char* s = sv.data();
    const char* e = s + sv.size();
    const char* start = s;
    if (s == e) {
        return {nullptr, 0};
    }
    while (s < e) {
        if (*s == delim) {
            break;
        }
        s++;
    }
    // skip one past delim
    size_t n = SkipTo(sv, s);
    // skip delim
    if (s < e) {
        SkipN(sv, 1);
    }
    return {start, n};
}

std::string_view ParseUntilBack(std::string_view& sv, char delim) {
    const char* start = sv.data();
    const char* end = start + sv.size();
    if (start == end) {
        return {nullptr, 0};
    }
    const char* s = end - 1;
    while (s >= start) {
        if (*s == delim) {
            break;
        }
        s--;
    }
    size_t size = (size_t)(end - s - 1);
    std::string_view el = {s + 1, size};
    size_t newSize = sv.size() - size;
    if (newSize > 0) {
        // eat delim
        newSize--;
    }
    sv = {start, newSize};
    return el;
}

// skips all c chars in the beginning of sv
// returns number of chars skipped
// TODO: rename trimLeft?
size_t SkipChars(std::string_view& sv, char c) {
    const char* s = sv.data();
    const char* e = s + sv.size();
    while (s < e) {
        if (*s != c) {
            break;
        }
        s++;
    }
    return SkipTo(sv, s);
}

static bool CharNeedsQuoting(char c) {
    switch (c) {
        case '"':
        case '\\':
        case '\n':
        case '\r':
        case '\t':
        case '\b':
        case '\f':
            return true;
    }
    return false;
}

static bool NeedsQuoting(std::string_view sv) {
    const char* s = sv.data();
    const char* end = s + sv.size();
    while (s < end) {
        char c = *s;
        if (c == ' ' || CharNeedsQuoting(c)) {
            return true;
        }
        s++;
    }
    return false;
}

static char quoteChar(char c) {
    switch (c) {
        case '"':
        case '\\':
            return c;
        case '\n':
            return 'n';
        case '\r':
            return 'r';
        case '\t':
            return 't';
        case '\b':
            return 'b';
        case '\f':
            return 'f';
    }
    CrashIf(true);
    return c;
}

static bool unquoteChar(char& c) {
    switch (c) {
        case '"':
        case '\\':
            return true;
        case 'n':
            c = '\n';
            return true;
        case 'r':
            c = '\r';
            return true;
        case 't':
            c = '\t';
            return true;
        case 'b':
            c = '\b';
            return true;
        case 'f':
            c = '\f';
            return true;
    }
    return false;
}

void AppendQuoted(std::string_view sv, str::Str& out) {
    out.AppendChar('"');
    const char* s = sv.data();
    const char* end = s + sv.size();
    while (s < end) {
        auto c = *s;
        if (CharNeedsQuoting(c)) {
            out.AppendChar('\\');
            c = quoteChar(c);
        }
        out.AppendChar(c);
        s++;
    }
    out.AppendChar('"');
}

bool AppendMaybeQuoted(std::string_view sv, str::Str& out) {
    if (NeedsQuoting(sv)) {
        AppendQuoted(sv, out);
        return true;
    }
    out.AppendView(sv);
    return false;
}

// if <sv> starts with '"' it's quoted value that should end with '"'
// otherwise it's unquoted value that ends with ' '
// returns false if starts with '"' but doesn't end with '"'
// sets <out> to parsed value
// updates <sv> to consume parsed characters
bool ParseMaybeQuoted(std::string_view& sv, str::Str& out, bool full) {
    if (sv.size() == 0) {
        // empty value is ok
        return true;
    }
    const char* s = sv.data();
    const char* end = s + sv.size();
    char c = *s;
    if (c != '"') {
        // unqoted, parse until end of line or end of text
        if (full) {
            std::string_view v = ParseUntil(sv, '\n');
            out.AppendView(v);
            return true;
        }
        std::string_view v = ParseUntil(sv, ' ');
        out.AppendView(v);
        return true;
    }
    s++;
    while (s < end) {
        c = *s;
        if (c == '"') {
            s++;
            SkipTo(sv, s);
            return true;
        }
        if (c != '\\') {
            out.AppendChar(c);
            s++;
            continue;
        }
        // possibly un-quoting
        s++;
        if (s >= end) {
            // TODO: allow it?
            return false;
        }
        c = *s;
        bool unquoted = unquoteChar(c);
        if (unquoted) {
            out.AppendChar(c);
        } else {
            out.AppendChar('\\');
            out.AppendChar(c);
        }
        s++;
    }
    // started with '"' but didn't end with it
    return false;
}

// find key (':', ' ' or end of text) in <sv>
static std::string_view parseKey(std::string_view& sv) {
    sv::SkipChars(sv, ' ');
    const char* s = sv.data();
    const char* end = s + sv.size();

    const char* keyStart = s;
    const char* keyEnd = end;
    while (s < end) {
        char c = *s;
        if (c == ':' || c == ' ') {
            keyEnd = s;
            s++;
            break;
        }
        s++;
    }
    size_t keySize = (keyEnd - keyStart);
    sv::SkipTo(sv, s);
    sv::SkipChars(sv, ' ');
    return {keyStart, keySize};
}

// <str> could be:
// "key"
// "key:unquoted-value"
// "key:"quoted value"
// updates <str> in place to account for parsed data
ParsedKV ParseKV(std::string_view& sv, bool full) {
    ParsedKV res;
    std::string_view key = parseKey(sv);
    if (key.empty()) {
        res.ok = false;
        return res;
    }
    res.key = str::Dup(key);

    str::Str val;
    res.ok = ParseMaybeQuoted(sv, val, full);
    res.val = val.StealData();
    return res;
}

// parse key/value out of <s>, expecting a given <key>
ParsedKV ParseValueOfKey(std::string_view& str, std::string_view key, bool full) {
    ParsedKV res = ParseKV(str, full);
    if (res.ok) {
        res.ok = str::Eq(key, res.key);
    }
    return res;
}

} // namespace sv
