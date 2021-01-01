/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"
#include "utils/LogDbg.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/DropDownCtrl.h"

// https://docs.microsoft.com/en-us/windows/win32/controls/combo-boxes

Kind kindDropDown = "dropdown";

DropDownCtrl::DropDownCtrl(HWND parent) : WindowBase(parent) {
    dwStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST;
    winClass = WC_COMBOBOX;
    kind = kindDropDown;
}

DropDownCtrl::~DropDownCtrl() {
}

static void SetDropDownItems(HWND hwnd, Vec<std::string_view>& items) {
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
    if (a.idx >= 0 && a.idx < (int)w->items.size()) {
        a.item = w->items.at(a.idx);
    }
    w->onSelectionChanged(&a);
}

static void Handle_WM_COMMAND(void* user, WndEvent* ev) {
    auto w = (DropDownCtrl*)user;
    uint msg = ev->msg;
    CrashIf(msg != WM_COMMAND);
    WPARAM wp = ev->wp;
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
    SetDropDownItems(hwnd, items);
    SetCurrentSelection(-1);

    void* user = this;
    RegisterHandlerForMessage(hwnd, WM_COMMAND, Handle_WM_COMMAND, user);
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
    SetDropDownItems(hwnd, items);
    SetCurrentSelection(-1);
}

static void DropDownItemsFromStringArray(Vec<std::string_view>& items, const char* strings) {
    while (*strings) {
        items.Append(strings);
        strings = seqstrings::SkipStr(strings);
    }
}

void DropDownCtrl::SetItemsSeqStrings(const char* items) {
    Vec<std::string_view> strings;
    DropDownItemsFromStringArray(strings, items);
    SetItems(strings);
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
    // TODO: not sure if I want scrollbar. Only needed if a lot of items
    int dxPad = GetSystemMetrics(SM_CXVSCROLL);
    int dx = s1.dx + dxPad + DpiScale(hwnd, 8);
    // TODO: 5 is a guessed number.
    int dyPad = DpiScale(hwnd, 4);
    int dy = s1.dy + dyPad;
    Rect rc = WindowRect(hwnd);
    if (rc.dy > dy) {
        dy = rc.dy;
    }
    return {dx, dy};
}
