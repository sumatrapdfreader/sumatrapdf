/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct LabelWithCloseCreateArgs {
    HWND parent = nullptr;
    HFONT font = nullptr;
    int cmdId = 0;
};

struct LabelWithCloseWnd : Wnd {
    LabelWithCloseWnd() = default;
    ~LabelWithCloseWnd() = default;

    HWND Create(const LabelWithCloseCreateArgs&);

    void OnPaint(HDC hdc, PAINTSTRUCT* ps) override;
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;

    void SetLabel(const char*) const;
    void SetFont(HFONT);
    void SetPaddingXY(int x, int y);

    Size GetIdealSize();

    int cmdId = 0;

    Rect closeBtnPos{};

    // in points
    int padX = 0;
    int padY = 0;
};
