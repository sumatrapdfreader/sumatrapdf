/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

const char* GetWinMessageName(UINT msg);
void DbgLogMsg(const char* prefix, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
