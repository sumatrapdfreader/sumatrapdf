/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct VirtWnd;

struct VirtWnd : LayoutBase {
    VirtWnd();
    ~VirtWnd() = default;

    // ILayout
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    Size Layout(Constraints bc) override;

    virtual Size GetIdealSize();

    virtual void Paint(HDC);

    VirtWnd* next = nullptr;
    VirtWnd* firstChild = nullptr;

    // position within HWND
    Rect bounds;

    HWND hwnd = nullptr;

    Visibility visibility = Visibility::Collapse;
};

struct VirtWndText : VirtWnd {
    const char* s = nullptr;
    HFONT font = nullptr;
    bool withUnderline = false;
    bool isRtl = false;
    COLORREF textColor = kColorUnset;

    Size sz = {0, 0};

    VirtWndText(HWND hwnd, const char* s, HFONT font = nullptr);
    ~VirtWndText();

    // ILayout
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    Size Layout(const Constraints bc) override;

    Size MinIntrinsicSize(int width, int height);
    Size GetIdealSize(bool onlyIfEmpty = false);

    void Paint(HDC dc) override;
};
