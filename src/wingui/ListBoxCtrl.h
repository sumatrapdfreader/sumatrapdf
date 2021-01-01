/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct ListBoxModel {
    virtual ~ListBoxModel(){};
    virtual int ItemsCount() = 0;
    virtual Size Draw(bool measure) = 0;
    virtual std::string_view Item(int) = 0;
};

struct ListBoxModelStrings : ListBoxModel {
    VecStr strings;

    ~ListBoxModelStrings() override;
    int ItemsCount() override;
    Size Draw(bool measure) override;
    std::string_view Item(int) override;
};

struct ListBoxCtrl;

struct ListBoxSelectionChangedEvent : WndEvent {
    ListBoxCtrl* listBox = nullptr;
    int idx = 0;
    std::string_view item;
};

typedef std::function<void(ListBoxSelectionChangedEvent*)> ListBoxSelectionChangedHandler;

struct ListBoxCtrl : WindowBase {
    ListBoxModel* model = nullptr;
    ListBoxSelectionChangedHandler onSelectionChanged = nullptr;

    Size idealSize{};
    int idealSizeLines = 0;

    ListBoxCtrl(HWND parent);
    ~ListBoxCtrl() override;
    bool Create() override;

    int GetItemHeight(int);

    Size GetIdealSize() override;

    int GetCurrentSelection();
    bool SetCurrentSelection(int);
    void SetModel(ListBoxModel*);
};
