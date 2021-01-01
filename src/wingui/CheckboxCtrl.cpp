/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/CheckboxCtrl.h"

// https://docs.microsoft.com/en-us/windows/win32/controls/buttons

static Kind kindCheckbox = "checkbox";

static CheckState GetButtonCheckState(HWND hwnd) {
    auto res = Button_GetCheck(hwnd);
    return (CheckState)res;
}

static void SetButtonCheckState(HWND hwnd, CheckState newState) {
    CrashIf(!hwnd);
    Button_SetCheck(hwnd, newState);
}

CheckboxCtrl::CheckboxCtrl(HWND parent) : WindowBase(parent) {
    dwStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX;
    winClass = WC_BUTTON;
    kind = kindCheckbox;
}

CheckboxCtrl::~CheckboxCtrl() {
}

static void Handle_WM_COMMAND(void* user, WndEvent* ev) {
    auto w = (CheckboxCtrl*)user;
    uint msg = ev->msg;
    CrashIf(msg != WM_COMMAND);
    WPARAM wp = ev->wp;
    auto code = HIWORD(wp);
    if (code == BN_CLICKED) {
        if (w->onCheckStateChanged) {
            auto state = w->GetCheckState();
            w->onCheckStateChanged(state);
        }
        ev->didHandle = true;
        ev->result = 0;
        return;
    }
}

bool CheckboxCtrl::Create() {
    bool ok = WindowBase::Create();
    if (!ok) {
        return false;
    }
    void* user = this;
    RegisterHandlerForMessage(hwnd, WM_COMMAND, Handle_WM_COMMAND, user);
    return ok;
}

Size CheckboxCtrl::GetIdealSize() {
    return ButtonGetIdealSize(hwnd);
}

void CheckboxCtrl::SetCheckState(CheckState newState) {
    // TODO: maybe remember if called before hwnd is created
    CrashIf(!hwnd);
    SetButtonCheckState(hwnd, newState);
}

CheckState CheckboxCtrl::GetCheckState() const {
    return GetButtonCheckState(hwnd);
}

void CheckboxCtrl::SetIsChecked(bool isChecked) {
    CheckState newState = isChecked ? CheckState::Checked : CheckState::Unchecked;
    // TODO: maybe remember if called before hwnd is created
    CrashIf(!hwnd);
    SetButtonCheckState(hwnd, newState);
}

bool CheckboxCtrl::IsChecked() const {
    // TODO: maybe make work before hwnd is created
    CrashIf(!hwnd);
    auto state = GetCheckState();
    return state == CheckState::Checked;
}
