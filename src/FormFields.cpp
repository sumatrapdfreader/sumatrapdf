/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "base/Dpi.h"

#include <mupdf/pdf.h>

#include "wingui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "Annotation.h"
#include "SumatraPDF.h"
#include "Toolbar.h"
#include "FormFields.h"

// One field is edited at a time: either a text edit box or a choice list box
// floats over the page.
struct ActiveFormEdit {
    HWND hwnd = nullptr; // WC_EDITW (text) or LISTBOX (choice), child of hwndCanvas
    HFONT font = nullptr;
    Annotation* widget = nullptr;
    MainWindow* win = nullptr;
    bool multiline = false;
    bool isChoice = false;
};

static ActiveFormEdit gEdit;
static WNDPROC gDefCtrlProc = nullptr;
static bool gCommitting = false;

bool IsFormFieldEditActive() {
    return gEdit.hwnd != nullptr;
}

void CommitFormFieldEdit(bool save) {
    if (!gEdit.hwnd || gCommitting) {
        return;
    }
    gCommitting = true;
    HWND h = gEdit.hwnd;
    HFONT font = gEdit.font;
    Annotation* widget = gEdit.widget;
    MainWindow* win = gEdit.win;
    bool isChoice = gEdit.isChoice;

    Str text;
    if (save) {
        if (isChoice) {
            int sel = (int)SendMessageW(h, LB_GETCURSEL, 0, 0);
            if (sel < 0) {
                save = false; // nothing selected
            } else {
                int n = (int)SendMessageW(h, LB_GETTEXTLEN, sel, 0);
                TempWStr buf = AllocArrayTemp<WCHAR>(n + 1);
                SendMessageW(h, LB_GETTEXT, sel, (LPARAM)buf.s);
                text = ToUtf8Temp(buf);
            }
        } else {
            text = str::DupTemp(HwndGetTextTemp(h));
        }
    }
    // clear state and unsubclass *before* destroying so the destroy-time
    // WM_KILLFOCUS doesn't re-enter the commit path
    gEdit = {};
    SetWindowLongPtrW(h, GWLP_WNDPROC, (LONG_PTR)gDefCtrlProc);
    DestroyWindow(h);
    if (font) {
        DeleteObject(font);
    }
    bool changed = false;
    if (save && widget) {
        changed = isChoice ? SetWidgetChoiceValue(widget, text) : SetWidgetTextValue(widget, text);
    }
    if (win) {
        HwndSetFocus(win->hwndCanvas);
        if (changed) {
            MainWindowRerender(win);
            // refresh the tab's unsaved-changes (red dot) indicator and toolbar
            // state now, otherwise it only updates on the next repaint trigger
            // (tab switch, resize)
            ToolbarUpdateStateForWindow(win, false);
        }
    }
    gCommitting = false;
}

static LRESULT CALLBACK WndProcFormCtrl(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    bool isChoice = gEdit.isChoice;
    switch (msg) {
        case WM_GETDLGCODE:
            // we handle Tab / Enter / Esc ourselves
            return DLGC_WANTALLKEYS;
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) {
                CommitFormFieldEdit(false);
                return 0;
            }
            if (wp == VK_TAB) {
                // commit, then move to the next/prev editable field on the page
                bool back = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                Annotation* cur = gEdit.widget;
                MainWindow* win = gEdit.win;
                CommitFormFieldEdit(true);
                DisplayModel* dm = win ? win->AsFixed() : nullptr;
                if (dm && cur) {
                    Annotation* next = EngineMupdfGetAdjacentWidget(dm->GetEngine(), cur, !back);
                    if (next) {
                        StartFormFieldEdit(win, next);
                    }
                }
                return 0;
            }
            if (wp == VK_RETURN && (isChoice || !gEdit.multiline)) {
                CommitFormFieldEdit(true);
                return 0;
            }
            break;
        case WM_LBUTTONUP:
            if (isChoice) {
                // let the listbox finalize the clicked selection, then commit it
                LRESULT r = CallWindowProcW(gDefCtrlProc, hwnd, msg, wp, lp);
                CommitFormFieldEdit(true);
                return r;
            }
            break;
        case WM_KILLFOCUS:
            // text: clicking elsewhere commits; choice: clicking away cancels
            CommitFormFieldEdit(!isChoice);
            return 0;
    }
    return CallWindowProcW(gDefCtrlProc, hwnd, msg, wp, lp);
}

