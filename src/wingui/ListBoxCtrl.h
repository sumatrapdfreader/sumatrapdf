/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct ListBoxModel {
    virtual ~ListBoxModel(){};
    virtual int ItemsCount() = 0;
    virtual SizeI Draw(bool measure) = 0;
    virtual std::string_view Item(int) = 0;
};

struct ListBoxModelStrings : ListBoxModel {
    VecStr strings;

    ~ListBoxModelStrings() override;
    int ItemsCount() override;
    SizeI Draw(bool measure) override;
    std::string_view Item(int) override;
};

struct ListBoxCtrl : WindowBase {
    ListBoxModel* model = nullptr;
    SizeI minSize{120, 32};

    ListBoxCtrl(HWND parent);
    ~ListBoxCtrl() override;
    bool Create() override;

    SizeI GetIdealSize() override;

    int GetSelectedItem();
    bool SetSelectedItem(int);
    void SetModel(ListBoxModel*);
};

WindowBaseLayout* NewListBoxLayout(ListBoxCtrl*);

bool IsListBox(Kind);
bool IsListBox(ILayout*);
