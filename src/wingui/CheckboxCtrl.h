/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

enum class CheckState {
    Unchecked = BST_UNCHECKED,
    Checked = BST_CHECKED,
    Indeterminate = BST_INDETERMINATE,
};

typedef std::function<void(CheckState)> OnCheckStateChanged;

struct CheckboxCtrl : WindowBase {
    OnCheckStateChanged onCheckStateChanged = nullptr;

    CheckboxCtrl(HWND parent);
    ~CheckboxCtrl();
    bool Create() override;

    Size GetIdealSize() override;

    void SetCheckState(CheckState);
    CheckState GetCheckState() const;

    void SetIsChecked(bool isChecked);
    bool IsChecked() const;
};
