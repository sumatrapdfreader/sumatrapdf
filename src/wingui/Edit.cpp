/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/BitManip.h"
#include "base/Win.h"
#include "base/Dpi.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
#include "wingui/WinGui.h"

//--- Edit

// https://docs.microsoft.com/en-us/windows/win32/controls/edit-controls

// TODO:
// - expose EN_UPDATE
// https://docs.microsoft.com/en-us/windows/win32/controls/en-update
// - fuller border decorations via WM_NCCALCSIZE / WM_NCPAINT / WM_NCHITTEST
//   etc., http://www.catch22.net/tuts/insert-buttons-edit-control

static Kind kindEdit = "edit";

// 1px non-client underline for withBottomBorder (must not sit in the client
// area: edit client paint on typing would overwrite a WM_PAINT GetDC line)
static constexpr int kEditBottomBorderDy = 1;

static bool EditSetCueText(HWND hwnd, Str s) {
    if (!hwnd) {
        return false;
    }
    WCHAR* ws = CWStrTemp(s);
    bool ok = Edit_SetCueBannerText(hwnd, ws) == TRUE;
    return ok;
}

// average character width for sizing edits by character count
static int EditAverageCharDx(HWND hwnd, HFONT font) {
    Size s = HwndMeasureText(hwnd, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", font);
    int ave = (s.dx + 25) / 52;
    if (ave < 1) {
        ave = DpiScale(hwnd, 7);
    }
    return ave;
}

static int EditWidthForChars(HWND hwnd, HFONT font, int nChars) {
    if (nChars <= 0) {
        return 0;
    }
    return EditAverageCharDx(hwnd, font) * nChars;
}

Edit::Edit() {
    kind = kindEdit;
}

Edit::~Edit() {
    // DeleteObject(bgBrush);
}

void Edit::SetSelection(int start, int end) {
    Edit_SetSel(hwnd, start, end);
}

void Edit::SelectAll() {
    TempWStr s = HwndGetTextWTemp(hwnd);
    int pos = len(s);
    Edit_SetSel(hwnd, 0, pos);
}

void Edit::SetCursorPosition(int pos) {
    SetSelection(pos, pos);
}

void Edit::SetCursorPositionAtEnd() {
    TempWStr s = HwndGetTextWTemp(hwnd);
    int pos = len(s);
    SetCursorPosition(pos);
}

// preferred GetIdealSize width ≈ nChars average character widths (0 clears)
// set preferred / max width to ~nChars average character widths (0 clears)
void Edit::SetIdealWidthChars(int nChars) {
    if (!hwnd || nChars <= 0) {
        idealDx = 0;
        return;
    }
    idealDx = EditWidthForChars(hwnd, HwndGetFont(hwnd), nChars);
}

// cap GetIdealSize width at ≈ nChars average character widths (0 clears)
void Edit::SetMaxWidthChars(int nChars) {
    if (!hwnd || nChars <= 0) {
        maxDx = 0;
        return;
    }
    maxDx = EditWidthForChars(hwnd, HwndGetFont(hwnd), nChars);
}

HWND Edit::Create(const CreateArgs& args) {
    // https://docs.microsoft.com/en-us/windows/win32/controls/edit-control-styles
    CreateControlArgs cargs;
    cargs.className = WC_EDITW;
    cargs.parent = args.parent;
    cargs.font = args.font;
    cargs.isRtl = args.isRtl;
    cargs.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT;
    if (args.withBorder) {
        cargs.exStyle = WS_EX_CLIENTEDGE;
        createdWithBorder = true;
    }
    createdWithBottomBorder = args.withBottomBorder && !args.withBorder;
    if (args.isMultiLine) {
        cargs.style |= ES_MULTILINE | WS_VSCROLL | ES_WANTRETURN;
    } else {
        // ES_AUTOHSCROLL disable wrapping in multi-line setup
        cargs.style |= ES_AUTOHSCROLL;
    }
    idealSizeLines = args.idealSizeLines;
    idealSizeLines = std::max(idealSizeLines, 1);
    Wnd::CreateControl(cargs);
    if (!hwnd) {
        return nullptr;
    }
    // character-based ideal/max width (needs hwnd + font for measurement)
    if (args.idealWidthChars > 0) {
        SetIdealWidthChars(args.idealWidthChars);
    }
    if (args.maxWidthChars > 0) {
        SetMaxWidthChars(args.maxWidthChars);
    }
    if (args.textPadding > 0) {
        textPadding = DpiScale(hwnd, args.textPadding);
    }
    SizeToIdealSize(this);
    ApplyTextPadding();

    if (createdWithBottomBorder) {
        // apply the 1px bottom NC strip from WM_NCCALCSIZE
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }

    if (args.cueText) {
        EditSetCueText(hwnd, args.cueText);
    }
    if (args.text) {
        SetText(args.text);
    }
    return hwnd;
}

// inset the text from the client edges by textPadding. The edit control resets
// its formatting rectangle to the full client area on every resize, so this has
// to be re-applied after each WM_SIZE. Ignored by single-line edit controls.
void Edit::ApplyTextPadding() {
    if (!hwnd || textPadding <= 0) {
        return;
    }
    RECT rc;
    GetClientRect(hwnd, &rc);
    InflateRect(&rc, -textPadding, -textPadding);
    if (rc.right <= rc.left || rc.bottom <= rc.top) {
        return;
    }
    SendMessageW(hwnd, EM_SETRECTNP, 0, (LPARAM)&rc);
}

LRESULT Edit::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SIZE: {
            LRESULT res = WndProcDefault(hwnd, msg, wp, lp);
            ApplyTextPadding();
            return res;
        }

        case WM_KEYDOWN: {
            bool isCtrlBack = (VK_BACK == wp) && IsCtrlPressed() && !IsShiftPressed();
            if (isCtrlBack) {
                PostMessageW(hwnd, UWM_DELAYED_CTRL_BACK, 0, 0);
                return true;
            }
            break;
        }

        case UWM_DELAYED_CTRL_BACK: {
            EditImplementCtrlBack(hwnd);
            return true;
        }

        case WM_NCCALCSIZE: {
            if (!createdWithBottomBorder) {
                break;
            }
            // reserve a 1px strip under the client so typing never paints over the line
            LRESULT res = WndProcDefault(hwnd, msg, wp, lp);
            RECT* rc = (wp == TRUE) ? &((NCCALCSIZE_PARAMS*)lp)->rgrc[0] : (RECT*)lp;
            if (rc->bottom - rc->top > kEditBottomBorderDy) {
                rc->bottom -= kEditBottomBorderDy;
            }
            return res;
        }

        case WM_NCPAINT: {
            if (!createdWithBottomBorder) {
                break;
            }
            // borderless edit has no default NC chrome; still call default then draw
            WndProcDefault(hwnd, msg, wp, lp);
            HDC hdc = GetWindowDC(hwnd);
            if (hdc) {
                RECT wr{};
                GetWindowRect(hwnd, &wr);
                int w = wr.right - wr.left;
                int h = wr.bottom - wr.top;
                COLORREF col = EditBottomBorderColor();
                HPEN pen = CreatePen(PS_SOLID, 1, col);
                HGDIOBJ old = SelectObject(hdc, pen);
                MoveToEx(hdc, 0, h - 1, nullptr);
                LineTo(hdc, w, h - 1);
                SelectObject(hdc, old);
                DeleteObject(pen);
                ReleaseDC(hwnd, hdc);
            }
            return 0;
        }
    }
    return WndProcDefault(hwnd, msg, wp, lp);
    // return FinalWindowProc(msg, wp, lp);
}

