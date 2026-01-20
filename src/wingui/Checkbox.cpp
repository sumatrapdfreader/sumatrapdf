/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/BitManip.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"
#include "utils/WinDynCalls.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Theme.h"

#include "utils/Log.h"

//- Checkbox

// https://docs.microsoft.com/en-us/windows/win32/controls/buttons

static Kind kindCheckbox = "checkbox";

static Checkbox::State GetButtonState(HWND hwnd) {
    auto res = Button_GetCheck(hwnd);
    return (Checkbox::State)res;
}

static void SetButtonState(HWND hwnd, Checkbox::State newState) {
    ReportIf(!hwnd);
    Button_SetCheck(hwnd, newState);
}

Checkbox::Checkbox() {
    kind = kindCheckbox;
}

HWND Checkbox::Create(const CreateArgs& args) {
    CreateControlArgs cargs;
    cargs.parent = args.parent;
    cargs.text = args.text;
    cargs.className = WC_BUTTONW;
    cargs.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX;

    Wnd::CreateControl(cargs);
    SetButtonState(hwnd, args.initialState);
    SizeToIdealSize(this);
    return hwnd;
}

bool Checkbox::OnCommand(WPARAM wp, LPARAM) {
    auto code = HIWORD(wp);
    if (code == BN_CLICKED && onStateChanged.IsValid()) {
        onStateChanged.Call();
        return true;
    }
    return false;
}

Size Checkbox::GetIdealSize() {
    return ButtonGetIdealSize(hwnd);
}

void Checkbox::SetState(State newState) {
    ReportIf(!hwnd);
    SetButtonState(hwnd, newState);
}

Checkbox::State Checkbox::GetState() const {
    return GetButtonState(hwnd);
}

void Checkbox::SetIsChecked(bool isChecked) {
    ReportIf(!hwnd);
    Checkbox::State newState = isChecked ? Checkbox::State::Checked : Checkbox::State::Unchecked;
    SetButtonState(hwnd, newState);
}

bool Checkbox::IsChecked() const {
    ReportIf(!hwnd);
    auto state = GetState();
    return state == Checkbox::State::Checked;
}
