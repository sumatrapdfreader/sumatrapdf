/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct TooltipCtrl : WindowBase {
    TooltipCtrl();
    ~TooltipCtrl() override;
    bool Create(HWND parent) override;

    Size GetIdealSize() override;

    void ShowOrUpdate(const char* s, Rect& rc, bool multiline);
    void ShowOrUpdate(const WCHAR* text, Rect& rc, bool multiline);
    void Hide();

    void SetDelayTime(int type, int timeInMs);

    void SetMaxWidth(int dx);
    int Count();
    bool IsShowing();

    HWND parent = nullptr;
};
