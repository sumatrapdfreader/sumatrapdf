/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "StrUtil.h"
#include "GeomUtil.h"
#include "WinUtil.h"
#include "Vec.h"

#include "Notifications.h"
#include "SumatraPDF.h"
#include "WindowInfo.h"

#define NOTIFICATION_WND_CLASS_NAME _T("SUMATRA_PDF_NOTIFICATION_WINDOW")

void NotificationWnd::CreatePopup(HWND parent, const TCHAR *message)
{
    NONCLIENTMETRICS ncm = { 0 };
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    font = CreateFontIndirect(&ncm.lfMessageFont);

    HDC hdc = GetDC(parent);
    progressWidth = MulDiv(PROGRESS_WIDTH, GetDeviceCaps(hdc, LOGPIXELSX), USER_DEFAULT_SCREEN_DPI);
    ReleaseDC(parent, hdc);

    self = CreateWindowEx(WS_EX_TOPMOST, NOTIFICATION_WND_CLASS_NAME, message, WS_CHILD | SS_CENTER,
                          TL_MARGIN, TL_MARGIN, 0, 0,
                          parent, (HMENU)0, ghinst, NULL);
    SetWindowLongPtr(self, GWLP_USERDATA, (LONG_PTR)this);
    ToggleWindowStyle(self, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, IsUIRightToLeft(), GWL_EXSTYLE);
    UpdateWindowPosition(message, true);
    ShowWindow(self, SW_SHOW);
}

