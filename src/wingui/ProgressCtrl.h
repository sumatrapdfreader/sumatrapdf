/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct ProgressCtrl : public WindowBase {
    ProgressCtrl(HWND parent);
    ~ProgressCtrl() override;
    bool Create() override;
    LRESULT WndProcParent(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, bool& didHandle) override;

    SIZE GetIdealSize() override;
};

ILayout* NewStaticLayout(ProgressCtrl* b);

bool IsProgress(Kind);
bool IsProgress(ILayout*);
