/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "utils/BitManip.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/EditCtrl.h"

// https://docs.microsoft.com/en-us/windows/win32/controls/edit-controls

// TODO:
// - expose EN_UPDATE
// (http://msdn.microsoft.com/en-us/library/windows/desktop/bb761687(v=vs.85).aspx)
// - add border and possibly other decorations by handling WM_NCCALCSIZE, WM_NCPAINT and
// WM_NCHITTEST
//   etc., http://www.catch22.net/tuts/insert-buttons-edit-control
// - include value we remember in WM_NCCALCSIZE in GetIdealSize()

Kind kindEdit = "edit";
 
bool IsEdit(Kind kind) {
    return kind == kindEdit;
}

bool IsEdit(ILayout* l) {
    return IsLayoutOfKind(l, kindEdit);
}

ILayout* NewEditLayout(EditCtrl* e) {
    return new WindowBaseLayout(e, kindEdit);
}

void EditCtrl::WndProcParent(WndProcArgs* args) {
    EditCtrl* w = this;

    UINT msg = args->msg;
    WPARAM wp = args->wparam;
    LPARAM lp = args->lparam;

    HWND hwndCtrl = (HWND)lp;
    if (hwndCtrl != w->hwnd) {
        return;
    }

    if (WM_CTLCOLOREDIT == msg) {
        if (w->bgBrush == nullptr) {
            args->result = DefSubclassProc(hwnd, msg, wp, lp);
            return;
        }
        HDC hdc = (HDC)wp;
        // SetBkColor(hdc, w->bgCol);
        SetBkMode(hdc, TRANSPARENT);
        if (w->textColor != ColorUnset) {
            ::SetTextColor(hdc, w->textColor);
        }
        args->didHandle = true;
        args->result = (INT_PTR)w->bgBrush;
        return;
    }

    if (WM_COMMAND == msg) {
        if (EN_CHANGE == HIWORD(wp)) {
            if (w->OnTextChanged) {
                EditTextChangedArgs eargs{};
                eargs.procArgs = args;
                str::Str s = win::GetTextUtf8(w->hwnd);
                eargs.text = s.as_view();
                w->OnTextChanged(&eargs);
                if (args->didHandle) {
                    return;
                }
            }
        }
    }

    // TODO: handle WM_CTLCOLORSTATIC for read-only/disabled controls
}

void EditCtrl::WndProc(WndProcArgs* args) {
    EditCtrl* w = this;
    if (w->msgFilter) {
        w->msgFilter(args);
        if (args->didHandle) {
            return;
        }
    }
}

#if 0
void EditCtrl::SetColors(COLORREF txtCol, COLORREF bgCol) {
    DeleteObject(this->bgBrush);
    this->bgBrush = nullptr;
    if (txtCol != NO_CHANGE) {
        this->txtCol = txtCol;
    }
    if (bgCol != NO_CHANGE) {
        this->bgCol = bgCol;
    }
    if (this->bgCol != NO_COLOR) {
        this->bgBrush = CreateSolidBrush(bgCol);
    }
}
#endif

static bool HwndSetCueText(HWND hwnd, std::string_view s) {
    if (!hwnd) {
        return false;
    }
    auto* ws = strconv::Utf8ToWchar(s);
    bool ok = Edit_SetCueBannerText(hwnd, ws) == TRUE;
    free(ws);
    return ok;
}

bool EditCtrl::SetCueText(std::string_view s) {
    cueText.Set(s);
    return HwndSetCueText(hwnd, cueText.AsView());
}

void EditCtrl::SetSelection(int start, int end) {
    Edit_SetSel(hwnd, start, end);
}

EditCtrl::EditCtrl(HWND p) : WindowBase(p) {
    dwStyle = WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL;
    winClass = WC_EDIT;
    kind = kindEdit;
}

bool EditCtrl::Create() {
    // Note: has to remember this here because when I GetWindowStyle() later on,
    // WS_BORDER is not set, which is a mystery, because it is being drawn.
    // also, WS_BORDER seems to be painted in client area
    hasBorder = bit::IsMaskSet<DWORD>(dwStyle, WS_BORDER);

    bool ok = WindowBase::Create();
    if (!ok) {
        return false;
    }

    HwndSetCueText(hwnd, cueText.AsView());
    // Subclass();
    SubclassParent();
    return true;
}

EditCtrl::~EditCtrl() {
    DeleteObject(bgBrush);
}

#if 0
    RECT curr = params->rgrc[0];
    w->ncDx = RectDx(orig) - RectDx(curr);
    w->ncDy = RectDy(orig) - RectDy(curr);
    return res;
#endif

#if 0
static void NcCalcSize(HWND hwnd, NCCALCSIZE_PARAMS* params) {
    WPARAM wp = (WPARAM)TRUE;
    LPARAM lp = (LPARAM)params;
    SendMessageW(hwnd, WM_NCCALCSIZE, wp, lp);
}
#endif

SIZE EditCtrl::GetIdealSize() {
    SIZE s1 = MeasureTextInHwnd(hwnd, L"Minimal", hfont);
    WCHAR* txt = win::GetText(hwnd);
    SIZE s2 = MeasureTextInHwnd(hwnd, txt, hfont);
    free(txt);

    int dx = std::max(s1.cx, s2.cx);
    int dy = std::max(s2.cy, s2.cy);
    SIZE res{dx, dy};

    LRESULT margins = SendMessage(hwnd, EM_GETMARGINS, 0, 0);
    int lm = (int)LOWORD(margins);
    int rm = (int)HIWORD(margins);
    res.cx += lm + rm;

    if (this->hasBorder) {
        res.cx += 4;
        res.cy += 4;
    }

    return res;
}