void NotificationWnd::UpdateWindowPosition(const TCHAR *message, bool init)
{
    // compute the length of the message
    RECT rc = ClientRect(self).ToRECT();
    HDC hdc = GetDC(self);
    HFONT oldfnt = SelectFont(hdc, font);
    DrawText(hdc, message, -1, &rc, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
    SelectFont(hdc, oldfnt);
    ReleaseDC(self, hdc);

    RectI rectMsg = RectI::FromRECT(rc);
    if (hasCancel) {
        rectMsg.dy = max(rectMsg.dy, 16);
        rectMsg.dx += 20;
    }
    rectMsg.Inflate(PADDING, PADDING);

    // adjust the window to fit the message (only shrink the window when there's no progress bar)
    if (!hasProgress) {
        SetWindowPos(self, NULL, 0, 0, rectMsg.dx, rectMsg.dy, SWP_NOMOVE | SWP_NOZORDER);
    } else if (init) {
        RectI rect = WindowRect(self);
        rect.dx = max(progressWidth + 2 * PADDING, rectMsg.dx);
        rect.dy = rectMsg.dy + PROGRESS_HEIGHT + PADDING / 2;
        SetWindowPos(self, NULL, 0, 0, rect.dx, rect.dy, SWP_NOMOVE | SWP_NOZORDER);
    } else if (rectMsg.dx > progressWidth + 2 * PADDING) {
        SetWindowPos(self, NULL, 0, 0, rectMsg.dx, WindowRect(self).dy, SWP_NOMOVE | SWP_NOZORDER);
    }

    // move the window to the right for a right-to-left layout
    if (IsUIRightToLeft()) {
        HWND parent = GetParent(self);
        RectI rect = MapRectToWindow(WindowRect(self), HWND_DESKTOP, parent);
        rect.x = WindowRect(parent).dx - rect.dx - TL_MARGIN - GetSystemMetrics(SM_CXVSCROLL);
        SetWindowPos(self, NULL, rect.x, rect.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
}

bool NotificationWnd::UpdateProgress(int current, int total)
{
    assert(total > 0);
    if (total <= 0)
        total = 1;
    progress = limitValue(100 * current / total, 0, 100);
    if (hasProgress && progressMsg) {
        ScopedMem<TCHAR> message(str::Format(progressMsg, current, total));
        UpdateMessage(message);
    }
    return isCanceled;
}

void NotificationWnd::UpdateMessage(const TCHAR *message, int timeoutInMS, bool highlight)
{
    win::SetText(self, message);
    this->highlight = highlight;
    if (timeoutInMS)
        hasCancel = false;
    ToggleWindowStyle(self, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, IsUIRightToLeft(), GWL_EXSTYLE);
    UpdateWindowPosition(message);
    InvalidateRect(self, NULL, TRUE);
    if (timeoutInMS)
        SetTimer(self, TIMEOUT_TIMER_ID, timeoutInMS, NULL);
}

LRESULT CALLBACK NotificationWnd::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)\
{
    NotificationWnd *wnd = (NotificationWnd *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (WM_ERASEBKGND == message) {
        // do nothing, helps to avoid flicker
        return TRUE;
    }
    if (WM_TIMER == message && TIMEOUT_TIMER_ID == wParam) {
        if (wnd->notificationCb)
            wnd->notificationCb->RemoveNotification(wnd);
        else
            delete wnd;
        return 0;
    }
    if (WM_PAINT == message && wnd) {
        PAINTSTRUCT ps;
        HDC hdcWnd = BeginPaint(hwnd, &ps);

        ClientRect rect(hwnd);
        DoubleBuffer buffer(hwnd, rect);
        HDC hdc = buffer.GetDC();
        HFONT oldfnt = SelectFont(hdc, wnd->font);

        DrawFrameControl(hdc, &rect.ToRECT(), DFC_BUTTON, DFCS_BUTTONPUSH);
        if (wnd->highlight) {
            SetBkMode(hdc, OPAQUE);
            SetTextColor(hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
            SetBkColor(hdc, GetSysColor(COLOR_HIGHLIGHT));
        }
        else {
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
        }

        rect.Inflate(-PADDING, -PADDING);
        RectI rectMsg = rect;
        if (wnd->hasProgress)
            rectMsg.dy -= PROGRESS_HEIGHT + PADDING / 2;
        if (wnd->hasCancel)
            rectMsg.dx -= 20;
        ScopedMem<TCHAR> text(win::GetText(hwnd));
        DrawText(hdc, text, -1, &rectMsg.ToRECT(), DT_SINGLELINE | DT_NOPREFIX);

        if (wnd->hasCancel)
            DrawFrameControl(hdc, &GetCancelRect(hwnd).ToRECT(), DFC_CAPTION, DFCS_CAPTIONCLOSE | DFCS_FLAT);

        if (wnd->hasProgress) {
            rect.dx = wnd->progressWidth;
            rect.y += rectMsg.dy + PADDING / 2;
            rect.dy = PROGRESS_HEIGHT;
            PaintRect(hdc, rect);

            rect.x += 2;
            rect.dx = (wnd->progressWidth - 3) * wnd->progress / 100;
            rect.y += 2;
            rect.dy -= 3;

            HBRUSH brush = GetStockBrush(BLACK_BRUSH);
            FillRect(hdc, &rect.ToRECT(), brush);
            DeleteObject(brush);
        }

        SelectFont(hdc, oldfnt);

        buffer.Flush(hdcWnd);
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (WM_SETCURSOR == message && wnd->hasCancel) {
        POINT pt;
        if (GetCursorPos(&pt) && ScreenToClient(hwnd, &pt) &&
            GetCancelRect(hwnd).Inside(PointI(pt.x, pt.y))) {
            SetCursor(LoadCursor(NULL, IDC_HAND));
            return TRUE;
        }
    }
    if (WM_LBUTTONUP == message && wnd->hasCancel) {
        if (GetCancelRect(hwnd).Inside(PointI(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)))) {
            if (wnd->notificationCb)
                wnd->notificationCb->RemoveNotification(wnd);
            else
                delete wnd;
            return 0;
        }
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}


int Notifications::GetWndX(NotificationWnd *wnd)
{
    RectI rect = WindowRect(wnd->hwnd());
    rect = MapRectToWindow(rect, HWND_DESKTOP, GetParent(wnd->hwnd()));
    return rect.x;
}

void Notifications::MoveBelow(NotificationWnd *fix, NotificationWnd *move)
{
    RectI rect = WindowRect(fix->hwnd());
    rect = MapRectToWindow(rect, HWND_DESKTOP, GetParent(fix->hwnd()));
    SetWindowPos(move->hwnd(), NULL,
                 GetWndX(move), rect.y + rect.dy + NotificationWnd::TL_MARGIN,
                 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

void Notifications::Remove(NotificationWnd *wnd)
{
    int ix = wnds.Find(wnd);
    if (ix == -1)
        return;
    wnds.Remove(wnd);
    if (ix == 0 && wnds.Count() > 0) {
        SetWindowPos(wnds.At(0)->hwnd(), NULL,
                     GetWndX(wnds.At(0)), NotificationWnd::TL_MARGIN,
                     0, 0, SWP_NOSIZE | SWP_NOZORDER);
        ix = 1;
    }
    for (size_t i = ix; i < wnds.Count(); i++) {
        MoveBelow(wnds.At(i - 1), wnds.At(i));
    }
}

void Notifications::Add(NotificationWnd *wnd, int groupId)
{
    // use groupId to classify notifications and make a notification
    // replace any other notification of the same class
    if (groupId != 0)
        RemoveAllInGroup(groupId);
    wnd->groupId = groupId;

    if (wnds.Count() > 0)
        MoveBelow(wnds.At(wnds.Count() - 1), wnd);
    wnds.Append(wnd);
}

NotificationWnd *Notifications::GetFirstInGroup(int groupId)
{
    for (size_t i = 0; i < wnds.Count(); i++) {
        if (wnds.At(i)->groupId == groupId)
            return wnds.At(i);
    }
    return NULL;
}

void Notifications::RemoveNotification(NotificationWnd *wnd)
{
    if (Contains(wnd)) {
        Remove(wnd);
        delete wnd;
    }
}

void Notifications::RemoveAllInGroup(int groupId)
{
    for (size_t i = wnds.Count(); i > 0; i--) {
        if (wnds.At(i - 1)->groupId == groupId)
            RemoveNotification(wnds.At(i - 1));
    }
}

void Notifications::Relayout()
{
    if (wnds.Count() == 0)
        return;

    HWND hwndCanvas = GetParent(wnds.At(0)->hwnd());
    ClientRect frame(hwndCanvas);
    for (size_t i = 0; i < wnds.Count(); i++) {
        RectI rect = WindowRect(wnds.At(i)->hwnd());
        rect = MapRectToWindow(rect, HWND_DESKTOP, hwndCanvas);
        if (IsUIRightToLeft())
            rect.x = frame.dx - rect.dx - NotificationWnd::TL_MARGIN - GetSystemMetrics(SM_CXVSCROLL);
        else
            rect.x = NotificationWnd::TL_MARGIN;
        SetWindowPos(wnds.At(i)->hwnd(), NULL, rect.x, rect.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
}

void ShowNotification(WindowInfo *win, const TCHAR *message, bool autoDismiss, bool highlight, NotificationGroup groupId)
{
    NotificationWnd *wnd = new NotificationWnd(win->hwndCanvas, message, autoDismiss ? 3000 : 0, highlight, win->notifications);
    win->notifications->Add(wnd, groupId);
}

bool RegisterNotificationsWndClass(HINSTANCE inst)
{
    WNDCLASSEX  wcex;
    FillWndClassEx(wcex, inst);
    wcex.lpfnWndProc    = NotificationWnd::WndProc;
    wcex.hCursor        = LoadCursor(NULL, IDC_APPSTARTING);
    wcex.hbrBackground  = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    wcex.lpszClassName  = NOTIFICATION_WND_CLASS_NAME;
    ATOM atom = RegisterClassEx(&wcex);
    return atom != 0;
}
