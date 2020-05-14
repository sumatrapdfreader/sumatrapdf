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

Size ListBoxModelStrings::Draw(bool measure) {
    UNUSED(measure);
    CrashIf(true);
    return Size{};
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

static void DispatchSelectionChanged(ListBoxCtrl* w, WndEvent* ev) {
    ListBoxSelectionChangedEvent a;
    CopyWndEvent cp(&a, ev);
    a.listBox = w;
    a.idx = w->GetCurrentSelection();
    std::string_view s;
    if (a.idx >= 0 && a.idx < w->model->ItemsCount()) {
        a.item = w->model->Item(a.idx);
    }
    w->onSelectionChanged(&a);
}

static void DispatchWM_COMMAND(void* user, WndEvent* ev) {
    auto w = (ListBoxCtrl*)user;
    UINT msg = ev->msg;
    CrashIf(msg != WM_COMMAND);
    WPARAM wp = ev->wparam;
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
    RegisterHandlerForMessage(hwnd, WM_COMMAND, DispatchWM_COMMAND, user);
    return ok;
}

Size ListBoxCtrl::GetIdealSize() {
    return minSize;
}

int ListBoxCtrl::GetCurrentSelection() {
    LRESULT res = ListBox_GetCurSel(hwnd);
    return (int)res;
}

bool ListBoxCtrl::SetCurrentSelection(int n) {
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
