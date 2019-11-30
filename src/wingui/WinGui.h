/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

typedef std::function<LRESULT(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, bool& didHandle)> MsgFilter;
typedef std::function<LRESULT(HWND, int id, int event, LPARAM lp, bool& didHandle)> WmCommandCb;
typedef std::function<void(HWND, int dx, int dy, WPARAM resizeType)> SizeCb;
typedef std::function<void(HWND, int xMouseScreen, int yMouseScreen)> ContextMenuCb;

SIZE MeasureTextInHwnd(HWND hwnd, const WCHAR* txt, HFONT font);
