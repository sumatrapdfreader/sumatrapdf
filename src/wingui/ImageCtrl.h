/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct ImageCtrl : WindowBase {
    // we don't own it
    Gdiplus::Bitmap* bmp = nullptr;

    ImageCtrl(HWND parent);
    ~ImageCtrl() override;
    bool Create() override;

    Size GetIdealSize() override;
};
