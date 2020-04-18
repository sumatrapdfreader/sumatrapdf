/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct TooltipCtrl : WindowBase {
    bool isShowing = false;

    TooltipCtrl(HWND parent);
    ~TooltipCtrl() override;
    bool Create() override;

    SizeI GetIdealSize() override;

    void Show(std::string_view s, Rect& rc, bool multiline);
    void Show(const WCHAR* text, Rect& rc, bool multiline);
    void Hide();

    void SetMaxWidth(int dx);
};
