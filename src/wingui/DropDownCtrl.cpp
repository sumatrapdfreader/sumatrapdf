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

static void DispatchSelectionChanged(DropDownCtrl* w, WndEvent* ev) {
    DropDownSelectionChangedEvent a;
    CopyWndEvent cp(&a, ev);
    a.dropDown = w;
    a.idx = w->GetCurrentSelection();
    std::string_view s;
    if (a.idx >= 0 && a.idx < (int)w->items.size()) {
        a.item = w->items.at(a.idx);
    }
    w->onSelectionChanged(&a);
}

static void DispatchWM_COMMAND(void* user, WndEvent* ev) {
    auto w = (DropDownCtrl*)user;
    UINT msg = ev->msg;
    CrashIf(msg != WM_COMMAND);
    WPARAM wp = ev->wparam;
    auto code = HIWORD(wp);
    if (code == CBN_SELCHANGE && w->onSelectionChanged) {
        DispatchSelectionChanged(w, ev);
        return;
    }
}

bool DropDownCtrl::Create() {
    bool ok = WindowBase::Create();
    if (!ok) {
        return false;
    }
    setDropDownItems(hwnd, items);
    SetCurrentSelection(-1);

    void* user = this;
    RegisterHandlerForMessage(hwnd, WM_COMMAND, DispatchWM_COMMAND, user);
    return true;
}

// -1 means no selection
int DropDownCtrl::GetCurrentSelection() {
    int res = (int)ComboBox_GetCurSel(hwnd);
    return res;
}

// -1 : no selection
void DropDownCtrl::SetCurrentSelection(int n) {
    if (n < 0) {
        ComboBox_SetCurSel(hwnd, -1);
        return;
    }
    int nItems = (int)items.size();
    CrashIf(n >= nItems);
    ComboBox_SetCurSel(hwnd, n);
}

void DropDownCtrl::SetCueBanner(std::string_view sv) {
    AutoFreeWstr ws = strconv::Utf8ToWstr(sv);
    ComboBox_SetCueBannerText(hwnd, ws.Get());
}

void DropDownCtrl::SetItems(Vec<std::string_view>& newItems) {
    items.Reset();
    for (std::string_view s : newItems) {
        items.Append(s);
    }
    setDropDownItems(hwnd, items);
    SetCurrentSelection(-1);
}

Size DropDownCtrl::GetIdealSize() {
    Size s1 = TextSizeInHwnd(hwnd, L"Minimal", hfont);
    for (std::string_view s : items) {
        WCHAR* ws = strconv::Utf8ToWstr(s);
        Size s2 = TextSizeInHwnd(hwnd, ws, hfont);
        s1.dx = std::max(s1.dx, s2.dx);
        s1.dy = std::max(s1.dy, s2.dy);
        free(ws);
    }
    // TODO: scale with dpi
    // TODO: not sure if I want scrollbar. Only needed if a lot of items
    int pad = GetSystemMetrics(SM_CXVSCROLL);
    pad += 8;
    return {s1.dx + pad, s1.dy + 2};
}
