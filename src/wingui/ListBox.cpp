/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/BitManip.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"
#include "utils/WinDynCalls.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Theme.h"

#include "utils/Log.h"

Kind kindListBox = "listbox";

ListBox::ListBox() {
    kind = kindListBox;
#if 0
    ctrlID = 0;
#endif
}

ListBox::~ListBox() {
    delete this->model;
}

HWND ListBox::Create(const CreateArgs& args) {
    idealSizeLines = args.idealSizeLines;
    if (idealSizeLines < 0) {
        idealSizeLines = 0;
    }
    idealSize = {DpiScale(args.parent, 120), DpiScale(args.parent, 32)};

    CreateControlArgs cargs;
    cargs.className = L"LISTBOX";
    cargs.parent = args.parent;
    cargs.font = args.font;

    // https://docs.microsoft.com/en-us/windows/win32/controls/list-box-styles
    cargs.style = WS_CHILD | WS_TABSTOP | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL;
    cargs.style |= LBS_NOINTEGRALHEIGHT | LBS_NOTIFY;
    if (onDrawItem.IsValid()) {
        cargs.style |= LBS_OWNERDRAWFIXED;
    }
    // args.style |= WS_BORDER;
    Wnd::CreateControl(cargs);
    SizeToIdealSize(this);

    if (hwnd) {
        // For owner-draw, set item height manually since WM_MEASUREITEM
        // is sent during CreateWindowEx before we're registered in WndList
        if (onDrawItem.IsValid()) {
            int itemHeight = GetItemHeight(0) + DpiScale(hwnd, 4);
            SendMessageW(hwnd, LB_SETITEMHEIGHT, 0, itemHeight);
        }
        if (model != nullptr) {
            FillWithItems(this->hwnd, model);
        }
    }

    return hwnd;
}

// https://docs.microsoft.com/en-us/windows/win32/controls/lb-getitemheight
int ListBox::GetItemHeight(int idx) {
    // idx only valid for LBS_OWNERDRAWVARIABLE, otherwise should be 0
    int res = (int)SendMessageW(hwnd, LB_GETITEMHEIGHT, idx, 0);
    if (res == LB_ERR) {
        // if failed for some reason, fallback to measuring text in default font
        // HFONT f = GetFont();
        HFONT f = GetDefaultGuiFont();
        Size sz = HwndMeasureText(hwnd, "A", f);
        res = sz.dy;
    }
    return res;
}

Size ListBox::GetIdealSize() {
    Size res = idealSize;
    if (idealSizeLines > 0) {
        int dy = GetItemHeight(0) * idealSizeLines + DpiScale(hwnd, 2 + 2); // padding of 2 at top and bottom
        res.dy = dy;
    }
    return res;
}

int ListBox::GetCount() {
    LRESULT res = ListBox_GetCount(hwnd);
    return (int)res;
}

int ListBox::GetCurrentSelection() {
    LRESULT res = ListBox_GetCurSel(hwnd);
    return (int)res;
}

// -1 to clear selection
// returns false on error
bool ListBox::SetCurrentSelection(int n) {
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
void ListBox::SetModel(ListBoxModel* model) {
    if (this->model && (this->model != model)) {
        delete this->model;
    }
    this->model = model;
    if (model != nullptr) {
        FillWithItems(this->hwnd, model);
    }
    SetCurrentSelection(-1);
    // TODO: update ideal size based on the size of the model
}

bool ListBox::OnCommand(WPARAM wparam, LPARAM lparam) {
    auto code = HIWORD(wparam);
    // https://docs.microsoft.com/en-us/windows/win32/controls/lbn-selchange
    if (code == LBN_SELCHANGE && onSelectionChanged.IsValid()) {
        onSelectionChanged.Call();
        return true;
    }
    // https://docs.microsoft.com/en-us/windows/win32/controls/lbn-dblclk
    if (code == LBN_DBLCLK && onDoubleClick.IsValid()) {
        onDoubleClick.Call();
        return true;
    }
    return false;
}

LRESULT ListBox::OnMessageReflect(UINT msg, WPARAM wp, LPARAM lparam) {
    // https://docs.microsoft.com/en-us/windows/win32/controls/wm-ctlcolorlistbox
    if (msg == WM_CTLCOLORLISTBOX) {
        HDC hdc = (HDC)wp;
        if (!IsSpecialColor(textColor)) {
            SetTextColor(hdc, textColor);
        }
        if (!IsSpecialColor(bgColor)) {
            SetBkColor(hdc, bgColor);
        }
        auto br = BackgroundBrush();
        return (LRESULT)br;
    }

    // https://docs.microsoft.com/en-us/windows/win32/controls/wm-measureitem
    if (msg == WM_MEASUREITEM) {
        if (!onDrawItem.IsValid()) {
            return 0;
        }
        MEASUREITEMSTRUCT* mis = (MEASUREITEMSTRUCT*)lparam;
        // add vertical padding for spacing between items
        mis->itemHeight = GetItemHeight(0) + DpiScale(hwnd, 4);
        return TRUE;
    }

    // https://docs.microsoft.com/en-us/windows/win32/controls/wm-drawitem
    if (msg == WM_DRAWITEM) {
        if (!onDrawItem.IsValid()) {
            return 0;
        }
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lparam;
        DrawItemEvent ev;
        ev.listBox = this;
        ev.hdc = dis->hDC;
        ev.itemRect = dis->rcItem;
        ev.itemIndex = (int)dis->itemID;
        ev.selected = (dis->itemState & ODS_SELECTED) != 0;
        onDrawItem.Call(&ev);
        return TRUE;
    }

    return 0;
}
