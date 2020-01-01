/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct StaticCtrl : public WindowBase {
    StaticCtrl(HWND parent);
    ~StaticCtrl() override;
    bool Create() override;

    void WndProcParent(WndProcArgs*) override;

    SIZE GetIdealSize() override;
};

ILayout* NewStaticLayout(StaticCtrl* b);

bool IsStatic(Kind);
bool IsStatic(ILayout*);
