/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct StaticCtrl : WindowBase {
    explicit StaticCtrl(HWND parent);
    ~StaticCtrl() override;

    bool Create() override;
    Size GetIdealSize() override;

    HBRUSH tmpBgBrush = nullptr;
};
