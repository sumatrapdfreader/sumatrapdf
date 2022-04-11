/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct ListBoxModel {
    virtual ~ListBoxModel() = default;
    ;
    virtual int ItemsCount() = 0;
    virtual std::string_view Item(int) = 0;
};

struct ListBoxModelStrings : ListBoxModel {
    StrVec strings;

    ~ListBoxModelStrings() override;
    int ItemsCount() override;
    std::string_view Item(int) override;
};

struct ListBoxCtrl;

struct ListBoxSelectionChangedEvent : WndEvent {
    ListBoxCtrl* listBox = nullptr;
    int idx = 0;
    std::string_view item;
};

using ListBoxSelectionChangedHandler = std::function<void(ListBoxSelectionChangedEvent*)>;

struct ListBoxCtrl : WindowBase {
    ListBoxModel* model = nullptr;
    ListBoxSelectionChangedHandler onSelectionChanged = nullptr;

    Size idealSize = {};
    int idealSizeLines = 0;

    ListBoxCtrl();
    ~ListBoxCtrl() override;
    bool Create(HWND parent) override;

    int GetItemHeight(int);

    Size GetIdealSize() override;

    int GetCurrentSelection();
    bool SetCurrentSelection(int);
    void SetModel(ListBoxModel*);
};

void FillWithItems(HWND hwnd, ListBoxModel* model);
