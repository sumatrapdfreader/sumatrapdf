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
// - add border and possibly other decorations by handling WM_NCCALCSIZE, WM_NCPAINT and
// WM_NCHITTEST
//   etc., http://www.catch22.net/tuts/insert-buttons-edit-control
// - include value we remember in WM_NCCALCSIZE in GetIdealSize()

Kind kindEdit = "edit";

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
    if (idealSizeLines < 1) {
        idealSizeLines = 1;
    }
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
    SizeToIdealSize(this);

    if (args.cueText) {
        EditSetCueText(hwnd, args.cueText);
    }
    if (args.text) {
        SetText(args.text);
    }
    return hwnd;
}

LRESULT Edit::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
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

        case WM_PAINT: {
            LRESULT res = WndProcDefault(hwnd, msg, wp, lp);
            if (createdWithBottomBorder) {
                // underline so borderless edits stay visible on flat dialog backgrounds
                HDC hdc = GetDC(hwnd);
                if (hdc) {
                    RECT rc{};
                    GetClientRect(hwnd, &rc);
                    COLORREF col = IsSpecialColor(textColor) ? GetSysColor(COLOR_GRAYTEXT) : textColor;
                    // muted line: blend text color toward background
                    if (!IsSpecialColor(bgColor)) {
                        u8 r, g, b, br, bg, bb;
                        UnpackColor(col, r, g, b);
                        UnpackColor(bgColor, br, bg, bb);
                        col = RGB((r + br * 2) / 3, (g + bg * 2) / 3, (b + bb * 2) / 3);
                    }
                    HPEN pen = CreatePen(PS_SOLID, 1, col);
                    HGDIOBJ old = SelectObject(hdc, pen);
                    MoveToEx(hdc, rc.left, rc.bottom - 1, nullptr);
                    LineTo(hdc, rc.right, rc.bottom - 1);
                    SelectObject(hdc, old);
                    DeleteObject(pen);
                    ReleaseDC(hwnd, hdc);
                }
            }
            return res;
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
    // logf("Edit::GetIdealSize(): dx=%d, dy=%d\n", int(res.cx), int(res.cy));
    return {dx, dy};
}

// horizontal offset of the text from the control's left (window) edge: the
// border (WS_EX_CLIENTEDGE) plus the internal left margin. Useful to align a
// borderless Static label's text with this edit's text.
int Edit::GetLeftTextMargin() {
    int border = 0;
    if (HasBorder()) {
        POINT clientOrigin{0, 0};
        ClientToScreen(hwnd, &clientOrigin);
        RECT wr{};
        GetWindowRect(hwnd, &wr);
        border = clientOrigin.x - wr.left;
    }
    DWORD margins = (DWORD)SendMessageW(hwnd, EM_GETMARGINS, 0, 0);
    int leftMargin = (int)LOWORD(margins);
    return border + leftMargin;
}

// https://docs.microsoft.com/en-us/windows/win32/controls/en-change
// EN_KILLFOCUS also notifies so callers can flush the last edit before blur
// (annotation Contents save; plus df1b2aab8).
bool Edit::OnCommand(WPARAM wparam, LPARAM lparam) {
    auto code = HIWORD(wparam);
    if ((code == EN_CHANGE || code == EN_KILLFOCUS) && onTextChanged.IsValid()) {
        onTextChanged.Call();
        return true;
    }
    return false;
}

LRESULT Edit::OnMessageReflect(UINT msg, WPARAM wp, LPARAM lparam) {
    if (msg == WM_CTLCOLOREDIT) {
        HDC hdc = (HDC)wp;
        if (!IsSpecialColor(textColor)) {
            SetTextColor(hdc, textColor);
        }
        if (!IsSpecialColor(bgColor)) {
            SetBkColor(hdc, bgColor);
            SetBkMode(hdc, TRANSPARENT);
        }
        auto br = BackgroundBrush();
        return (LRESULT)br;
        return 0;
    }
    return 0;
}
