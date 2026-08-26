/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Win.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"

#include "gui/Layout.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/win/WinGui.h"

//- DropDown

// https://docs.microsoft.com/en-us/windows/win32/controls/combo-boxes

static Kind kindDropDown = "dropdown";

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

void DropDown::OnCommand(ControlBase::CommandEvent* ev) {
    if (suppressNotify) {
        return;
    }
    auto code = HIWORD(ev->wparam);
    if (code == CBN_EDITCHANGE && onTextChanged.IsValid()) {
        onTextChanged.Call();
        ev->didHandle = true;
        return;
    }
    if ((code == CBN_SELCHANGE) && onSelectionChanged.IsValid()) {
        onSelectionChanged.Call();
        // must leave didHandle false or else the drop-down list will not close
    }
    if (code == CBN_CLOSEUP && onCloseUp.IsValid()) {
        onCloseUp.Call();
    }
}

static int ColorSwatchItemDy(PlatformFont* font) {
    return PlatformFontMeasureText(font, StrL("Mg")).dy + DpiScale(6);
}

static void PaintCheckerSwatch(Gfx* gfx, Rect r) {
    gfx->FillRect(r, MkRgb(240, 240, 240));
    int s = std::max(r.dx / 4, 2);
    Color dark = MkRgb(180, 180, 180);
    for (int y = 0; y < r.dy; y += s) {
        for (int x = 0; x < r.dx; x += s) {
            if (((x / s) + (y / s)) & 1) {
                int dx = std::min(s, r.dx - x);
                int dy = std::min(s, r.dy - y);
                gfx->FillRect({r.x + x, r.y + y, dx, dy}, dark);
            }
        }
    }
}

static void DrawColorSwatchItem(DropDown* w, DRAWITEMSTRUCT* dis) {
    if (dis->itemID == (UINT)-1) {
        return;
    }
    // Combo owner-draw is a native list DC; GfxHdc paints immediately and
    // does not need Direct2D BindDC (which fails on some 24-bit combo DCs).
    GfxHdc gfx(dis->hDC);
    Rect rc = ToRect(dis->rcItem);
    bool selected = (dis->itemState & ODS_SELECTED) != 0;
    Color bg =
        selected ? GetSysColor(COLOR_HIGHLIGHT) : (IsSpecialColor(w->bgColor) ? GetSysColor(COLOR_WINDOW) : w->bgColor);
    Color txt = selected ? GetSysColor(COLOR_HIGHLIGHTTEXT)
                         : (IsSpecialColor(w->textColor) ? GetSysColor(COLOR_WINDOWTEXT) : w->textColor);
    gfx.FillRect(rc, bg);

    int pad = DpiScale(3);
    int sw = std::max(rc.dy - (2 * pad), 8);
    Rect swatch{rc.x + pad, rc.y + (rc.dy - sw) / 2, sw, sw};
    Color col = kColorTransparent;
    if ((int)dis->itemID < len(w->itemColors)) {
        col = w->itemColors[(int)dis->itemID];
    }
    if (col == kColorTransparent || ColorSkipsPaint(col)) {
        PaintCheckerSwatch(&gfx, swatch);
    } else {
        gfx.FillRect(swatch, col);
    }
    gfx.DrawRect(swatch, MkRgb(80, 80, 80), 1);

    Str name;
    if ((int)dis->itemID < len(w->items)) {
        name = w->items[(int)dis->itemID];
    }
    if (name && w->font) {
        int gap = DpiScale(6);
        Rect textRc{swatch.x + swatch.dx + gap, rc.y, rc.Right() - (swatch.x + swatch.dx + gap + pad), rc.dy};
        gfx.DrawText(name, textRc, gfxTextVCenter | gfxTextSingleLine | gfxTextEllipsis, w->font, txt);
    }
    if (dis->itemState & ODS_FOCUS) {
        gfx.DrawFocusRect(rc);
    }
}

void DropDown::OnMessageReflect(ControlBase::MessageReflectEvent* ev) {
    if (colorSwatches && ev->msg == WM_MEASUREITEM) {
        auto* mis = (MEASUREITEMSTRUCT*)ev->lparam;
        if (mis->CtlType == ODT_COMBOBOX || mis->CtlType == ODT_LISTBOX) {
            mis->itemHeight = (UINT)ColorSwatchItemDy(font);
            ev->result = TRUE;
        }
        return;
    }
    if (colorSwatches && ev->msg == WM_DRAWITEM) {
        auto* dis = (DRAWITEMSTRUCT*)ev->lparam;
        if (dis->CtlType == ODT_COMBOBOX || dis->CtlType == ODT_LISTBOX) {
            DrawColorSwatchItem(this, dis);
            ev->result = TRUE;
        }
        return;
    }
    if (ev->msg == WM_CTLCOLOREDIT || ev->msg == WM_CTLCOLORSTATIC || ev->msg == WM_CTLCOLORLISTBOX) {
        HDC hdc = (HDC)ev->wparam;
        if (!IsSpecialColor(textColor)) {
            SetTextColor(hdc, textColor);
        }
        if (!IsSpecialColor(bgColor)) {
            SetBkColor(hdc, bgColor);
            SetBkMode(hdc, OPAQUE);
        }
        ev->result = (LRESULT)BackgroundBrush();
    }
}

