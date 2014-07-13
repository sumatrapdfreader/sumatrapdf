/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "LabelWithCloseWnd.h"

#include "GdiPlusUtil.h"
#include "WinUtil.h"

/*
TODO:
 . dpi adjust values
*/

#define CLOSE_BTN_DX 16
#define CLOSE_BTN_DY 16

#define WND_CLASS_NAME          L"LabelWithCloseWndClass"

struct LabelWithCloseWnd {
    HWND                    hwnd;
    HFONT                   font;
    int                     cmd;

    RectI                   closeBtnPos;
    COLORREF                txtCol;
    COLORREF                bgCol;

    // in pixels
    int                     padL, padR, padT, padB;

    // data that needs to be freed
    const WCHAR *           label;
};

static bool IsMouseOverClose(LabelWithCloseWnd *w)
{
    PointI p;
    GetCursorPosInHwnd(w->hwnd, p);
    return w->closeBtnPos.Contains(p);
}

// Draws the 'x' close button in regular state or onhover state
// Tries to mimic visual style of Chrome tab close button
static void DrawCloseButton(HDC hdc, RectI& r, bool onHover)
{
    Graphics g(hdc);
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPageUnit(UnitPixel);

    Color c;

    // in onhover state, background is a red-ish circle
    if (onHover) {
        c.SetFromCOLORREF(COL_CLOSE_HOVER_BG);
        SolidBrush b(c);
        g.FillEllipse(&b, r.x, r.y, r.dx-2, r.dy-2);
    }

    // draw 'x'
    c.SetFromCOLORREF(onHover ? COL_CLOSE_X_HOVER : COL_CLOSE_X);
    g.TranslateTransform((float)r.x, (float)r.y);
    Pen p(c, 2);
    if (onHover) {
        g.DrawLine(&p, Point(4,      4), Point(r.dx-6, r.dy-6));
        g.DrawLine(&p, Point(r.dx-6, 4), Point(4,      r.dy-6));
    } else {
        g.DrawLine(&p, Point(4,      5), Point(r.dx-6, r.dy-5));
        g.DrawLine(&p, Point(r.dx-6, 5), Point(4,      r.dy-5));
    }
}

static void OnPaint(LabelWithCloseWnd *w)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(w->hwnd, &ps);
    HBRUSH br = CreateSolidBrush(w->bgCol);
    FillRect(hdc, &ps.rcPaint, br);

    ClientRect cr(w->hwnd);

    int x = w->padL;
    int y = w->padT;
    UINT opts = ETO_OPAQUE;
    if (IsRtl(w->hwnd)) {
        opts = opts | ETO_RTLREADING;
    }

    if (w->font) {
        SelectObject(hdc, w->font);
    }
    ::SetTextColor(hdc, w->txtCol);
    ::SetBkColor(hdc, w->bgCol);

    ExtTextOutW(hdc, x, y, opts, NULL, w->label, str::Len(w->label), NULL);

    // Text might be too long and invade close button area. We just re-paint
    // the background, which is not the pretties but works.
    // A better way would be to intelligently truncate text or shrink the font
    // size (within reason)
    x = w->closeBtnPos.x - 8;
    RectI ri(x, 0, cr.dx - x, cr.dy);
    RECT r = ri.ToRECT();
    FillRect(hdc, &r, br);

    DrawCloseButton(hdc, w->closeBtnPos, IsMouseOverClose(w));
    DeleteObject(br);
    EndPaint(w->hwnd, &ps);
}

static void CalcCloseButtonPos(LabelWithCloseWnd *w, int dx, int dy)
{
    // TODO: dpi-adjust
    int x = dx - w->padR - CLOSE_BTN_DX;
    int y = 0;
    if (dy > CLOSE_BTN_DY) {
        y  = (dy - CLOSE_BTN_DY) / 2;
    }
    w->closeBtnPos = RectI(x, y, CLOSE_BTN_DX, CLOSE_BTN_DY);
}

