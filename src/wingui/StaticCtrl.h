/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct StaticCtrl : WindowBase {
    StaticCtrl();
    ~StaticCtrl() override;

    bool Create(HWND parent) override;
    Size GetIdealSize() override;

    HBRUSH tmpBgBrush = nullptr;
};
