/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/BitManip.h"
#include "base/ScopedWin.h"
#include "base/Win.h"
#include "base/UITask.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"

#include "gui/Layout.h"
#include "gui/PlatformFont.h"
#include "gui/GuiColors.h"
#include "gui/win/WinGui.h"

//--- Edit

// the base/Win.h edit helpers, taking the control instead of its HWND
static HWND HwndOf(Edit* e) {
    return e ? e->hwnd : nullptr;
}

void EditSelectAll(Edit* e) {
    EditSelectAll(HwndOf(e));
}

void EditSelectText(Edit* e, int start, int end) {
    EditSelectText(HwndOf(e), start, end);
}

void EditGetSelection(Edit* e, int& start, int& end) {
    EditGetSelection(HwndOf(e), start, end);
}

void EditSetCursorPos(Edit* e, int pos) {
    EditSetCursorPos(HwndOf(e), pos);
}

void EditSetCursorPosAtEnd(Edit* e) {
    EditSetCursorPosAtEnd(HwndOf(e));
}

int EditGetTextLen(Edit* e) {
    return EditGetTextLen(HwndOf(e));
}

void EditSetModified(Edit* e, bool on) {
    EditSetModified(HwndOf(e), on);
}

bool EditIsModified(Edit* e) {
    return EditIsModified(HwndOf(e));
}

void EditSetCueText(Edit* e, Str s) {
    EditSetCueText(HwndOf(e), s);
}

void EditSetMargins(Edit* e, int left, int right) {
    EditSetMargins(HwndOf(e), left, right);
}

void EditSetNumbersOnly(Edit* e, bool on) {
    EditSetNumbersOnly(HwndOf(e), on);
}

void EditSetPasswordVisible(Edit* e, bool show) {
    EditSetPasswordVisible(HwndOf(e), show);
}

void EditSetFocus(Edit* e) {
    HwndSetFocus(HwndOf(e));
}

// after the current mouse/focus message so the edit's default processing
// does not undo the selection
static void DelayedEditSelectAll(HWND hwnd) {
    if (hwnd && ::IsWindow(hwnd)) {
        EditSelectAll(hwnd);
    }
}

void PostDelayedEditSelectAll(HWND hwnd) {
    if (!hwnd) {
        return;
    }
    uitask::Post(MkFunc0(DelayedEditSelectAll, hwnd), "DelayedEditSelectAll");
}

// Delete the word behind the cursor after Ctrl+Backspace inserts its temporary glyph.
static void EditImplementCtrlBack(HWND hwnd) {
    // we calc selection in WCHAR space because it's easier
    TempWStr text = HwndGetTextWTemp(hwnd);
    int selStart = 0;
    int selEnd = 0;
    EditGetSelection(hwnd, selStart, selEnd);
    selEnd = selStart;
    // remove the rectangle produced by Ctrl+Backspace
    if (selStart > 0 && text.s[selStart - 1] == '\x7F') {
        memmove(text.s + selStart - 1, text.s + selStart, len(text.s + selStart - 1) * sizeof(WCHAR));
        TempStr s = ToUtf8Temp(text);
        HwndSetText(hwnd, s);
        selStart = selEnd = selStart - 1;
    }
    // remove the previous word (and any spacing after it)
    for (; selStart > 0 && wstr::IsWs(text.s[selStart - 1]); selStart--) {
        ;
    }
    for (; selStart > 0 && !wstr::IsWs(text.s[selStart - 1]); selStart--) {
        ;
    }
    EditSelectText(hwnd, selStart, selEnd);
    SendMessageW(hwnd, WM_CLEAR, 0, 0); // delete selected text
}

// after WM_KEYDOWN so the edit does not insert the Ctrl+Backspace glyph first
static void DelayedEditCtrlBack(HWND hwnd) {
    if (hwnd && ::IsWindow(hwnd)) {
        EditImplementCtrlBack(hwnd);
    }
}

void PostDelayedEditCtrlBack(HWND hwnd) {
    if (!hwnd) {
        return;
    }
    uitask::Post(MkFunc0(DelayedEditCtrlBack, hwnd), "DelayedEditCtrlBack");
}

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

// space between the text and the left / right edges of the client area, in
// (already DPI-scaled) pixels
// the placeholder text shown while the edit is empty
static int EditWidthForChars(PlatformFont* font, int nChars) {
    if (nChars <= 0) {
        return 0;
    }
    return font->averageCharWidth * nChars;
}

Edit::Edit() {
    kind = kindEdit;
}

Edit::~Edit() {
    // DeleteObject(bgBrush);
}

// null restores the edit's own cursor (I-beam). Must not be done with
// GCLP_HCURSOR: that is per window class (WC_EDIT), i.e. every edit in the
// process, so one edit would change the cursor of all the others
void Edit::SetCursorId(LPWSTR id) {
    cursorId = id;
}

