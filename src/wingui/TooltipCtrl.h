/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct TooltipCtrl : WindowBase {
    str::WStr text;

    explicit TooltipCtrl(HWND parent);
    ~TooltipCtrl() override;
    bool Create() override;

    Size GetIdealSize() override;

    void Show(std::string_view s, Rect& rc, bool multiline);
    void Show(const WCHAR* text, Rect& rc, bool multiline);
    void Hide();

    void SetDelayTime(int type, int timeInMs);

    void SetMaxWidth(int dx);
};
