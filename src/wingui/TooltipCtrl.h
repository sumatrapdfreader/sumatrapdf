/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct TooltipCtrl : public WindowBase {
    bool isShowing = false;

    TooltipCtrl(HWND parent);
    ~TooltipCtrl() override;
    bool Create() override;

    SIZE GetIdealSize() override;

    void Show(std::string_view s, RectI& rc, bool multiline);
    void Show(const WCHAR* text, RectI& rc, bool multiline);
    void Hide();

    void SetMaxWidth(int dx);
};