bool Edit::HasBorder() {
    // don't infer from window styles: with themes darkmodelib strips
    // WS_EX_CLIENTEDGE / WS_BORDER and draws the border in a subclass, which
    // made GetIdealSize() too small for the font
    return createdWithBorder;
}

Size Edit::GetIdealSize() {
    HFONT hfont = HwndGetFont(hwnd);
    Size s1 = HwndMeasureText(hwnd, "Minimal", hfont);
    // logf("Edit::GetIdealSize: s1.dx=%d, s2.dy=%d\n", (int)s1.cx, (int)s1.cy);
    TempStr txt = HwndGetTextTemp(hwnd);
    Size s2 = HwndMeasureText(hwnd, txt, hfont);
    // logf("Edit::GetIdealSize: s2.dx=%d, s2.dy=%d\n", (int)s2.cx, (int)s2.cy);

    int dx = std::max(s1.dx, s2.dx);
    // preferred width in characters (or pixels via idealDx)
    if (idealDx > 0 && dx < idealDx) {
        dx = idealDx;
    }
    // max width in characters (or pixels via maxDx)
    if (maxDx > 0 && dx > maxDx) {
        dx = maxDx;
    }
    // for multi-line text, this measures multiple line.
    // TODO: maybe figure out better protocol
    int dy = std::min(s1.dy, s2.dy);
    if (dy == 0) {
        dy = std::max(s1.dy, s2.dy);
    }
    dy = dy * idealSizeLines;
    // logf("Edit::GetIdealSize: dx=%d, dy=%d\n", (int)dx, (int)dy);

    LRESULT margins = SendMessageW(hwnd, EM_GETMARGINS, 0, 0);
    int lm = (int)LOWORD(margins);
    int rm = (int)HIWORD(margins);
    dx += lm + rm;

    if (HasBorder()) {
        dx += DpiScale(hwnd, 4);
        dy += DpiScale(hwnd, 8);
    }
    if (createdWithBottomBorder) {
        dy += kEditBottomBorderDy;
    }
    // the text is inset on all 4 sides, so the client area has to grow to still
    // show idealSizeLines lines
    dx += textPadding * 2;
    dy += textPadding * 2;
    // logf("Edit::GetIdealSize(): dx=%d, dy=%d\n", int(res.cx), int(res.cy));
    return {dx, dy};
}

