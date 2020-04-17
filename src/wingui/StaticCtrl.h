/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct StaticCtrl : WindowBase {
    StaticCtrl(HWND parent);
    ~StaticCtrl() override;
    bool Create() override;

    void HandleWM_COMMAND(WndEvent*);

    SizeI GetIdealSize() override;
};

ILayout* NewStaticLayout(StaticCtrl* w);
ILayout* NewLabelLayout(StaticCtrl* w);
bool IsStatic(Kind);
bool IsStatic(ILayout*);
