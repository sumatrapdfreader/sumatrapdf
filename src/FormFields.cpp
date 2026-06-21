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

// One field is edited at a time (the active edit box floats over the page).
struct ActiveFormEdit {
    HWND hwnd = nullptr; // WC_EDITW overlay, child of win->hwndCanvas
    HFONT font = nullptr;
    Annotation* widget = nullptr;
    MainWindow* win = nullptr;
    bool multiline = false;
};

static ActiveFormEdit gEdit;
static WNDPROC gDefEditProc = nullptr;
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
    TempStr text = save ? str::DupTemp(HwndGetTextTemp(h)) : nullptr;
    // clear state and unsubclass *before* destroying so the destroy-time
    // WM_KILLFOCUS doesn't re-enter the commit path
    gEdit = {};
    SetWindowLongPtrW(h, GWLP_WNDPROC, (LONG_PTR)gDefEditProc);
    DestroyWindow(h);
    if (font) {
        DeleteObject(font);
    }
    bool changed = false;
    if (save && widget) {
        changed = SetWidgetTextValue(widget, text);
    }
    if (win) {
        HwndSetFocus(win->hwndCanvas);
        if (changed) {
            MainWindowRerender(win);
        }
    }
    gCommitting = false;
}

static LRESULT CALLBACK WndProcFormEdit(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
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
            if (wp == VK_RETURN && !gEdit.multiline) {
                CommitFormFieldEdit(true);
                return 0;
            }
            break;
        case WM_KILLFOCUS:
            // clicking elsewhere (or focusing another control) commits
            CommitFormFieldEdit(true);
            return 0;
    }
    return CallWindowProcW(gDefEditProc, hwnd, msg, wp, lp);
}

bool StartFormFieldEdit(MainWindow* win, Annotation* widget) {
    if (!win || !widget) {
        return false;
    }
    if (GetWidgetType(widget) != PDF_WIDGET_TYPE_TEXT) {
        return false; // phase 1: text fields only
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
    int pageNo = widget->pageNo;
    Rect rc = dm->CvtToScreen(pageNo, widget->bounds); // canvas-client coords
    if (rc.dx < 4 || rc.dy < 4) {
        return false;
    }

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
    // a font sized to roughly fill the field height (negative => pixel height).
    // TODO(phase 3): use the field's /DA font size.
    int fontPx = std::max(8, (int)((float)rc.dy * 0.7f));
    HFONT font = CreateFontW(-fontPx, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Arial");
    SetWindowFont(hEdit, font, TRUE);
    int margin = DpiScale(win->hwndCanvas, 2);
    SendMessageW(hEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(margin, margin));

    HwndSetText(hEdit, GetWidgetValue(widget));

    gDefEditProc = (WNDPROC)GetWindowLongPtrW(hEdit, GWLP_WNDPROC);
    SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)WndProcFormEdit);

    gEdit.hwnd = hEdit;
    gEdit.font = font;
    gEdit.widget = widget;
    gEdit.win = win;
    gEdit.multiline = multiline;

    HwndSetFocus(hEdit);
    Edit_SetSel(hEdit, 0, -1);
    return true;
}
