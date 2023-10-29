/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct HwndWidgetText : LayoutBase {
    const char* s = nullptr;
    HWND hwnd = nullptr;
    HFONT font = nullptr;
    bool withUnderline = false;
    bool isRtl = false;

    Size sz = {0, 0};

    HwndWidgetText(const char* s, HWND hwnd, HFONT font = nullptr);

    // ILayout
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    Size Layout(const Constraints bc) override;

    Size MinIntrinsicSize(int width, int height);
    Size Measure(bool onlyIfEmpty = false);
    void Draw(HDC dc);
};
