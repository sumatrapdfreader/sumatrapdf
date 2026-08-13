/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct VirtText;

struct FrameRateWnd : WindowBase {
    FrameRateWnd() = default;
    ~FrameRateWnd() override;

    bool Create(HWND hwndAssociatedWith);

    void ShowFrameRate(int frameRate);
    void ShowFrameRateDur(double durMs);

    HWND hwndAssociatedWith = nullptr;
    HWND hwndAssociatedWithTopLevel = nullptr;

    // owned by `layout`
    VirtText* text = nullptr;

    Size maxSizeSoFar;
    int frameRate = -1;
};

int FrameRateFromDuration(double durMs);
