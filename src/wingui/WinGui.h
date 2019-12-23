/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

typedef std::function<LRESULT(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, bool& didHandle)> MsgFilter;

struct WmCommandArgs {
    HWND hwnd = nullptr;
    int id = 0;
    int ev = 0;
    WPARAM wparam = 0;
    LPARAM lparam = 0;
    bool didHandle = false;

    LRESULT result = 0;
};

typedef std::function<void(WmCommandArgs*)> OnWmCommand;
typedef std::function<void(HWND, int dx, int dy, WPARAM resizeType)> OnSize;
typedef std::function<void(HWND, int xMouseScreen, int yMouseScreen)> ContextMenuCb;

SIZE MeasureTextInHwnd(HWND hwnd, const WCHAR* txt, HFONT font);
