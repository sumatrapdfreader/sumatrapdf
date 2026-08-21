/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct StrVec;

void SplitFilterToWords(Str filter, StrVec& words);
bool FilterMatches(Str str, const StrVec& words);
