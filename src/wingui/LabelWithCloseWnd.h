/* Copyright 2018 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

class LabelWithCloseWnd {
  public:
    LabelWithCloseWnd() = default;
    ~LabelWithCloseWnd() = default;

    bool Create(HWND parent, int cmd);
    void SetLabel(const WCHAR*);
    void SetFont(HFONT);
    void SetBgCol(COLORREF);
    void SetTextCol(COLORREF);
    void SetPaddingXY(int x, int y);
    SizeI GetIdealSize();

    HWND hwnd;
    HFONT font;
    int cmd;

    RectI closeBtnPos;
    COLORREF txtCol;
    COLORREF bgCol;

    // in points
    int padX, padY;
};
