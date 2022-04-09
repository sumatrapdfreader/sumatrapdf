/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct ImageCtrl : WindowBase {
    // we don't own it
    Gdiplus::Bitmap* bmp = nullptr;

    ImageCtrl();
    ~ImageCtrl() override;
    bool Create(HWND parent) override;

    Size GetIdealSize() override;
};
