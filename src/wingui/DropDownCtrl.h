/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

typedef std::function<void(int, std::string_view)> OnDropDownSelectionChanged;

struct DropDownCtrl : public WindowBase {
    Vec<std::string_view> items;
    OnDropDownSelectionChanged OnDropDownSelectionChanged;

    DropDownCtrl(HWND parent);
    ~DropDownCtrl();
    bool Create() override;
    LRESULT WndProcParent(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, bool& didHandle) override;

    SIZE GetIdealSize() override;

    void SetCurrentSelection(int n);
    int GetCurrentSelection();
    void SetItems(Vec<std::string_view>& newItems);
};

ILayout* NewDropDownLayout(DropDownCtrl* b);

bool IsDropDown(Kind);
bool IsDropDown(ILayout*);
