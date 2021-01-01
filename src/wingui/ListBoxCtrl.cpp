/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/ListBoxCtrl.h"

// https://docs.microsoft.com/en-us/windows/win32/controls/list-boxes

Kind kindListBox = "listbox";

ListBoxModelStrings::~ListBoxModelStrings() {
}

int ListBoxModelStrings::ItemsCount() {
    return strings.Size();
}

Size ListBoxModelStrings::Draw([[maybe_unused]] bool measure) {
    CrashIf(true);
    return Size{};
}

std::string_view ListBoxModelStrings::Item(int i) {
    return strings.at(i);
}

ListBoxCtrl::ListBoxCtrl(HWND p) : WindowBase(p), idealSize({DpiScale(p, 120), DpiScale(p, 32)}) {
    kind = kindListBox;
    dwExStyle = 0;
    dwStyle = WS_CHILD | WS_BORDER | WS_TABSTOP | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL;
    dwStyle |= LBS_NOINTEGRALHEIGHT | LBS_NOTIFY;
    winClass = L"LISTBOX";
    ctrlID = 0;
}

ListBoxCtrl::~ListBoxCtrl() {
}

static void FillWithItems(ListBoxCtrl* w, ListBoxModel* model) {
    HWND hwnd = w->hwnd;
    ListBox_ResetContent(hwnd);
    for (int i = 0; i < model->ItemsCount(); i++) {
        auto sv = model->Item(i);
        AutoFreeWstr ws = strconv::Utf8ToWstr(sv);
        ListBox_AddString(hwnd, ws.Get());
    }
}

static void DispatchSelectionChanged(ListBoxCtrl* w, WndEvent* ev) {
    ListBoxSelectionChangedEvent a;
    CopyWndEvent cp(&a, ev);
    a.listBox = w;
    a.idx = w->GetCurrentSelection();
    if (a.idx >= 0 && a.idx < w->model->ItemsCount()) {
        a.item = w->model->Item(a.idx);
    }
    w->onSelectionChanged(&a);
}

static void Handle_WM_COMMAND(void* user, WndEvent* ev) {
    auto w = (ListBoxCtrl*)user;
    uint msg = ev->msg;
    CrashIf(msg != WM_COMMAND);
    WPARAM wp = ev->wp;
    auto code = HIWORD(wp);
    if (code == LBN_SELCHANGE && w->onSelectionChanged) {
        DispatchSelectionChanged(w, ev);
        return;
    }
}

bool ListBoxCtrl::Create() {
    bool ok = WindowBase::Create();
    // TODO: update ideal size based on the size of the model?
    if (!ok) {
        return false;
    }
    if (model != nullptr) {
        FillWithItems(this, model);
    }
    void* user = this;
    RegisterHandlerForMessage(hwnd, WM_COMMAND, Handle_WM_COMMAND, user);
    return ok;
}

// https://docs.microsoft.com/en-us/windows/win32/controls/lb-getitemheight
int ListBoxCtrl::GetItemHeight(int idx) {
    // idx only valid for LBS_OWNERDRAWVARIABLE, otherwise should be 0
    int res = (int)SendMessageW(hwnd, LB_GETITEMHEIGHT, idx, 0);
    if (res == LB_ERR) {
        // if failed for some reason, fallback to measuring text in default font
        HFONT f = GetFont();
        Size sz = HwndMeasureText(hwnd, L"A", f);
        res = sz.dy;
    }
    return res;
}

Size ListBoxCtrl::GetIdealSize() {
    Size res = idealSize;
    if (idealSizeLines > 0) {
        int dy = GetItemHeight(0) * idealSizeLines + DpiScale(hwnd, 2 + 2); // padding of 2 at top and bottom
        res.dy = dy;
    }
    return res;
}

int ListBoxCtrl::GetCurrentSelection() {
    LRESULT res = ListBox_GetCurSel(hwnd);
    return (int)res;
}

// -1 to clear selection
// returns false on error
bool ListBoxCtrl::SetCurrentSelection(int n) {
    if (n < 0) {
        ListBox_SetCurSel(hwnd, -1);
        return true;
    }
    int nItems = model->ItemsCount();
    if (n >= nItems) {
        return false;
    }
    LRESULT res = ListBox_SetCurSel(hwnd, n);
    if (res == LB_ERR) {
        return false;
    }
    return true;
}

void ListBoxCtrl::SetModel(ListBoxModel* model) {
    this->model = model;
    if (model != nullptr) {
        FillWithItems(this, model);
    }
    SetCurrentSelection(-1);
    // TODO: update ideal size based on the size of the model
}
