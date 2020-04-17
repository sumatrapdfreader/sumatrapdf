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

ListBoxModelStrings::~ListBoxModelStrings() {
}

int ListBoxModelStrings::ItemsCount() {
    return strings.size();
}

SizeI ListBoxModelStrings::Draw(bool measure) {
    UNUSED(measure);
    CrashIf(true);
    return SizeI{};
}

std::string_view ListBoxModelStrings::Item(int i) {
    return strings.at(i);
}

ListBoxCtrl::ListBoxCtrl(HWND p) : WindowBase(p) {
    kind = kindListBox;
    dwExStyle = 0;
    // win.WS_BORDER|win.WS_TABSTOP|win.WS_VISIBLE|win.WS_VSCROLL|win.WS_HSCROLL|win.LBS_NOINTEGRALHEIGHT|win.LBS_NOTIFY|style,
    dwStyle =
        WS_CHILD | WS_BORDER | WS_TABSTOP | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY;
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

bool ListBoxCtrl::Create() {
    bool ok = WindowBase::Create();
    // TODO: update ideal size based on the size of the model?
    if (!ok) {
        return false;
    }
    if (model != nullptr) {
        FillWithItems(this, model);
    }
    return ok;
}

SizeI ListBoxCtrl::GetIdealSize() {
    return minSize;
}

int ListBoxCtrl::GetSelectedItem() {
    LRESULT res = ListBox_GetCurSel(hwnd);
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
    // TODO: update ideal size based on the size of the model
}

WindowBaseLayout* NewListBoxLayout(ListBoxCtrl* w) {
    return new WindowBaseLayout(w, kindListBox);
}
