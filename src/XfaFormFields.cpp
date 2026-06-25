/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"
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
#include "XfaFormFields.h"

struct ActiveXfaEdit {
    HWND hwnd = nullptr;
    HFONT font = nullptr;
    MainWindow* win = nullptr;
    XfaFieldHit field;
    bool isChoice = false;
};

static ActiveXfaEdit gXfaEdit;
static WNDPROC gXfaDefCtrlProc = nullptr;
static bool gXfaCommitting = false;

bool IsXfaFieldEditActive() {
    return gXfaEdit.hwnd != nullptr;
}

void CommitXfaFieldEdit(bool save) {
    if (!gXfaEdit.hwnd || gXfaCommitting) {
        return;
    }
    gXfaCommitting = true;
    HWND h = gXfaEdit.hwnd;
    HFONT font = gXfaEdit.font;
    XfaFieldHit field = gXfaEdit.field;
    MainWindow* win = gXfaEdit.win;
    bool isChoice = gXfaEdit.isChoice;
    EngineBase* engine = win && win->AsFixed() ? win->AsFixed()->GetEngine() : nullptr;

    TempStr text = nullptr;
    if (save) {
        if (isChoice) {
            int sel = (int)SendMessageW(h, LB_GETCURSEL, 0, 0);
            if (sel < 0) {
                save = false;
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
    gXfaEdit = {};
    SetWindowLongPtrW(h, GWLP_WNDPROC, (LONG_PTR)gXfaDefCtrlProc);
    DestroyWindow(h);
    if (font) {
        DeleteObject(font);
    }
    bool changed = false;
    if (save && engine && field.IsValid()) {
        TempStr prev = EngineGetXfaFieldContentTemp(engine, field.name);
        const char* newText = text ? text : "";
        if (EngineSetXfaFieldContent(engine, field.name, newText) && (!prev || !str::Eq(prev, newText))) {
            changed = true;
            EngineMarkXfaPageModified(engine, field.pageNo);
        }
    }
    if (win) {
        HwndSetFocus(win->hwndCanvas);
        if (changed) {
            MainWindowRerender(win);
            ToolbarUpdateStateForWindow(win, false);
        }
    }
    gXfaCommitting = false;
}

static LRESULT CALLBACK WndProcXfaFormCtrl(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    bool isChoice = gXfaEdit.isChoice;
    switch (msg) {
        case WM_GETDLGCODE:
            return DLGC_WANTALLKEYS;
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) {
                CommitXfaFieldEdit(false);
                return 0;
            }
            if (wp == VK_RETURN) {
                CommitXfaFieldEdit(true);
                return 0;
            }
            if (wp == VK_TAB) {
                bool back = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                XfaFieldHit cur = gXfaEdit.field;
                MainWindow* win = gXfaEdit.win;
                CommitXfaFieldEdit(true);
                if (win && cur.IsValid()) {
                    AdvanceXfaFieldTabStop(win, cur, !back);
                }
                return 0;
            }
            break;
        case WM_LBUTTONUP:
            if (isChoice) {
                LRESULT r = CallWindowProcW(gXfaDefCtrlProc, hwnd, msg, wp, lp);
                CommitXfaFieldEdit(true);
                return r;
            }
            break;
        case WM_KILLFOCUS:
            CommitXfaFieldEdit(!isChoice);
            return 0;
    }
    return CallWindowProcW(gXfaDefCtrlProc, hwnd, msg, wp, lp);
}

static HFONT MakeXfaFieldFont(int fontPx) {
    fontPx = std::max(8, fontPx);
    return CreateFontW(-fontPx, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Arial");
}

static int XfaFieldFontPx(const XfaFieldHit& field, Rect rc) {
    float pageDy = field.bounds.dy;
    if (pageDy > 0) {
        float scale = (float)rc.dy / pageDy;
        return std::max(8, (int)(10.0f * scale));
    }
    return std::max(8, (int)((float)rc.dy * 0.7f));
}

static bool StartXfaChoiceEdit(MainWindow* win, const XfaFieldHit& field, Rect rc) {
    DisplayModel* dm = win->AsFixed();
    EngineBase* engine;
    int n;
    if (!dm) {
        return false;
    }
    engine = dm->GetEngine();
    n = EngineGetXfaFieldChoiceCount(engine, field.name);
    if (n == 0) {
        return false;
    }

    int fontPx = XfaFieldFontPx(field, rc);
    int itemDy = fontPx + DpiScale(win->hwndCanvas, 6);
    int visN = std::min(n, 8);
    int listDy = visN * itemDy + DpiScale(win->hwndCanvas, 4);
    int listDx = std::max(rc.dx, DpiScale(win->hwndCanvas, 120));
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
    HFONT font = MakeXfaFieldFont(fontPx);
    SetWindowFont(hLb, font, TRUE);
    SendMessageW(hLb, LB_SETITEMHEIGHT, 0, (LPARAM)itemDy);

    TempStr cur = EngineGetXfaFieldContentTemp(engine, field.name);
    int curIdx = -1;
    for (int i = 0; i < n; i++) {
        TempStr o = EngineGetXfaFieldChoiceOptionTemp(engine, field.name, i);
        if (!o) {
            continue;
        }
        SendMessageW(hLb, LB_ADDSTRING, 0, (LPARAM)ToWStrTemp(o));
        if (curIdx < 0 && str::Eq(o, cur)) {
            curIdx = i;
        }
    }
    SendMessageW(hLb, LB_SETCURSEL, (WPARAM)curIdx, 0);

    gXfaDefCtrlProc = (WNDPROC)GetWindowLongPtrW(hLb, GWLP_WNDPROC);
    SetWindowLongPtrW(hLb, GWLP_WNDPROC, (LONG_PTR)WndProcXfaFormCtrl);

    gXfaEdit.hwnd = hLb;
    gXfaEdit.font = font;
    gXfaEdit.win = win;
    gXfaEdit.field = field;
    gXfaEdit.isChoice = true;

    HwndSetFocus(hLb);
    return true;
}

bool AdvanceXfaFieldTabStop(MainWindow* win, const XfaFieldHit& cur, bool forward) {
    if (!win || !cur.IsValid()) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return false;
    }
    XfaFieldHit next = EngineGetAdjacentXfaField(dm->GetEngine(), cur, forward);
    if (!next.IsValid()) {
        return false;
    }
    return StartXfaFieldInteraction(win, next);
}

bool StartXfaFieldInteraction(MainWindow* win, const XfaFieldHit& field) {
    if (!win || !field.IsValid()) {
        return false;
    }
    if (field.kind == XfaFieldKind::Text) {
        return StartXfaFieldEdit(win, field);
    }
    if (field.kind == XfaFieldKind::Choice) {
        CommitFormFieldEdit(true);
        CommitXfaFieldEdit(true);

        DisplayModel* dm = win->AsFixed();
        if (!dm) {
            return false;
        }
        Rect rc = dm->CvtToScreen(field.pageNo, field.bounds);
        if (dm->ScrollScreenToRect(field.pageNo, rc)) {
            rc = dm->CvtToScreen(field.pageNo, field.bounds);
        }
        if (rc.dx < 4 || rc.dy < 4) {
            return false;
        }
        return StartXfaChoiceEdit(win, field, rc);
    }
    return false;
}

bool StartXfaFieldEdit(MainWindow* win, const XfaFieldHit& field) {
    if (!win || !field.IsValid() || field.kind != XfaFieldKind::Text) {
        return false;
    }
    CommitFormFieldEdit(true);
    CommitXfaFieldEdit(true);

    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return false;
    }
    Rect rc = dm->CvtToScreen(field.pageNo, field.bounds);
    if (dm->ScrollScreenToRect(field.pageNo, rc)) {
        rc = dm->CvtToScreen(field.pageNo, field.bounds);
    }
    if (rc.dx < 4 || rc.dy < 4) {
        return false;
    }

    DWORD style = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL;
    HMODULE hmod = GetModuleHandleW(nullptr);
    HWND hEdit =
        CreateWindowExW(0, WC_EDITW, L"", style, rc.x, rc.y, rc.dx, rc.dy, win->hwndCanvas, nullptr, hmod, nullptr);
    if (!hEdit) {
        return false;
    }
    HFONT font = MakeXfaFieldFont(XfaFieldFontPx(field, rc));
    SetWindowFont(hEdit, font, TRUE);
    int margin = DpiScale(win->hwndCanvas, 2);
    SendMessageW(hEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(margin, margin));
    HwndSetText(hEdit, EngineGetXfaFieldContentTemp(dm->GetEngine(), field.name));

    gXfaDefCtrlProc = (WNDPROC)GetWindowLongPtrW(hEdit, GWLP_WNDPROC);
    SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)WndProcXfaFormCtrl);

    gXfaEdit.hwnd = hEdit;
    gXfaEdit.font = font;
    gXfaEdit.win = win;
    gXfaEdit.field = field;
    gXfaEdit.isChoice = false;

    HwndSetFocus(hEdit);
    Edit_SetSel(hEdit, 0, -1);
    return true;
}