// horizontal offset of the text from the control's left (window) edge: the
// border (WS_EX_CLIENTEDGE) plus the internal left margin. Useful to align a
// borderless Static label's text with this edit's text.
// horizontal offset of the text from the control's left edge (border +
// internal margin); used to align a borderless label with the edit's text
int Edit::GetLeftTextMargin() {
    int border = 0;
    if (HasBorder()) {
        Point clientOrigin = HwndClientToScreen(hwnd, Point());
        Rect wr = HwndWindowRect(hwnd);
        border = clientOrigin.x - wr.x;
    }
    DWORD margins = (DWORD)SendMessageW(hwnd, EM_GETMARGINS, 0, 0);
    int leftMargin = (int)LOWORD(margins);
    return border + leftMargin;
}

// https://docs.microsoft.com/en-us/windows/win32/controls/en-change
// EN_CHANGE → onTextChanged; EN_KILLFOCUS → onKillFocus (separate, so a
// filter field that only sets onTextChanged is not re-entered when focus
// moves to an in-place editor — that UAF'd Advanced Settings enum drop-downs).
bool Edit::OnCommand(WPARAM wparam, LPARAM /*lparam*/) {
    auto code = HIWORD(wparam);
    if (code == EN_CHANGE && onTextChanged.IsValid()) {
        onTextChanged.Call();
        return true;
    }
    if (code == EN_KILLFOCUS && onKillFocus.IsValid()) {
        onKillFocus.Call();
        return true;
    }
    return false;
}

LRESULT Edit::OnMessageReflect(UINT msg, WPARAM wp, LPARAM /*lparam*/) {
    // a read-only edit is coloured with WM_CTLCOLORSTATIC, not WM_CTLCOLOREDIT,
    // so handling only the latter left read-only edits with the default white
    // background whatever SetColors() said - like the advanced settings dialog's
    // description field on a dark theme (issue #5895)
    if (msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORSTATIC) {
        HDC hdc = (HDC)wp;
        if (!IsSpecialColor(textColor)) {
            SetTextColor(hdc, textColor);
        }
        if (!IsSpecialColor(bgColor)) {
            SetBkColor(hdc, bgColor);
            SetBkMode(hdc, TRANSPARENT);
        }
        return (LRESULT)BackgroundBrush();
    }
    return 0;
}
