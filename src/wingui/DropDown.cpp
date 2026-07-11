/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Win.h"
#include "base/Dpi.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
#include "wingui/WinGui.h"

//- DropDown

// https://docs.microsoft.com/en-us/windows/win32/controls/combo-boxes

Kind kindDropDown = "dropdown";

DropDown::DropDown() {
    kind = kindDropDown;
}

static void SetDropDownItems(HWND hwnd, StrVec& items) {
    ComboBox_ResetContent(hwnd);
    int n = len(items);
    for (int i = 0; i < n; i++) {
        Str s = items[i];
        WCHAR* ws = CWStrTemp(s);
        ComboBox_AddString(hwnd, ws);
    }
}

bool DropDown::OnCommand(WPARAM wp, LPARAM) {
    auto code = HIWORD(wp);
    if (code == CBN_EDITCHANGE && onTextChanged.IsValid()) {
        onTextChanged.Call();
        return true;
    }
    if ((code == CBN_SELCHANGE) && onSelectionChanged.IsValid()) {
        onSelectionChanged.Call();
        // must return false or else the drop-down list will not close
        return false;
    }
    return false;
}

HWND DropDown::Create(const CreateArgs& args) {
    CreateControlArgs cargs;
    cargs.parent = args.parent;
    cargs.isRtl = args.isRtl;
    cargs.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
    if (args.isEditable) {
        cargs.style |= CBS_DROPDOWN | WS_VSCROLL;
    } else {
        cargs.style |= CBS_DROPDOWNLIST;
    }
    cargs.className = WC_COMBOBOX;
    cargs.font = args.font;

    Wnd::CreateControl(cargs);
    if (!hwnd) {
        return nullptr;
    }

    // SetDropDownItems(hwnd, items);
    SetCurrentSelection(-1);

    SizeToIdealSize(this);
    return hwnd;
}

// -1 means no selection
int DropDown::GetCurrentSelection() {
    int res = (int)ComboBox_GetCurSel(hwnd);
    return res;
}

// -1 : no selection
void DropDown::SetCurrentSelection(int n) {
    if (n < 0) {
        ComboBox_SetCurSel(hwnd, -1);
        return;
    }
    int nItems = len(items);
    ReportIf(n >= nItems);
    ComboBox_SetCurSel(hwnd, n);
}

void DropDown::SetCueBanner(Str sv) {
    WCHAR* ws = CWStrTemp(sv);
    ComboBox_SetCueBannerText(hwnd, ws);
}

void DropDown::SetItems(StrVec& newItems) {
    items.Reset();
    int n = len(newItems);
    for (int i = 0; i < n; i++) {
        Str s = newItems[i];
        items.Append(s);
    }
    SetDropDownItems(hwnd, items);
    SetCurrentSelection(-1);
}

static void DropDownItemsFromStringArray(StrVec& items, SeqStrings strings) {
    for (int off = 0; SeqStrAt(strings, off);) {
        items.Append(SeqStrAt(strings, off));
        if (!SeqStrAdvance(strings, off)) {
            break;
        }
    }
}

void DropDown::SetItemsSeqStrings(SeqStrings items) {
    StrVec strings;
    DropDownItemsFromStringArray(strings, items);
    SetItems(strings);
}

Size DropDown::GetIdealSize() {
    HFONT hfont = GetWindowFont(hwnd);
    Size s1 = HwndMeasureText(hwnd, "Minimal", hfont);

    int n = len(items);
    for (int i = 0; i < n; i++) {
        Str s = items[i];
        Size s2 = HwndMeasureText(hwnd, s, hfont);
        s1.dx = std::max(s1.dx, s2.dx);
        s1.dy = std::max(s1.dy, s2.dy);
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