static LRESULT CALLBACK WndProcLabelWithClose(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (WM_ERASEBKGND == msg) {
        return TRUE; // tells Windows we handle background erasing so it doesn't do it
    }

    LabelWithCloseWnd *w = NULL;
    if (WM_NCCREATE == msg) {
        LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lp);
        w = reinterpret_cast<LabelWithCloseWnd *>(lpcs->lpCreateParams);
        w->hwnd = hwnd;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(w));
        goto DoDefault;
    } else {
        w = reinterpret_cast<LabelWithCloseWnd *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (!w) {
        goto DoDefault;
    }
    
    // to match other controls, preferred way is explict SetFont() call
    if (WM_SETFONT == msg) {
        SetFont(w, (HFONT)wp);
        return 0;
    }

    if (WM_GETFONT == msg) {
        return (HRESULT)w->font;
    }

    if (WM_SIZE == msg) {
        int dx = LOWORD(lp);
        int dy = HIWORD(lp);
        CalcCloseButtonPos(w, dx, dy);
        ScheduleRepaint(hwnd);
        return 0;
    }

    if (WM_MOUSEMOVE == msg) {
        ScheduleRepaint(w->hwnd);

        if (IsMouseOverClose(w)) {
            // ask for WM_MOUSELEAVE notifications
            TRACKMOUSEEVENT tme = { 0 };
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
        }
        goto DoDefault;
    }

    if (WM_MOUSELEAVE == msg) {
        ScheduleRepaint(w->hwnd);
        return 0;
    }

    if (WM_LBUTTONUP == msg) {
        if (IsMouseOverClose(w)) {
            HWND parent = GetParent(w->hwnd);
            SendMessage(parent, WM_COMMAND, w->cmd, 0);
        }
        return 0;
    }

    if (WM_PAINT == msg) {
        OnPaint(w);
        return 0;
    }

DoDefault:
    return DefWindowProc(hwnd, msg, wp, lp);
}

void RegisterLabelWithCloseWnd()
{
    WNDCLASSEX wcex;
    FillWndClassEx(wcex, WND_CLASS_NAME, WndProcLabelWithClose);
    RegisterClassEx(&wcex);
}

void SetLabel(LabelWithCloseWnd *w, const WCHAR *label)
{
    free((void*)w->label);
    // TODO: use SetWindowText() instead?
    w->label = str::Dup(label);
    // TODO: only when visible and parent visible?
    ScheduleRepaint(w->hwnd);
}

void SetBgCol(LabelWithCloseWnd *w, COLORREF c)
{
    w->bgCol = c;
    ScheduleRepaint(w->hwnd);
}

// cmd is both the id of the window as well as id of WM_COMMAND sent
// when close button is clicked
LabelWithCloseWnd *CreateLabelWithCloseWnd(HWND parent, int cmd)
{
    LabelWithCloseWnd *w = AllocStruct<LabelWithCloseWnd>();
    w->cmd = cmd;
    w->bgCol = GetSysColor(COLOR_BTNFACE);
    w->txtCol = GetSysColor(COLOR_WINDOWTEXT);

    // sets w->hwnd during WM_NCCREATE
    CreateWindow(WND_CLASS_NAME, L"", WS_VISIBLE | WS_CHILD,
                           0, 0, 0, 0, parent, (HMENU)cmd,
                           GetModuleHandle(NULL), w);
    CrashIf(!w->hwnd);
    return w;
}

HWND GetHwnd(LabelWithCloseWnd* w)
{
    return w->hwnd;
}

SizeI GetIdealSize(LabelWithCloseWnd* w)
{
    SizeI size = TextSizeInHwnd(w->hwnd, w->label);
    // TDOO: dpi-adjust close button size
    size.dx += CLOSE_BTN_DX;
    // TODO: dpi-adjust 8
    size.dx += 8; // minimum distance between text and and close button
    size.dx += w->padL + w->padR;
    if (size.dy < CLOSE_BTN_DY) {
        size.dy = CLOSE_BTN_DY;
    }
    size.dy += w->padT + w->padB;
    return size;
}

void SetFont(LabelWithCloseWnd* w, HFONT f)
{
    w->font = f;
}

void SetPaddingXY(LabelWithCloseWnd *w, int x, int y)
{
    w->padL = x;
    w->padR = x;
    w->padT = y;
    w->padB = y;
    ScheduleRepaint(w->hwnd);
}

void Free(LabelWithCloseWnd* w)
{
    free((void*)w->label);
    free(w);
}