bool ToggleXfaFieldButton(EngineBase* engine, const XfaFieldHit& field) {
    if (!engine || !field.IsValid() || field.kind != XfaFieldKind::Checkbox) {
        return false;
    }
    if (!EngineToggleXfaCheckbox(engine, field.name)) {
        return false;
    }
    EngineMarkXfaPageModified(engine, field.pageNo);
    return true;
}

bool SelectXfaFieldRadio(EngineBase* engine, const XfaFieldHit& field) {
    if (!engine || !field.IsValid() || field.kind != XfaFieldKind::Radio) {
        return false;
    }
    if (!EngineSelectXfaRadio(engine, field.pageNo, field.name, field.bounds)) {
        return false;
    }
    EngineMarkXfaPageModified(engine, field.pageNo);
    return true;
}

WidgetCursorKind GetXfaFieldCursorKind(const XfaFieldHit& field) {
    if (!field.IsValid()) {
        return WidgetCursorKind::None;
    }
    if (field.kind == XfaFieldKind::Text) {
        return WidgetCursorKind::Text;
    }
    if (field.kind == XfaFieldKind::Checkbox || field.kind == XfaFieldKind::Radio ||
        field.kind == XfaFieldKind::Choice) {
        return WidgetCursorKind::Button;
    }
    return WidgetCursorKind::None;
}