static HFONT MakeFieldFont(int fontPx) {
    fontPx = std::max(8, fontPx);
    // negative height => character height in pixels
    return CreateFontW(-fontPx, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Arial");
}

// the field's on-screen font height in pixels: the /DA font size (PDF points)
// scaled to the page's current zoom, or a height-derived fallback for
// auto-sized (/DA size 0) fields.
static int FieldFontPx(Annotation* widget, Rect rc) {
    float daSize = GetWidgetFontSize(widget);
    float pageDy = widget->bounds.dy; // field height in page (PDF) units
    if (daSize > 0 && pageDy > 0) {
        float scale = (float)rc.dy / pageDy; // screen px per PDF unit
        return std::max(8, (int)(daSize * scale));
    }
    return std::max(8, (int)((float)rc.dy * 0.7f));
}

static bool StartTextEdit(MainWindow* win, Annotation* widget, Rect rc, int flags) {
    bool multiline = (flags & PDF_TX_FIELD_IS_MULTILINE) != 0;
    bool password = (flags & PDF_TX_FIELD_IS_PASSWORD) != 0;
    DWORD style = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL;
    if (multiline) {
        style |= ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN;
    }
    if (password) {
        style |= ES_PASSWORD;
    }
    HMODULE hmod = GetModuleHandleW(nullptr);
    HWND hEdit =
        CreateWindowExW(0, WC_EDITW, L"", style, rc.x, rc.y, rc.dx, rc.dy, win->hwndCanvas, nullptr, hmod, nullptr);
    if (!hEdit) {
        return false;
    }
    HFONT font = MakeFieldFont(FieldFontPx(widget, rc));
    SetWindowFont(hEdit, font, TRUE);
    int margin = DpiScale(win->hwndCanvas, 2);
    SendMessageW(hEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(margin, margin));
    int maxLen = GetWidgetMaxLen(widget); // comb / limited fields (e.g. SSN)
    if (maxLen > 0) {
        SendMessageW(hEdit, EM_SETLIMITTEXT, (WPARAM)maxLen, 0);
    }
    HwndSetText(hEdit, GetWidgetValue(widget));

    gDefCtrlProc = (WNDPROC)GetWindowLongPtrW(hEdit, GWLP_WNDPROC);
    SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)WndProcFormCtrl);

    gEdit.hwnd = hEdit;
    gEdit.font = font;
    gEdit.widget = widget;
    gEdit.win = win;
    gEdit.multiline = multiline;
    gEdit.isChoice = false;

    HwndSetFocus(hEdit);
    Edit_SetSel(hEdit, 0, -1);
    return true;
}

static bool StartChoiceEdit(MainWindow* win, Annotation* widget, Rect rc) {
    StrVec opts;
    GetWidgetChoiceOptions(widget, opts);
    int n = len(opts);
    if (n == 0) {
        return false;
    }
    int fontPx = FieldFontPx(widget, rc);
    int itemDy = fontPx + DpiScale(win->hwndCanvas, 6);
    int visN = std::min(n, 8);
    int listDy = visN * itemDy + DpiScale(win->hwndCanvas, 4);
    int listDx = std::max(rc.dx, DpiScale(win->hwndCanvas, 120));
    // drop down just below the field, or above if it would fall off the canvas
    Rect canvasRc = ClientRect(win->hwndCanvas);
    int x = rc.x;
    int y = rc.y + rc.dy;
    if (y + listDy > canvasRc.dy && rc.y - listDy >= 0) {
        y = rc.y - listDy;
    }
    DWORD style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS;
    HMODULE hmod = GetModuleHandleW(nullptr);
    HWND hLb =
        CreateWindowExW(0, L"LISTBOX", L"", style, x, y, listDx, listDy, win->hwndCanvas, nullptr, hmod, nullptr);
    if (!hLb) {
        return false;
    }
    HFONT font = MakeFieldFont(fontPx);
    SetWindowFont(hLb, font, TRUE);
    SendMessageW(hLb, LB_SETITEMHEIGHT, 0, (LPARAM)itemDy);

    Str cur = GetWidgetValue(widget);
    int curIdx = -1;
    for (int i = 0; i < n; i++) {
        Str o = opts[i];
        SendMessageW(hLb, LB_ADDSTRING, 0, (LPARAM)CWStrTemp(o));
        if (curIdx < 0 && str::Eq(o, cur)) {
            curIdx = i;
        }
    }
    SendMessageW(hLb, LB_SETCURSEL, (WPARAM)curIdx, 0);

    gDefCtrlProc = (WNDPROC)GetWindowLongPtrW(hLb, GWLP_WNDPROC);
    SetWindowLongPtrW(hLb, GWLP_WNDPROC, (LONG_PTR)WndProcFormCtrl);

    gEdit.hwnd = hLb;
    gEdit.font = font;
    gEdit.widget = widget;
    gEdit.win = win;
    gEdit.multiline = false;
    gEdit.isChoice = true;

    HwndSetFocus(hLb);
    return true;
}

bool StartFormFieldEdit(MainWindow* win, Annotation* widget) {
    if (!win || !widget) {
        return false;
    }
    int wt = GetWidgetType(widget);
    bool isText = (wt == PDF_WIDGET_TYPE_TEXT);
    bool isChoice = (wt == PDF_WIDGET_TYPE_COMBOBOX) || (wt == PDF_WIDGET_TYPE_LISTBOX);
    if (!isText && !isChoice) {
        return false;
    }
    int flags = GetWidgetFieldFlags(widget);
    if (flags & PDF_FIELD_IS_READ_ONLY) {
        return false;
    }
    CommitFormFieldEdit(true); // commit any prior edit

    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return false;
    }
    Rect rc = dm->CvtToScreen(widget->pageNo, widget->bounds); // canvas-client coords
    // scroll the field into view if it's off-screen (e.g. Tab moved past the
    // fold), then recompute its on-screen rect
    if (dm->ScrollScreenToRect(widget->pageNo, rc)) {
        rc = dm->CvtToScreen(widget->pageNo, widget->bounds);
    }
    if (rc.dx < 4 || rc.dy < 4) {
        return false;
    }
    if (isChoice) {
        return StartChoiceEdit(win, widget, rc);
    }
    return StartTextEdit(win, widget, rc, flags);
}
