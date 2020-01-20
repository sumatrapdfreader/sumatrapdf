/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/DropDownCtrl.h"

// https://docs.microsoft.com/en-us/windows/win32/controls/combo-boxes

Kind kindDropDown = "dropdown";

bool IsDropDown(Kind kind) {
    return kind == kindDropDown;
}

bool IsDropDown(ILayout* l) {
    return IsLayoutOfKind(l, kindDropDown);
}

DropDownCtrl::DropDownCtrl(HWND parent) : WindowBase(parent) {
    dwStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST;
    winClass = WC_COMBOBOX;
    kind = kindDropDown;
}

DropDownCtrl::~DropDownCtrl() {
}

static void setDropDownItems(HWND hwnd, Vec<std::string_view>& items) {
    ComboBox_ResetContent(hwnd);
    for (std::string_view s : items) {
        WCHAR* ws = strconv::Utf8ToWstr(s);
        ComboBox_AddString(hwnd, ws);
        free(ws);
    }
}

bool DropDownCtrl::Create() {
    bool ok = WindowBase::Create();
    setDropDownItems(hwnd, items);
    SetCurrentSelection(0);
    if (ok) {
        SubclassParent();
    }
    return ok;
}

void DropDownCtrl::WndProcParent(WndProcArgs* args) {
    UINT msg = args->msg;
    WPARAM wp = args->wparam;
    if (msg == WM_COMMAND) {
        auto code = HIWORD(wp);
        if (code == CBN_SELCHANGE) {
            if (onDropDownSelectionChanged) {
                DropDownSelectionChangedArgs a;
                CopyWndProcArgs cp(&a, args);
                a.dropDown = this;
                a.idx = GetCurrentSelection();
                std::string_view s;
                if (a.idx >= 0 && a.idx < (int)items.size()) {
                    a.item = items.at(a.idx);
                }
                onDropDownSelectionChanged(&a);
            }
            return;
        }
    }
}

int DropDownCtrl::GetCurrentSelection() {
    int res = (int)ComboBox_GetCurSel(hwnd);
    return res;
}

void DropDownCtrl::SetCurrentSelection(int n) {
    int nItems = (int)items.size();
    if (nItems == 0) {
        return;
    }
    if (n < 0) {
        n = 0;
    }
    if (n >= nItems) {
        n = nItems - 1;
    }
    ComboBox_SetCurSel(hwnd, n);
}

void DropDownCtrl::SetItems(Vec<std::string_view>& newItems) {
    items.Reset();
    for (std::string_view s : newItems) {
        items.Append(s);
    }
    setDropDownItems(hwnd, items);
    SetCurrentSelection(0);
    SetCurrentSelection(0);
}

SIZE DropDownCtrl::GetIdealSize() {
    SizeI s1 = TextSizeInHwnd(hwnd, L"Minimal", hfont);
    for (std::string_view s : items) {
        WCHAR* ws = strconv::Utf8ToWstr(s);
        SizeI s2 = TextSizeInHwnd(hwnd, ws, hfont);
        s1.dx = std::max(s1.dx, s2.dx);
        s1.dy = std::max(s1.dy, s2.dy);
        free(ws);
    }
    // TODO: scale with dpi
    // TODO: not sure if I want scrollbar. Only needed if a lot of items
    int pad = GetSystemMetrics(SM_CXVSCROLL);
    pad += 8;
    return SIZE{s1.dx + pad, s1.dy + 2};
}

ILayout* NewDropDownLayout(DropDownCtrl* b) {
    return new WindowBaseLayout(b, kindDropDown);
}
