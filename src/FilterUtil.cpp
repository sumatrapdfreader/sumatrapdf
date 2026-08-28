/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "FilterUtil.h"

bool FilterMatches(Str str, const StrVec& words) {
    for (Str word : words) {
        if (word && !str::ContainsI(str, word)) {
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
