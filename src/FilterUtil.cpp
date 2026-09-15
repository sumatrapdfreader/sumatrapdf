/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "FilterUtil.h"

// lower-case and strip diacritics: 'Ł' -> 'l', 'é' -> 'e'
static int FoldRune(int c) {
    return FoldDiacriticsRune(FoldCaseRune(c));
}

// next rune of s at byteIdx, folded, skipping combining marks. 0 at end
static int NextFoldedRune(Str s, int& byteIdx) {
    while (byteIdx < s.len) {
        int c = Utf8CodepointNext(s, byteIdx);
        if (!IsCombiningMark(c)) {
            return FoldRune(c);
        }
    }
    return 0;
}

// case and diacritic insensitive search of word in s
// returns byte offset in s (or -1) and byte length of the match in s
int FilterIndexOf(Str s, Str word, int* matchLenOut) {
    if (len(s) == 0 || len(word) == 0) {
        return -1;
    }
    for (int start = 0; start < s.len;) {
        int si = start;
        int wi = 0;
        bool matched = true;
        while (wi < word.len) {
            int wc = NextFoldedRune(word, wi);
            if (wc == 0) {
                break;
            }
            if (NextFoldedRune(s, si) != wc) {
                matched = false;
                break;
            }
        }
        if (matched && si > start) {
            // include trailing combining marks of the last matched rune
            int end = si;
            while (end < s.len) {
                int next = end;
                if (!IsCombiningMark(Utf8CodepointNext(s, next))) {
                    break;
                }
                end = next;
            }
            if (matchLenOut) {
                *matchLenOut = end - start;
            }
            return start;
        }
        Utf8CodepointNext(s, start);
    }
    return -1;
}

bool FilterMatches(Str str, const StrVec& words) {
    for (Str word : words) {
        if (word && FilterIndexOf(str, word, nullptr) < 0) {
            return false;
        }
    }
    return true;
}

void SplitFilterToWords(Str filter, StrVec& words) {
    Str rest = filter;
    while (Str word = str::NextWord(rest)) {
        AppendIfNotExists(&words, word);
    }
}
