/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct StrVec;

int FoldCaseForSearch(int c);

void SplitFilterToWords(Str filter, StrVec& words);
bool FilterMatches(Str str, const StrVec& words);
int FilterIndexOf(Str s, Str word, int* matchLenOut);
