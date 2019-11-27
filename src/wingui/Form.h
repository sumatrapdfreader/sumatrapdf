/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern Kind windowKind;

struct Form : public Window {
    ~Form() override;
};
