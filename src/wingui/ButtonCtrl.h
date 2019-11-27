/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

typedef std::function<void(bool)> CheckboxChangeCb;

struct ButtonCtrl : public WindowBase {
    // creation parameters. must be set before Create() call

    ButtonCtrl(HWND parent);
    ~ButtonCtrl() override;

    bool Create() override;

    SIZE SetTextAndResize(const WCHAR*);
    SIZE GetIdealSize();
    void SetPos(RECT*);
};

struct CheckboxCtrl : public ButtonCtrl {
    // called when it's checked / unchecked
    // TODO: implement me

    CheckboxChangeCb OnChange = nullptr;

    CheckboxCtrl(HWND parent);
    ~CheckboxCtrl();

    void SetIsChecked(bool isChecked);
    bool IsChecked() const;
};

struct ButtonLayout : public ILayout {
    ButtonCtrl* button = nullptr;

    ButtonLayout(ButtonCtrl*);
    virtual ~ButtonLayout();

    Size Layout(const Constraints bc) override;
    Length MinIntrinsicHeight(Length) override;
    Length MinIntrinsicWidth(Length) override;
    void SetBounds(const Rect bounds) override;
};

bool IsButton(Kind);
bool IsButton(ILayout*);

bool IsCheckbox(Kind);
bool IsCheckbox(ILayout*);

struct CheckboxLayout : public ButtonLayout {
  explicit CheckboxLayout(ButtonCtrl*);
};