// preferred GetIdealSize width ≈ nChars average character widths (0 clears)
// set preferred / max width to ~nChars average character widths (0 clears)
void Edit::SetIdealWidthChars(int nChars) {
    if (!hwnd || nChars <= 0) {
        idealDx = 0;
        return;
    }
    idealDx = EditWidthForChars(font, nChars);
}

// cap GetIdealSize width at ≈ nChars average character widths (0 clears)
void Edit::SetMaxWidthChars(int nChars) {
    if (!hwnd || nChars <= 0) {
        maxDx = 0;
        return;
    }
    maxDx = EditWidthForChars(font, nChars);
}

void Edit::SetIdealWidthFromText(Str s, int extraPx) {
    if (!hwnd || len(s) == 0) {
        return;
    }
    HDC dc = GetDC(hwnd);
    Size sz;
    {
        ScopedSelectFont selectFont(dc, GetHFont());
        // GetTextExtent, not HwndMeasureText: DT_EDITCONTROL underestimates digits
        sz = HdcGetTextExtentPoint32(dc, s);
    }
    ReleaseDC(hwnd, dc);
    idealDx = sz.dx + extraPx;
}

// 0 shows the real text; the bullet matches the old password dialog.
HWND Edit::Create(const CreateArgs& args) {
    // https://docs.microsoft.com/en-us/windows/win32/controls/edit-control-styles
    onWndProc = MkMethod1<Edit, ControlBase::WndProcEvent*, &Edit::WndProc>(this);
    onCommand = MkMethod1<Edit, ControlBase::CommandEvent*, &Edit::OnCommand>(this);
    onMessageReflect = MkMethod1<Edit, ControlBase::MessageReflectEvent*, &Edit::OnMessageReflect>(this);
    CreateControlArgs cargs;
    cargs.className = WC_EDITW;
    cargs.parent = args.parent;
    cargs.font = args.font;
    cargs.isRtl = args.isRtl;
    cargs.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
    cargs.style |= args.alignRight ? ES_RIGHT : ES_LEFT;
    if (args.numbersOnly) {
        cargs.style |= ES_NUMBER;
    }
    if (args.isPassword) {
        cargs.style |= ES_PASSWORD;
    }
    if (args.withBorder) {
        createdWithBorder = true;
        // no WS_EX_CLIENTEDGE: a themed edit draws a blue bottom accent (Win11)
        createdWithFrame = true;
    }
    if (args.withFrame) {
        createdWithFrame = true;
    }
    createdWithBottomBorder = args.withBottomBorder && !createdWithFrame;
    centerTextVert = args.centerTextVert && !args.isMultiLine;
    selectAllOnFocus = args.selectAllOnFocus;
    if (args.isMultiLine) {
        cargs.style |= ES_MULTILINE | WS_VSCROLL | ES_WANTRETURN;
    } else {
        // ES_AUTOHSCROLL disable wrapping in multi-line setup
        cargs.style |= ES_AUTOHSCROLL;
    }
    idealSizeLines = args.idealSizeLines;
    idealSizeLines = std::max(idealSizeLines, 1);
    ControlBase::CreateControl(cargs);
    if (!hwnd) {
        return nullptr;
    }
    if (args.noTheme || createdWithFrame) {
        // strip the Win11 CFD/fluent style so it cannot draw a blue accent
        SetWindowTheme(hwnd, L"", L"");
    }
    // character-based ideal/max width (needs hwnd + font for measurement)
    if (args.idealWidthChars > 0) {
        SetIdealWidthChars(args.idealWidthChars);
    }
    if (args.maxWidthChars > 0) {
        SetMaxWidthChars(args.maxWidthChars);
    }
    if (args.textPadding > 0) {
        textPadding = DpiScale(args.textPadding);
    }
    if (args.marginLeft || args.marginRight) {
        EditSetMargins(hwnd, args.marginLeft, args.marginRight);
    }
    SizeToIdealSize(this);
    ApplyTextPadding();

    if (createdWithBottomBorder || createdWithFrame || centerTextVert) {
        // apply the NC strip from WM_NCCALCSIZE (frame and/or vertical centering)
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

// the brush the control's client is painted with: whatever the parent answers
// to the control-color message (which our own OnMessageReflect answers when
// the colors were set), or the system default if it doesn't answer
HBRUSH Edit::CtlColorBrush(HDC hdc) {
    HWND parent = GetParent(hwnd);
    if (!parent || !hdc) {
        return nullptr;
    }
    // a read-only edit is coloured with WM_CTLCOLORSTATIC (see OnMessageReflect)
    bool readOnly = bit::IsMaskSet(GetWindowLong(hwnd, GWL_STYLE), (long)ES_READONLY);
    UINT msg = readOnly ? WM_CTLCOLORSTATIC : WM_CTLCOLOREDIT;
    auto br = (HBRUSH)SendMessageW(parent, msg, (WPARAM)hdc, (LPARAM)hwnd);
    if (!br) {
        br = (HBRUSH)DefWindowProcW(parent, msg, (WPARAM)hdc, (LPARAM)hwnd);
    }
    return br;
}

void Edit::WndProc(ControlBase::WndProcEvent* ev) {
    HWND hwnd = ev->hwnd;
    UINT msg = ev->msg;
    WPARAM wp = ev->wparam;
    LPARAM lp = ev->lparam;
    switch (msg) {
        case WM_SIZE: {
            LRESULT res = WndProcDefault(hwnd, msg, wp, lp);
            ApplyTextPadding();
            ev->result = res;
            ev->didHandle = true;
            return;
        }

        case WM_CHAR: {
            if (onChar.IsValid()) {
                CharEvent ce;
                ce.c = (int)wp;
                onChar.Call(&ce);
                if (ce.didHandle) {
                    ev->result = 1;
                    ev->didHandle = true;
                    return;
                }
            }
            break;
        }

        case WM_SETCURSOR: {
            if (cursorId && (HWND)wp == hwnd) {
                SetCursor(GetCachedCursor(cursorId));
                ev->result = TRUE;
                ev->didHandle = true;
                return;
            }
            break;
        }

        case WM_LBUTTONDOWN: {
            if (selectAllOnFocus) {
                delaySelectAll = !HwndIsFocused(hwnd);
                if (delaySelectAll) {
                    HWND hwndFg = GetForegroundWindow();
                    DWORD fgTid = hwndFg ? GetWindowThreadProcessId(hwndFg, nullptr) : 0;
                    DWORD ourTid = GetCurrentThreadId();
                    bool attached = false;
                    if (fgTid && fgTid != ourTid) {
                        attached = AttachThreadInput(ourTid, fgTid, TRUE) != 0;
                    }
                    ::SetFocus(hwnd);
                    if (attached) {
                        AttachThreadInput(ourTid, fgTid, FALSE);
                    }
                }
            }
            break;
        }

        case WM_LBUTTONUP: {
            if (selectAllOnFocus && delaySelectAll) {
                int selStart = 0;
                int selEnd = 0;
                EditGetSelection(hwnd, selStart, selEnd);
                if (selStart == selEnd) {
                    PostDelayedEditSelectAll(hwnd);
                }
                delaySelectAll = false;
            }
            break;
        }

        case WM_SETFOCUS: {
            if (selectAllOnFocus && !delaySelectAll) {
                PostDelayedEditSelectAll(hwnd);
            }
            break;
        }

        case WM_KEYDOWN: {
            bool isCtrlBack = (VK_BACK == wp) && IsCtrlPressed() && !IsShiftPressed();
            if (isCtrlBack) {
                PostDelayedEditCtrlBack(hwnd);
                ev->result = true;
                ev->didHandle = true;
                return;
            }
            break;
        }

        case WM_NCCALCSIZE: {
            if (!createdWithBottomBorder && !createdWithFrame && !centerTextVert) {
                break;
            }
            LRESULT res = WndProcDefault(hwnd, msg, wp, lp);
            RECT* rc = (wp == TRUE) ? &((NCCALCSIZE_PARAMS*)lp)->rgrc[0] : (RECT*)lp;
            if (createdWithFrame) {
                if (rc->right - rc->left > 2) {
                    rc->left += 1;
                    rc->right -= 1;
                }
                if (rc->bottom - rc->top > 2) {
                    rc->top += 1;
                    rc->bottom -= 1;
                }
            } else if (createdWithBottomBorder && rc->bottom - rc->top > kEditBottomBorderDy) {
                // reserve a 1px strip under the client so typing never paints over the line
                rc->bottom -= kEditBottomBorderDy;
            }
            // a single-line edit puts its text at the top of the client area, so
            // the only way to move it down is to start the client area lower
            ncCenterTop = 0;
            if (centerTextVert) {
                int surplus = (int)(rc->bottom - rc->top) - LineDy();
                if (surplus > 1) {
                    ncCenterTop = surplus / 2;
                    rc->top += ncCenterTop;
                }
            }
            ev->result = res;
            ev->didHandle = true;
            return;
        }

        case WM_NCPAINT: {
            if (!createdWithBottomBorder && !createdWithFrame && ncCenterTop <= 0) {
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
                // the strip WM_NCCALCSIZE took off the top is outside the client
                // area, so the edit never paints it: fill it like the client
                // SetColors() is optional, so BackgroundBrush() can be null - and
                // then the strip kept whatever pixels happened to be on screen
                // under the control. Nobody else ever paints them: they are
                // outside the client area the edit paints and inside the child
                // rect WS_CLIPCHILDREN keeps the parent out of. Restoring the
                // maximized Save Image window moves the path box onto where the
                // image was, so the image's checkered background stayed in the
                // strip, a few pixels inside the box's top edge.
                HBRUSH bgBr = BackgroundBrush();
                if (!bgBr && ncCenterTop > 0) {
                    bgBr = CtlColorBrush(hdc);
                }
                if (ncCenterTop > 0 && bgBr) {
                    int inset = createdWithFrame ? 1 : 0;
                    RECT tr{inset, inset, w - inset, inset + ncCenterTop};
                    FillRect(hdc, &tr, bgBr);
                }
                Color col = gColsEdit[kColEditBottomBorder];
                if (createdWithFrame) {
                    RECT fr{0, 0, w, h};
                    HBRUSH br = CreateSolidBrush(col);
                    FrameRect(hdc, &fr, br);
                    DeleteObject(br);
                } else {
                    HPEN pen = CreatePen(PS_SOLID, 1, col);
                    HGDIOBJ old = SelectObject(hdc, pen);
                    MoveToEx(hdc, 0, h - 1, nullptr);
                    LineTo(hdc, w, h - 1);
                    SelectObject(hdc, old);
                    DeleteObject(pen);
                }
                ReleaseDC(hwnd, hdc);
            }
            ev->result = 0;
            ev->didHandle = true;
            return;
        }
    }
}

// height of one line of text in the control's font
int Edit::LineDy() {
    return PlatformFontMeasureText(font, StrL("Minimal")).dy;
}

bool Edit::HasBorder() {
    // CreateArgs.withBorder, not window styles: we draw a 1px frame instead
    // of WS_EX_CLIENTEDGE (a themed client-edge is a blue bottom accent on Win11)
    return createdWithBorder;
}

Size Edit::GetIdealSize() {
    Size s1 = PlatformFontMeasureText(font, StrL("Minimal"));
    // logf("Edit::GetIdealSize: s1.dx=%d, s2.dy=%d\n", (int)s1.cx, (int)s1.cy);
    TempStr txt = HwndGetTextTemp(hwnd);
    Size s2 = PlatformFontMeasureText(font, txt);
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
    if (idealDy > 0 && dy < idealDy) {
        dy = idealDy;
    }
    // logf("Edit::GetIdealSize: dx=%d, dy=%d\n", (int)dx, (int)dy);

    LRESULT margins = SendMessageW(hwnd, EM_GETMARGINS, 0, 0);
    int lm = (int)LOWORD(margins);
    int rm = (int)HIWORD(margins);
    dx += lm + rm;

    if (HasBorder()) {
        // room for the 1px frame plus a bit of padding, so a dialog edit is
        // not just text-height + 1px (withFrame-only callers set their own size)
        dx += DpiScale(4);
        dy += DpiScale(8);
    } else if (createdWithFrame) {
        dx += 2;
        dy += 2;
    } else if (createdWithBottomBorder) {
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
void Edit::OnCommand(ControlBase::CommandEvent* ev) {
    auto code = HIWORD(ev->wparam);
    if (code == EN_CHANGE && onTextChanged.IsValid()) {
        onTextChanged.Call();
        ev->didHandle = true;
        return;
    }
    if (code == EN_KILLFOCUS && onKillFocus.IsValid()) {
        onKillFocus.Call();
        ev->didHandle = true;
        return;
    }
    if (code == EN_SETFOCUS && onFocus.IsValid()) {
        onFocus.Call();
        ev->didHandle = true;
    }
}

void Edit::OnMessageReflect(ControlBase::MessageReflectEvent* ev) {
    // a read-only edit is coloured with WM_CTLCOLORSTATIC, not WM_CTLCOLOREDIT,
    // so handling only the latter left read-only edits with the default white
    // background whatever SetColors() said - like the advanced settings dialog's
    // description field on a dark theme (issue #5895)
    if (ev->msg == WM_CTLCOLOREDIT || ev->msg == WM_CTLCOLORSTATIC) {
        HDC hdc = (HDC)ev->wparam;
        if (!IsSpecialColor(textColor)) {
            SetTextColor(hdc, textColor);
        }
        if (!IsSpecialColor(bgColor)) {
            SetBkColor(hdc, bgColor);
            // OPAQUE so character cells are filled before ClearType glyphs.
            // TRANSPARENT left trails / harsh-soft flicker when inserting text
            // (e.g. at the start of the translate dialog source box, issue #5935).
            SetBkMode(hdc, OPAQUE);
        }
        ev->result = (LRESULT)BackgroundBrush();
    }
}
