/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

typedef std::function<void()> OnClicked;
typedef std::function<void(bool)> CheckboxChangeCb;

struct ButtonCtrl : public WindowBase {
    // creation parameters. must be set before Create() call

    OnClicked OnClicked = nullptr;

    ButtonCtrl(HWND parent);
    ~ButtonCtrl() override;
    bool Create() override;
    LRESULT WndProcParent(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) override;

    SIZE GetIdealSize() override;
};

struct CheckboxCtrl : public WindowBase {
    // called when it's checked / unchecked
    // TODO: implement me

    CheckboxChangeCb OnChange = nullptr;

    CheckboxCtrl(HWND parent);
    ~CheckboxCtrl();

    SIZE GetIdealSize() override;

    void SetIsChecked(bool isChecked);
    bool IsChecked() const;
};

ILayout* NewButtonLayout(ButtonCtrl* b);
ILayout* NewCheckboxLayout(CheckboxCtrl* b);

bool IsButton(Kind);
bool IsButton(ILayout*);

bool IsCheckbox(Kind);
bool IsCheckbox(ILayout*);
