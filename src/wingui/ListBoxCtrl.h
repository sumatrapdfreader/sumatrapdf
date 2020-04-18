/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
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

struct ListBoxCtrl : WindowBase {
    ListBoxModel* model = nullptr;
    Size minSize{120, 32};

    ListBoxCtrl(HWND parent);
    ~ListBoxCtrl() override;
    bool Create() override;

    Size GetIdealSize() override;

    int GetSelectedItem();
    bool SetSelectedItem(int);
    void SetModel(ListBoxModel*);
};

WindowBaseLayout* NewListBoxLayout(ListBoxCtrl*);

bool IsListBox(Kind);
bool IsListBox(ILayout*);
