/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct LabelWithCloseWnd {
    LabelWithCloseWnd() = default;
    ~LabelWithCloseWnd() = default;

    bool Create(HWND parent, int cmd);
    void SetLabel(const WCHAR*) const;
    void SetFont(HFONT);
    void SetBgCol(COLORREF);
    void SetTextCol(COLORREF);
    void SetPaddingXY(int x, int y);
    Size GetIdealSize() const;

    HWND hwnd = nullptr;
    HFONT font = nullptr;
    int cmd = 0;

    Rect closeBtnPos{};
    COLORREF txtCol = 0;
    COLORREF bgCol = 0;

    // in points
    int padX = 0;
    int padY = 0;
};

struct LabelWithCloseCtrl : public Window {
    LabelWithCloseCtrl();
    ~LabelWithCloseCtrl() override;

    bool Create(HWND parent, const WCHAR*);

    void SetLabel(const WCHAR*);

    void SetPaddingXY(int x, int y);
    Size GetIdealSize() override;

    Rect closeBtnPos{};

    // in points
    int padX = 0;
    int padY = 0;
};
