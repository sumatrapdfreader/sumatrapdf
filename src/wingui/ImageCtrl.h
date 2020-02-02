/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct ImageCtrl : public WindowBase {
    // we don't own it
    Gdiplus::Bitmap* bmp = nullptr;

    ImageCtrl(HWND parent);
    ~ImageCtrl() override;
    bool Create() override;

    SIZE GetIdealSize() override;
};

ILayout* NewImageLayout(ImageCtrl* b);

bool IsImage(Kind);
bool IsImage(ILayout*);
