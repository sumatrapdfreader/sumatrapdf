/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct ProgressCtrl : public WindowBase {
    explicit ProgressCtrl(HWND parent, int max = 0);

    // those might be outdated if user manipulates hwnd directly
    int max = 0;
    int current = 0;

    int idealDx = 0;
    int idealDy = 0;

    ~ProgressCtrl() override;
    bool Create() override;

    void SetMax(int);
    void SetCurrent(int);
    int GetMax();
    int GetCurrent();

    SIZE GetIdealSize() override;
};

ILayout* NewProgressLayout(ProgressCtrl* b);

bool IsProgress(Kind);
bool IsProgress(ILayout*);