HWND DropDown::Create(const CreateArgs& args) {
    onCommand = MkMethod1<DropDown, ControlBase::CommandEvent*, &DropDown::OnCommand>(this);
    onMessageReflect = MkMethod1<DropDown, ControlBase::MessageReflectEvent*, &DropDown::OnMessageReflect>(this);
    colorSwatches = args.colorSwatches;
    CreateControlArgs cargs;
    cargs.parent = args.parent;
    cargs.isRtl = args.isRtl;
    cargs.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
    if (args.isEditable) {
        cargs.style |= CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL;
    } else {
        cargs.style |= CBS_DROPDOWNLIST | WS_VSCROLL;
    }
    if (colorSwatches) {
        cargs.style |= CBS_OWNERDRAWFIXED | CBS_HASSTRINGS;
        static UINT nextId = 0x500;
        cargs.ctrlId = (HMENU)(INT_PTR)nextId++;
    }
    cargs.className = WC_COMBOBOX;
    cargs.font = args.font;

    ControlBase::CreateControl(cargs);
    if (!hwnd) {
        return nullptr;
    }

    // SetDropDownItems(hwnd, items);
    SetCurrentSelection(-1);
    ComboBox_SetMinVisible(hwnd, 10);
    if (colorSwatches) {
        int dy = ColorSwatchItemDy(font);
        ComboBox_SetItemHeight(hwnd, 0, dy);
        ComboBox_SetItemHeight(hwnd, -1, dy);
    }

    SizeToIdealSize(this);
    return hwnd;
}

// Editable combos keep the keyboard focus on the inner Edit. Focusing the
// ComboBox HWND itself uses the main accelerator table, so a leftover F from
// Ctrl+F can fire CmdToggleFullscreen and take focus away.
void DropDown::SetFocus() {
    HWND edit = EditHwnd();
    HwndSetFocus(edit ? edit : hwnd);
}

HWND DropDown::EditHwnd() const {
    if (!hwnd) {
        return nullptr;
    }
    COMBOBOXINFO info{};
    info.cbSize = sizeof(info);
    if (!GetComboBoxInfo(hwnd, &info)) {
        return nullptr;
    }
    return info.hwndItem;
}

bool DropDown::IsFocused() const {
    if (!hwnd) {
        return false;
    }
    if (HwndIsFocused(hwnd)) {
        return true;
    }
    HWND edit = EditHwnd();
    return edit && HwndIsFocused(edit);
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
    itemColors.Reset();
    int n = len(newItems);
    for (int i = 0; i < n; i++) {
        Str s = newItems[i];
        items.Append(s);
    }
    SetDropDownItems(hwnd, items);
    SetCurrentSelection(-1);
}

// ComboBox_ResetContent clears the edit; keep whatever the user is typing
// and the caret / selection (SetText would otherwise put the caret at 0).
// Do not CB_SETCURSEL(-1) afterwards: that clears a CBS_DROPDOWN edit.
void DropDown::SetItemsKeepText(StrVec& newItems) {
    TempStr cur = GetTextTemp();
    int selStart = 0, selEnd = 0;
    GetSelection(selStart, selEnd);
    bool prev = suppressNotify;
    suppressNotify = true;
    SetItems(newItems);
    SetText(cur);
    int n = len(cur);
    if (selStart > n) {
        selStart = n;
    }
    if (selEnd > n) {
        selEnd = n;
    }
    SetSelection(selStart, selEnd);
    suppressNotify = prev;
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
    Size s1 = PlatformFontMeasureText(font, StrL("Minimal"));

    int n = len(items);
    for (int i = 0; i < n; i++) {
        Str s = items[i];
        Size s2 = PlatformFontMeasureText(font, s);
        s1.dx = std::max(s1.dx, s2.dx);
        s1.dy = std::max(s1.dy, s2.dy);
    }
    // TODO: not sure if I want scrollbar. Only needed if a lot of items
    int dxPad = DpiGetSystemMetrics(SM_CXVSCROLL);
    int dx = s1.dx + dxPad + DpiScale(8);
    if (idealDx > 0 && dx < idealDx) {
        dx = idealDx;
    }
    if (maxDx > 0 && dx > maxDx) {
        dx = maxDx;
    }
    if (colorSwatches) {
        dx += s1.dy + DpiScale(10);
    }
    // TODO: 5 is a guessed number.
    int dyPad = DpiScale(4);
    int dy = s1.dy + dyPad;
    Rect rc = HwndWindowRect(hwnd);
    dy = std::max(rc.dy, dy);
    return {dx, dy};
}

int DropDown::GetTextLen() const {
    return hwnd ? HwndGetTextLen(hwnd) : 0;
}

void DropDown::SelectAll() {
    SetSelection(0, -1);
}

void DropDown::SetSelection(int start, int end) {
    if (hwnd) {
        ComboBox_SetEditSel(hwnd, start, end);
    }
}

void DropDown::GetSelection(int& start, int& end) const {
    start = 0;
    end = 0;
    if (!hwnd) {
        return;
    }
    DWORD sel = ComboBox_GetEditSel(hwnd);
    start = (int)LOWORD(sel);
    end = (int)HIWORD(sel);
}

void DropDown::SetModified(bool on) {
    HWND edit = EditHwnd();
    if (edit) {
        Edit_SetModify(edit, on);
    }
}

bool DropDown::IsModified() const {
    HWND edit = EditHwnd();
    return edit && Edit_GetModify(edit);
}

void DropDown::SetCursorId(LPWSTR id) {
    cursorId = id;
}
