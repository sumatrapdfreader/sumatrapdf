/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

using namespace wg;

struct TabsCreateArgs {
    HWND parent = nullptr;
    HFONT font = nullptr;
    bool createToolTipsHwnd = false;
    int ctrlID = 0;
};

struct TabsCtrl : Wnd {
    str::Str lastTabText;
    bool createToolTipsHwnd = false;
    str::Str currTooltipText;

    StrVec tooltips;

    TabsCtrl();
    ~TabsCtrl() override;

    HWND Create(TabsCreateArgs&);
    LRESULT OnNotifyReflect(WPARAM, LPARAM) override;

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
