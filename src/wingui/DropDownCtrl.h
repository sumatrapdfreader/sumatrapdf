/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct DropDownCtrl;

struct DropDownSelectionChangedEvent : WndEvent {
    DropDownCtrl* dropDown = nullptr;
    int idx = 0;
    std::string_view item;
};

typedef std::function<void(DropDownSelectionChangedEvent*)> DropDownSelectionChangedHandler;

struct DropDownCtrl : WindowBase {
    Vec<std::string_view> items;
    DropDownSelectionChangedHandler onSelectionChanged = nullptr;

    DropDownCtrl(HWND parent);
    ~DropDownCtrl();
    bool Create() override;

    Size GetIdealSize() override;

    int GetCurrentSelection();
    void SetCurrentSelection(int n);
    void SetItems(Vec<std::string_view>& newItems);
    void SetItemsSeqStrings(const char* items);
    void SetCueBanner(std::string_view);
};
