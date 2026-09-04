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
    CbResetContent(hwnd);
    int n = len(items);
    for (int i = 0; i < n; i++) {
        CbAddString(hwnd, items[i]);
    }
}

// Same result as SetDropDownItems() but without CB_RESETCONTENT, which on an
// editable combo also empties the edit control. Deleting an item the list has
// selected empties it too, so let go of the selection first - that way exactly
// one step here can disturb the edit, and the caller undoes it.
static void ReplaceDropDownItems(HWND hwnd, StrVec& items) {
    CbSetCurrentSelection(hwnd, -1);
    for (int i = CbGetItemsCount(hwnd) - 1; i >= 0; i--) {
        CbDeleteString(hwnd, i);
    }
    int n = len(items);
    for (int i = 0; i < n; i++) {
        CbInsertString(hwnd, i, items[i]);
    }
}

// On WM_SIZE a combo box re-sets its edit's text (to the same string) and
// leaves all of it selected. Any relayout around us - the find bar's "n / m"
// slot widening as the count comes in - would then wipe out what the user is
// typing, so put the caret back where it was (issue #6068).
static LRESULT CALLBACK DropDownProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg != WM_SIZE) {
        return DefSubclassProc(hwnd, msg, wp, lp);
    }
    int start = 0, end = 0;
    CbEditGetSelection(hwnd, start, end);
    LRESULT res = DefSubclassProc(hwnd, msg, wp, lp);
    CbEditSelectText(hwnd, start, end);
    return res;
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
    if (CbEditHwnd(hwnd)) {
        SetWindowSubclass(hwnd, DropDownProc, 0, 0);
    }
    CbSetCurrentSelection(hwnd, -1);
    CbSetMinVisible(hwnd, 10);
    if (colorSwatches) {
        int dy = ColorSwatchItemDy(font);
        CbSetItemHeight(hwnd, 0, dy);
        CbSetItemHeight(hwnd, -1, dy);
    }

    SizeToIdealSize(this);
    return hwnd;
}

// Editable combos keep the keyboard focus on the inner Edit. Focusing the
// ComboBox HWND itself uses the main accelerator table, so a leftover F from
// Ctrl+F can fire CmdToggleFullscreen and take focus away.
void DropDown::SetFocus() {
    HWND edit = CbEditHwnd(hwnd);
    HwndSetFocus(edit ? edit : hwnd);
}

bool DropDown::IsFocused() const {
    if (!hwnd) {
        return false;
    }
    if (HwndIsFocused(hwnd)) {
        return true;
    }
    HWND edit = CbEditHwnd(hwnd);
    return edit && HwndIsFocused(edit);
}

void DropDown::SetItems(StrVec& newItems) {
    items.Reset();
    VecReset(itemColors);
    int n = len(newItems);
    for (int i = 0; i < n; i++) {
        Str s = newItems[i];
        items.Append(s);
    }
    SetDropDownItems(hwnd, items);
    CbSetCurrentSelection(hwnd, -1);
}

// Rebuild the list under a user who is still typing, e.g. the find history
// growing while find-as-you-type runs. Do not go through SetItems: both the
// CB_RESETCONTENT it starts with and the CbSetCurrentSelection(-1) it ends
// with empty a CBS_DROPDOWN edit.
void DropDown::SetItemsKeepText(StrVec& newItems) {
    Str cur = str::Dup(GetTextTemp());
    int selStart = 0, selEnd = 0;
    CbEditGetSelection(hwnd, selStart, selEnd);
    bool prev = suppressNotify;
    suppressNotify = true;

    items.Reset();
    VecReset(itemColors);
    int nItems = len(newItems);
    for (int i = 0; i < nItems; i++) {
        items.Append(newItems[i]);
    }
    ReplaceDropDownItems(hwnd, items);

    // dropping the list selection empties the edit; put the text back. The
    // text is what the box means now - the list index it came from is stale
    // once the list is rebuilt.
    if (!str::Eq(GetTextTemp(), cur)) {
        SetText(cur);
        int n = len(cur);
        if (selStart > n) {
            selStart = n;
        }
        if (selEnd > n) {
            selEnd = n;
        }
        CbEditSelectText(hwnd, selStart, selEnd);
    }
    str::Free(cur);
    suppressNotify = prev;
}

static void DropDownItemsFromStringArray(StrVec& items, SeqStrings strings) {
    for (Str s = SeqStrFirst(strings); len(s) > 0; s = SeqStrNext(s)) {
        items.Append(s);
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

void DropDown::SetCursorId(LPWSTR id) {
    cursorId = id;
}

// the base/Win.h combo box helpers, taking the control instead of its HWND
static HWND HwndOf(DropDown* dd) {
    return dd ? dd->hwnd : nullptr;
}

void CbSetCueBanner(DropDown* dd, Str s) {
    CbSetCueBanner(HwndOf(dd), s);
}

int CbGetTextLen(DropDown* dd) {
    return CbGetTextLen(HwndOf(dd));
}

bool CbIsDropped(DropDown* dd) {
    return CbIsDropped(HwndOf(dd));
}

int CbGetCurrentSelection(DropDown* dd) {
    return CbGetCurrentSelection(HwndOf(dd));
}

void CbSetCurrentSelection(DropDown* dd, int n) {
    if (!dd) {
        return;
    }
    if (n < 0) {
        CbSetCurrentSelection(dd->hwnd, -1);
        return;
    }
    ReportIf(n >= len(dd->items));
    CbSetCurrentSelection(dd->hwnd, n);
}

HWND CbEditHwnd(DropDown* dd) {
    return CbEditHwnd(HwndOf(dd));
}

void CbEditSelectAll(DropDown* dd) {
    CbEditSelectAll(HwndOf(dd));
}

void CbEditSelectText(DropDown* dd, int start, int end) {
    CbEditSelectText(HwndOf(dd), start, end);
}

void CbEditGetSelection(DropDown* dd, int& start, int& end) {
    CbEditGetSelection(HwndOf(dd), start, end);
}

void CbEditSetModified(DropDown* dd, bool on) {
    CbEditSetModified(HwndOf(dd), on);
}

bool CbEditIsModified(DropDown* dd) {
    return CbEditIsModified(HwndOf(dd));
}
