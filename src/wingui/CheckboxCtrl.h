/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

enum class CheckState {
    Unchecked = BST_UNCHECKED,
    Checked = BST_CHECKED,
    Indeterminate = BST_INDETERMINATE,
};

typedef std::function<void(CheckState)> OnCheckStateChanged;

struct CheckboxCtrl : public WindowBase {
    OnCheckStateChanged OnCheckStateChanged = nullptr;

    CheckboxCtrl(HWND parent);
    ~CheckboxCtrl();
    bool Create() override;

    void WndProcParent(WndProcArgs*) override;

    SIZE GetIdealSize() override;

    void SetCheckState(CheckState);
    CheckState GetCheckState() const;

    void SetIsChecked(bool isChecked);
    bool IsChecked() const;
};

ILayout* NewCheckboxLayout(CheckboxCtrl* b);

bool IsCheckbox(Kind);
bool IsCheckbox(ILayout*);
