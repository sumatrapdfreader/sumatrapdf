/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>

#include "wingui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Annotation.h"
#include "SumatraPDF.h"
#include "FormFields.h"

#include "utils/Log.h"

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

    TempStr text = nullptr;
    if (save) {
        if (isChoice) {
            int sel = (int)SendMessageW(h, LB_GETCURSEL, 0, 0);
            if (sel < 0) {
                save = false; // nothing selected
            } else {
                int len = (int)SendMessageW(h, LB_GETTEXTLEN, sel, 0);
                TempWStr buf = AllocArrayTemp<WCHAR>((size_t)len + 1);
                SendMessageW(h, LB_GETTEXT, sel, (LPARAM)buf);
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
                // TODO(phase 3): move to the next/prev field
                CommitFormFieldEdit(true);
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

static HFONT MakeFieldFont(int fieldDy) {
    // a font sized to roughly fill the field height (negative => pixel height).
    // TODO(phase 3): use the field's /DA font size.
    int fontPx = std::max(8, (int)((float)fieldDy * 0.7f));
    return CreateFontW(-fontPx, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Arial");
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
    HWND hEdit = CreateWindowExW(0, WC_EDITW, L"", style, rc.x, rc.y, rc.dx, rc.dy, win->hwndCanvas, nullptr, hmod,
                                 nullptr);
    if (!hEdit) {
        return false;
    }
    HFONT font = MakeFieldFont(rc.dy);
    SetWindowFont(hEdit, font, TRUE);
    int margin = DpiScale(win->hwndCanvas, 2);
    SendMessageW(hEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(margin, margin));
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
    int n = opts.Size();
    if (n == 0) {
        return false;
    }
    int fontPx = std::max(10, (int)((float)rc.dy * 0.7f));
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
    HWND hLb = CreateWindowExW(0, L"LISTBOX", L"", style, x, y, listDx, listDy, win->hwndCanvas, nullptr, hmod, nullptr);
    if (!hLb) {
        return false;
    }
    HFONT font = MakeFieldFont(rc.dy);
    SetWindowFont(hLb, font, TRUE);
    SendMessageW(hLb, LB_SETITEMHEIGHT, 0, (LPARAM)itemDy);

    TempStr cur = GetWidgetValue(widget);
    int curIdx = -1;
    for (int i = 0; i < n; i++) {
        char* o = opts.At(i);
        SendMessageW(hLb, LB_ADDSTRING, 0, (LPARAM)ToWStrTemp(o));
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
    if (rc.dx < 4 || rc.dy < 4) {
        return false;
    }
    if (isChoice) {
        return StartChoiceEdit(win, widget, rc);
    }
    return StartTextEdit(win, widget, rc, flags);
}
