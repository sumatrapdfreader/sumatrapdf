/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct VirtWnd : public ILayout {
    VirtWnd();
    ~VirtWnd() = default;

    // ILayout
    Kind GetKind() override;
    void SetVisibility(Visibility) override;
    Visibility GetVisibility() override;
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    Size Layout(Constraints bc) override;
    void SetBounds(Rect) override;

    void Paint(HDC);

    Kind kind = nullptr;
    // each virtual window is associated with an parent window
    HWND hwnd = nullptr;

    // position within HWND
    Rect bounds;
    Visibility visibility = Visibility::Collapse;
};
