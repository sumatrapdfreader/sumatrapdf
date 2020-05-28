/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct StaticCtrl : WindowBase {
    StaticCtrl(HWND parent);
    ~StaticCtrl() override;

    bool Create() override;
    Size GetIdealSize() override;

    HBRUSH tmpBgBrush = nullptr;
};
