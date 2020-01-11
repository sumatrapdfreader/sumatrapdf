/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

typedef std::function<void(int, std::string_view)> DropDownSelectionChangedHandler;

struct DropDownCtrl : public WindowBase {
    Vec<std::string_view> items;
    DropDownSelectionChangedHandler onDropDownSelectionChanged = nullptr;

    DropDownCtrl(HWND parent);
    ~DropDownCtrl();
    bool Create() override;

    void WndProcParent(WndProcArgs*) override;

    SIZE GetIdealSize() override;

    void SetCurrentSelection(int n);
    int GetCurrentSelection();
    void SetItems(Vec<std::string_view>& newItems);
};

ILayout* NewDropDownLayout(DropDownCtrl* b);

bool IsDropDown(Kind);
bool IsDropDown(ILayout*);
