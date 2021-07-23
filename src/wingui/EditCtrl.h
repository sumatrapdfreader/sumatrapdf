
/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct EditTextChangedEvent : WndEvent {
    std::string_view text{};
};

using OnTextChanged = std::function<void(EditTextChangedEvent*)>;

// pass to SetColor() function to indicate this color should not change
#define NO_CHANGE (COLORREF)(-2) // -1 is taken by NO_COLOR in windows headers

/*
Creation sequence:
- auto ctrl = new EditCtrl()
- set creation parameters
- ctrl.Create()
*/
struct EditCtrl : WindowBase {
    // data that can be set directly

    str::Str cueText;
    OnTextChanged onTextChanged;

    // set before Create()
    bool isMultiLine = false;
    int idealSizeLines = 1;
    int maxDx = 0;

    // set those via SetColors() to keep bgBrush in sync with bgCol
    HBRUSH bgBrush = nullptr;

    bool hasBorder = false;

    explicit EditCtrl(HWND parent);
    ~EditCtrl() override;
    bool Create() override;
    Size GetIdealSize() override;

    void SetSelection(int start, int end);
    bool SetCueText(std::string_view);
};
