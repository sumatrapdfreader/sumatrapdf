/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct ProgressCtrl : WindowBase {
    explicit ProgressCtrl(int initialMax = 0);

    // those might be outdated if user manipulates hwnd directly
    int max = 0;
    int current = 0;

    int idealDx = 0;
    int idealDy = 0;

    ~ProgressCtrl() override;
    bool Create(HWND parent) override;

    void SetMax(int);
    void SetCurrent(int);
    int GetMax();
    int GetCurrent();

    Size GetIdealSize() override;
};
