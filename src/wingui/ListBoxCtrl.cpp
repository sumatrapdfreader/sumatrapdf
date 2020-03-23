/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/ListBoxCtrl.h"

Kind kindListBox = "listbox";

bool IsListBox(Kind kind) {
    return kind == kindListBox;
}

bool IsListBox(ILayout* l) {
    return IsLayoutOfKind(l, kindListBox);
}

ListBoxCtrl::ListBoxCtrl(HWND p) : WindowBase(p) {
    kind = kindListBox;
    dwExStyle = WS_EX_TOPMOST;
    // win.WS_BORDER|win.WS_TABSTOP|win.WS_VISIBLE|win.WS_VSCROLL|win.WS_HSCROLL|win.LBS_NOINTEGRALHEIGHT|win.LBS_NOTIFY|style,
    dwStyle = WS_BORDER | WS_TABSTOP | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY;
    winClass = L"LISTBOX";
    // CreateWindow() for tooltip fails if this is not 0
    ctrlID = 0;
}

ListBoxCtrl::~ListBoxCtrl() {
}

bool ListBoxCtrl::Create() {
    bool ok = WindowBase::Create();
    // TODO: update ideal size based on the size of the model
    return ok;
}

SIZE ListBoxCtrl::GetIdealSize() {
    // TODO:
    return SIZE{100, 32};
}

int ListBoxCtrl::GetSelectedItem() {
    LRESULT res = SendMessageW(hwnd, LB_GETCURSEL, 0, 0);
    return (int)res;
}

bool ListBoxCtrl::SetSelectedItem(int n) {
    if (n < 0) {
        return false;
    }
    int nItems = model->ItemsCount();
    if (n >= nItems) {
        return false;
    }
    LRESULT res = SendMessageW(hwnd, LB_SETCURSEL, n, 0);
    if (res == LB_ERR) {
        return false;
    }
    return true;
}

void ListBoxCtrl::SetModel(ListBoxModel* model) {
    this->model = model;
    // TODO: update ideal size based on the size of the model
}

WindowBaseLayout* NewListBoxLayout(ListBoxCtrl* w) {
    return new WindowBaseLayout(w, kindListBox);
}