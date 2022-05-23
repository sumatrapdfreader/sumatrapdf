/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */


struct TabsCtrl2 : WindowBase {
    str::Str lastTabText;
    bool createToolTipsHwnd = false;
    str::Str currTooltipText;

    StrVec tooltips;

    // for all WM_NOTIFY messages
    WmNotifyHandler onNotify = nullptr;

    TabsCtrl2();
    ~TabsCtrl2() override;
    bool Create(HWND parent) override;

    void WndProc(WndEvent*) override;

    Size GetIdealSize() override;

    int InsertTab(int idx, const char* sv);

    void RemoveTab(int idx);
    void RemoveAllTabs();

    void SetTabText(int idx, const char* sv);

    void SetTooltip(int idx, const char*);
    const char* GetTooltip(int idx);

    char* GetTabText(int idx);

    int GetSelectedTabIndex();
    int SetSelectedTabByIndex(int idx);

    void SetItemSize(Size sz);
    int GetTabCount();

    void SetToolTipsHwnd(HWND);
    HWND GetToolTipsHwnd();

    void MaybeUpdateTooltip();
    void MaybeUpdateTooltipText(int idx);
};
