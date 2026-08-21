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
    int i = 0;
    while (i < len(filter) && filter.s[i]) {
        while (i < len(filter) && str::IsWs(filter.s[i])) {
            i++;
        }
        if (i >= len(filter) || !filter.s[i]) {
            break;
        }
        int start = i;
        while (i < len(filter) && filter.s[i] && !str::IsWs(filter.s[i])) {
            i++;
        }
        Str word(filter.s + start, i - start);
        if (word) {
            AppendIfNotExists(&words, word);
        }
    }
}
