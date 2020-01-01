/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "StrSlice.h"

namespace str {

static inline bool IsWsOrNewline(char c) {
    return (' ' == c) || ('\r' == c) || ('\t' == c) || ('\n' == c);
}

static inline bool IsWsNoNewline(char c) {
    return (' ' == c) || ('\r' == c) || ('\t' == c);
}

Slice::Slice() {
    // nothing to do
}

Slice::Slice(char* s, char* e) : begin(s), end(e), curr(s) {
    CrashIf(begin > end);
}

Slice::Slice(char* s, size_t len) {
    Set(s, len);
}

void Slice::Set(char* s, size_t len) {
    begin = s;
    curr = s;
    end = s + len;
}

Slice::Slice(const Slice& other) {
    CrashIf(this == &other);
    this->begin = other.begin;
    this->end = other.end;
    this->curr = other.curr;
}

size_t Slice::Left() const {
    CrashIf(curr > end);
    return end - curr;
}

bool Slice::Finished() const {
    return curr >= end;
}

size_t Slice::AdvanceCurrTo(char* s) {
    CrashIf(curr > s);
    size_t res = s - curr;
    curr = s;
    return res;
}

// returns number of characters skipped
size_t Slice::SkipWsUntilNewline() {
    // things are faster if those are locals and not
    char* s = curr;
    char* e = end;
    while (s < e) {
        if (!IsWsNoNewline(*s)) {
            break;
        }
        s++;
    }
    return AdvanceCurrTo(s);
}

// returns number of characters skipped
size_t Slice::SkipNonWs() {
    char* s = curr;
    char* e = end;
    while (s < e) {
        if (IsWsOrNewline(*s)) {
            break;
        }
        s++;
    }
    return AdvanceCurrTo(s);
}

// advances to a given character or end
size_t Slice::SkipUntil(char toFind) {
    char* s = curr;
    char* e = end;
    while (s < e) {
        if (toFind == *s) {
            break;
        }
        s++;
    }
    return AdvanceCurrTo(s);
}

char Slice::PrevChar() const {
    if (curr > begin) {
        return curr[-1];
    }
    return 0;
}

char Slice::CurrChar() const {
    if (curr < end) {
        return *curr;
    }
    return 0;
}

// skip up to n characters
// returns the number of characters skipped
size_t Slice::Skip(int n) {
    size_t toSkip = std::min<size_t>(Left(), (size_t)n);
    curr += toSkip;
    CrashIf(curr > end);
    return toSkip;
}

void Slice::ZeroCurr() {
    if (curr < end) {
        *curr = 0;
    }
}

} // namespace str
