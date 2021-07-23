/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

enum class CheckState {
    Unchecked = BST_UNCHECKED,
    Checked = BST_CHECKED,
    Indeterminate = BST_INDETERMINATE,
};

using OnCheckStateChanged = std::function<void(CheckState)>;

struct CheckboxCtrl : WindowBase {
    OnCheckStateChanged onCheckStateChanged = nullptr;

    explicit CheckboxCtrl(HWND parent);
    ~CheckboxCtrl() override;
    bool Create() override;

    Size GetIdealSize() override;

    void SetCheckState(CheckState);
    CheckState GetCheckState() const;

    void SetIsChecked(bool isChecked);
    bool IsChecked() const;
};
