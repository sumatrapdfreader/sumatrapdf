/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Win.h"

#include "gui/UIModels.h"

#include "gui/Layout.h"
#include "gui/win/WinGui.h"

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
    onCommand = MkMethod1<Checkbox, ControlBase::CommandEvent*, &Checkbox::OnCommand>(this);
    onMessageReflect = MkMethod1<Checkbox, ControlBase::MessageReflectEvent*, &Checkbox::OnMessageReflect>(this);
    CreateControlArgs cargs;
    cargs.parent = args.parent;
    cargs.isRtl = args.isRtl;
    cargs.text = args.text;
    cargs.className = WC_BUTTONW;
    cargs.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
    cargs.style |= args.isRadio ? BS_AUTORADIOBUTTON : BS_AUTOCHECKBOX;
    if (args.isGroupStart) {
        cargs.style |= WS_GROUP;
    }

    ControlBase::CreateControl(cargs);
    SetButtonState(hwnd, args.initialState);
    SizeToIdealSize(this);
    return hwnd;
}

// A checkbox draws its label on the parent's background, which by default is
// the gray button face. Answer the reflected WM_CTLCOLORSTATIC with our own
// colors so the control blends into a themed (or plain white) parent.
void Checkbox::OnMessageReflect(ControlBase::MessageReflectEvent* ev) {
    if (ev->msg == WM_CTLCOLORSTATIC || ev->msg == WM_CTLCOLORBTN) {
        HDC hdc = (HDC)ev->wparam;
        if (!IsSpecialColor(textColor)) {
            SetTextColor(hdc, textColor);
        }
        if (!IsSpecialColor(bgColor)) {
            SetBkColor(hdc, bgColor);
        }
        auto* br = BackgroundBrush();
        ev->result = (LRESULT)br;
    }
}

void Checkbox::OnCommand(ControlBase::CommandEvent* ev) {
    auto code = HIWORD(ev->wparam);
    if (code == BN_CLICKED && onStateChanged.IsValid()) {
        onStateChanged.Call();
        ev->didHandle = true;
    }
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
