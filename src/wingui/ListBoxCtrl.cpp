/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/ListBoxCtrl.h"

// https://docs.microsoft.com/en-us/windows/win32/controls/list-boxes

Kind kindListBox = "listbox";

ListBoxCtrl::ListBoxCtrl() {
    kind = kindListBox;
    dwExStyle = 0;
    dwStyle = WS_CHILD | WS_BORDER | WS_TABSTOP | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL;
    dwStyle |= LBS_NOINTEGRALHEIGHT | LBS_NOTIFY;
    winClass = L"LISTBOX";
    ctrlID = 0;
}

ListBoxCtrl::~ListBoxCtrl() {
    delete this->model;
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

bool ListBoxCtrl::Create(HWND parent) {
    idealSize = {DpiScale(parent, 120), DpiScale(parent, 32)};
    bool ok = WindowBase::Create(parent);

    // TODO: update ideal size based on the size of the model?
    if (!ok) {
        return false;
    }
    if (model != nullptr) {
        FillWithItems(this->hwnd, model);
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
    return res != LB_ERR;
}

// for efficiency you can re-use model:
// get the model, change data, call SetModel() again
void ListBoxCtrl::SetModel(ListBoxModel* model) {
    if (this->model && (this->model != model)) {
        delete this->model;
    }
    this->model = model;
    FillWithItems(this->hwnd, model);
    SetCurrentSelection(-1);
    // TODO: update ideal size based on the size of the model
}
