/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
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

Slice::Slice(const Slice& other) {
    CrashIf(this == &other);
    this->begin = other.begin;
    this->end = other.end;
    this->curr = other.curr;
}

Slice::Slice(char* s, size_t len) {
    Set(s, len);
}

Slice::Slice(char* start, char* end) {
    CrashIf(start > end);
    begin = start;
    curr = start;
    end = end;
}

void Slice::Set(char* s, size_t len) {
    begin = s;
    curr = s;
    end = s + len;
}

size_t Slice::Left() const {
    CrashIf(curr > end);
    return end - curr;
}

bool Slice::Finished() const {
    return curr >= end;
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
    size_t res = s - curr;
    curr = s;
    return res;
}

// returns number of characters skipped
size_t Slice::SkipNonWs() {
    char* start = curr;
    while (!Finished()) {
        if (IsWsOrNewline(*curr)) {
            break;
        }
        curr++;
    }
    return curr - start;
}

// advances to a given character or end
size_t Slice::SkipUntil(char toFind) {
    char* start = curr;
    while (!Finished()) {
        if (*curr == toFind) {
            break;
        }
        curr++;
    }
    return curr - start;
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
    char* start = curr;
    while ((curr < end) && (n > 0)) {
        ++curr;
        --n;
    }
    return curr - start;
}

void Slice::ZeroCurr() {
    if (curr < end) {
        *curr = 0;
    }
}

} // namespace str
