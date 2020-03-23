/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct ListBoxModel {
    virtual int ItemsCount();
    virtual Size Draw(bool measure);
};

struct ListBoxCtrl : WindowBase {
    ListBoxModel* model = nullptr;

    ListBoxCtrl(HWND parent);
    ~ListBoxCtrl() override;
    bool Create() override;

    SIZE GetIdealSize() override;

    int GetSelectedItem();
    bool SetSelectedItem(int);
    void SetModel(ListBoxModel*);
};

WindowBaseLayout* NewListBoxLayout(ListBoxCtrl*);

bool IsListBox(Kind);
bool IsListBox(ILayout*);
