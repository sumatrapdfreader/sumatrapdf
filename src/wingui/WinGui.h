/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

SizeI MeasureTextInHwnd(HWND hwnd, const WCHAR* txt, HFONT font);
char* getWinMessageName(UINT msg);
void dbgLogMsg(char* prefix, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
