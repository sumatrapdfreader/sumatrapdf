/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/GdiPlusUtil.h"
#include "utils/WinUtil.h"
#include "AppColors.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"

extern bool IsUIRightToLeft(); // SumatraPDF.h

#define NOTIFICATION_WND_CLASS_NAME L"SUMATRA_PDF_NOTIFICATION_WINDOW"

constexpr int PROGRESS_WIDTH = 188;
constexpr int PROGRESS_HEIGHT = 5;
constexpr int PADDING = 6;
constexpr int TOP_LEFT_MARGIN = 8;
constexpr int TIMEOUT_TIMER_ID = 1;

static void RegisterNotificationsWndClass();

static RectI GetCancelRect(HWND hwnd) {
    return RectI(ClientRect(hwnd).dx - 16 - PADDING, PADDING, 16, 16);
}

NotificationWnd::NotificationWnd(HWND parent, int timeoutInMS) {
    this->parent = parent;
    this->timeoutInMS = timeoutInMS;
    this->hasCancel = (0 == timeoutInMS);
}

NotificationWnd::~NotificationWnd() {
    DestroyWindow(this->hwnd);
    DeleteObject(this->font);
    str::Free(this->progressMsg);
}

bool NotificationWnd::Create(const WCHAR* msg, const WCHAR* progressMsg) {
    if (progressMsg != nullptr) {
        this->hasCancel = true;
        this->hasProgress = true;
        this->progressMsg = str::Dup(progressMsg);
    }

    RegisterNotificationsWndClass();

    NONCLIENTMETRICS ncm = {};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    this->font = CreateFontIndirect(&ncm.lfMessageFont);

    HDC hdc = GetDC(parent);
    progressWidth = MulDiv(PROGRESS_WIDTH, GetDeviceCaps(hdc, LOGPIXELSX), USER_DEFAULT_SCREEN_DPI);
    ReleaseDC(parent, hdc);

    auto h = GetModuleHandleW(nullptr);
    const WCHAR* clsName = NOTIFICATION_WND_CLASS_NAME;
    DWORD style = WS_CHILD | SS_CENTER;
    DWORD exStyle = WS_EX_TOPMOST;
    int x = TOP_LEFT_MARGIN;
    int y = TOP_LEFT_MARGIN;
    this->hwnd = CreateWindowExW(exStyle, clsName, msg, style, x, y, 0, 0, parent, (HMENU)0, h, nullptr);
    if (this->hwnd == nullptr) {
        return false;
    }

    SetWindowLongPtr(this->hwnd, GWLP_USERDATA, (LONG_PTR)this);
    DWORD flags = CS_DROPSHADOW | WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT;
    // TODO: this is suspicious. Why CS_DROPSHADOW is mixed with WS_EX_LAYOUTRTL ?
    ToggleWindowExStyle(this->hwnd, flags, IsUIRightToLeft());
    UpdateWindowPosition(msg, true);
    ShowWindow(this->hwnd, SW_SHOW);

    if (this->timeoutInMS != 0) {
        SetTimer(this->hwnd, TIMEOUT_TIMER_ID, this->timeoutInMS, nullptr);
    }
    return true;
}

void NotificationWnd::UpdateWindowPosition(const WCHAR* message, bool init) {
    // compute the length of the message
    RECT rc = ClientRect(this->hwnd).ToRECT();

    HDC hdc = GetDC(this->hwnd);
    HFONT oldfnt = SelectFont(hdc, font);
    DrawText(hdc, message, -1, &rc, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
    SelectFont(hdc, oldfnt);
    ReleaseDC(this->hwnd, hdc);

    RectI rMsg = RectI::FromRECT(rc);
    if (this->hasCancel) {
        rMsg.dy = std::max(rMsg.dy, 16);
        rMsg.dx += 20;
    }
    rMsg.Inflate(PADDING, PADDING);

    if (this->shrinkLimit < 1.0f) {
        ClientRect rcOrig(this->hwnd);
        if (rMsg.dx < rcOrig.dx && rMsg.dx > rcOrig.dx * shrinkLimit) {
            rMsg.dx = rcOrig.dx;
        }
    }

    // adjust the window to fit the message (only shrink the window when there's no progress bar)
    UINT flags = SWP_NOMOVE | SWP_NOZORDER;
    if (!this->hasProgress) {
        SetWindowPos(this->hwnd, nullptr, 0, 0, rMsg.dx, rMsg.dy, flags);
    } else if (init) {
        RectI r = WindowRect(this->hwnd);
        r.dx = std::max(progressWidth + 2 * PADDING, rMsg.dx);
        r.dy = rMsg.dy + PROGRESS_HEIGHT + PADDING / 2;
        SetWindowPos(this->hwnd, nullptr, 0, 0, r.dx, r.dy, flags);
    } else if (rMsg.dx > progressWidth + 2 * PADDING) {
        SetWindowPos(this->hwnd, nullptr, 0, 0, rMsg.dx, WindowRect(this->hwnd).dy, flags);
    }

    // move the window to the right for a right-to-left layout
    if (IsUIRightToLeft()) {
        HWND parent = GetParent(this->hwnd);
        RectI r = MapRectToWindow(WindowRect(this->hwnd), HWND_DESKTOP, parent);
        int cxVScroll = GetSystemMetrics(SM_CXVSCROLL);
        r.x = WindowRect(parent).dx - r.dx - TOP_LEFT_MARGIN - cxVScroll;
        flags = SWP_NOSIZE | SWP_NOZORDER;
        SetWindowPos(this->hwnd, nullptr, r.x, r.y, 0, 0, flags);
    }
}

void NotificationWnd::UpdateMessage(const WCHAR* message, int timeoutInMS, bool highlight) {
    win::SetText(this->hwnd, message);
    this->highlight = highlight;
    if (timeoutInMS != 0) {
        this->hasCancel = false;
    }
    DWORD flags = CS_DROPSHADOW | WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT;
    ToggleWindowExStyle(this->hwnd, flags, IsUIRightToLeft());
    this->UpdateWindowPosition(message, false);
    InvalidateRect(this->hwnd, nullptr, TRUE);
    if (timeoutInMS != 0) {
        SetTimer(this->hwnd, TIMEOUT_TIMER_ID, timeoutInMS, nullptr);
    }
}

using namespace Gdiplus;

static void NotificationWndOnPaint(HWND hwnd, NotificationWnd* wnd) {
    PAINTSTRUCT ps = {0};
    HDC hdcWnd = BeginPaint(hwnd, &ps);

    ClientRect rect(hwnd);
    DoubleBuffer buffer(hwnd, rect);
    HDC hdc = buffer.GetDC();
    HFONT oldfnt = SelectFont(hdc, wnd->font);

    RECT rTmp = rect.ToRECT();

    Graphics graphics(hdc);
    auto col = GetAppColor(AppColor::NotificationsBg);
    SolidBrush br(GdiRgbFromCOLORREF(col));
    graphics.FillRectangle(&br, Rect(0, 0, rTmp.right - rTmp.left, rTmp.bottom - rTmp.top));

    if (wnd->highlight) {
        SetBkMode(hdc, OPAQUE);
        col = GetAppColor(AppColor::NotificationsHighlightText);
        SetTextColor(hdc, col);
        col = GetAppColor(AppColor::NotificationsHighlightBg);
        SetBkColor(hdc, col);
    } else {
        SetBkMode(hdc, TRANSPARENT);
        col = GetAppColor(AppColor::NotificationsText);
        SetTextColor(hdc, col);
    }

    rect.Inflate(-PADDING, -PADDING);
    RectI rectMsg = rect;
    if (wnd->hasProgress) {
        rectMsg.dy -= PROGRESS_HEIGHT + PADDING / 2;
    }
    if (wnd->hasCancel) {
        rectMsg.dx -= 20;
    }
    AutoFreeWstr text(win::GetText(hwnd));
    rTmp = rectMsg.ToRECT();
    DrawText(hdc, text, -1, &rTmp, DT_SINGLELINE | DT_NOPREFIX);

    if (wnd->hasCancel) {
        rTmp = GetCancelRect(hwnd).ToRECT();
        DrawFrameControl(hdc, &rTmp, DFC_CAPTION, DFCS_CAPTIONCLOSE | DFCS_FLAT);
    }

    if (wnd->hasProgress) {
        rect.dx = wnd->progressWidth;
        rect.y += rectMsg.dy + PADDING / 2;
        rect.dy = PROGRESS_HEIGHT;

        col = GetAppColor(AppColor::NotifcationsProgress);
        Pen pen(GdiRgbFromCOLORREF(col));
        graphics.DrawRectangle(&pen, Rect(rect.x, rect.y, rect.dx, rect.dy));

        rect.x += 2;
        rect.dx = (wnd->progressWidth - 3) * wnd->progress / 100;
        rect.y += 2;
        rect.dy -= 3;

        br.SetColor(GdiRgbFromCOLORREF(col));
        graphics.FillRectangle(&br, Rect(rect.x, rect.y, rect.dx, rect.dy));
    }

    SelectFont(hdc, oldfnt);

    buffer.Flush(hdcWnd);
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK NotificationWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    NotificationWnd* wnd = (NotificationWnd*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (WM_ERASEBKGND == msg) {
        // do nothing, helps to avoid flicker
        return TRUE;
    }

    if (WM_TIMER == msg && TIMEOUT_TIMER_ID == wParam) {
        if (wnd->wndRemovedCb)
            wnd->wndRemovedCb(wnd);
        else
            delete wnd;
        return 0;
    }

    if (WM_PAINT == msg && wnd) {
        NotificationWndOnPaint(hwnd, wnd);
        return 0;
    }

    if (WM_SETCURSOR == msg && wnd->hasCancel) {
        PointI pt;
        if (GetCursorPosInHwnd(hwnd, pt) && GetCancelRect(hwnd).Contains(pt)) {
            SetCursor(IDC_HAND);
            return TRUE;
        }
    }

    if (WM_LBUTTONUP == msg && wnd->hasCancel) {
        if (GetCancelRect(hwnd).Contains(PointI(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)))) {
            if (wnd->wndRemovedCb)
                wnd->wndRemovedCb(wnd);
            else
                delete wnd;
            return 0;
        }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void RegisterNotificationsWndClass() {
    static ATOM atom = 0;
    if (atom != 0) {
        // already registered
        return;
    }
    WNDCLASSEX wcex = {};
    FillWndClassEx(wcex, NOTIFICATION_WND_CLASS_NAME, NotificationWndProc);
    wcex.hCursor = LoadCursor(nullptr, IDC_APPSTARTING);
    atom = RegisterClassEx(&wcex);
    CrashIf(!atom);
}

int Notifications::GetWndX(NotificationWnd* wnd) {
    RectI rect = WindowRect(wnd->hwnd);
    rect = MapRectToWindow(rect, HWND_DESKTOP, GetParent(wnd->hwnd));
    return rect.x;
}

void Notifications::MoveBelow(NotificationWnd* fix, NotificationWnd* move) {
    RectI rect = WindowRect(fix->hwnd);
    rect = MapRectToWindow(rect, HWND_DESKTOP, GetParent(fix->hwnd));
    UINT flags = SWP_NOSIZE | SWP_NOZORDER;
    auto x = GetWndX(move);
    int y = rect.y + rect.dy + TOP_LEFT_MARGIN;
    SetWindowPos(move->hwnd, nullptr, x, y, 0, 0, flags);
}

void Notifications::Remove(NotificationWnd* wnd) {
    int pos = wnds.Remove(wnd);
    if (pos < 0) {
        return;
    }
    int n = wnds.size();
    if (n == 0) {
        return;
    }

    bool isFirst = (pos == 0);

    // TODO: this might be busted but I'm not sure what it's supposed
    // to do and it happens rarely. Would need to add a trigger for
    // visually testing notifications
    if (isFirst) {
        auto* first = wnds[0];
        UINT flags = SWP_NOSIZE | SWP_NOZORDER;
        auto x = GetWndX(first);
        SetWindowPos(first->hwnd, nullptr, x, TOP_LEFT_MARGIN, 0, 0, flags);
    }
    for (int i = pos; i < n; i++) {
        if (i == 0) {
            continue;
        }
        auto curr = wnds[i];
        auto prev = wnds[i - 1];
        MoveBelow(prev, curr);
    }
}

void Notifications::Add(NotificationWnd* wnd, NotificationGroupId groupId) {
    if (groupId != nullptr) {
        this->RemoveForGroup(groupId);
    }
    wnd->groupId = groupId;

    if (!wnds.empty()) {
        auto lastIdx = this->wnds.size() - 1;
        MoveBelow(this->wnds[lastIdx], wnd);
    }
    this->wnds.push_back(wnd);
}

NotificationWnd* Notifications::GetForGroup(NotificationGroupId groupId) const {
    CrashIf(!groupId);
    for (auto* wnd : this->wnds) {
        if (wnd->groupId == groupId) {
            return wnd;
        }
    }
    return nullptr;
}

void Notifications::RemoveForGroup(NotificationGroupId groupId) {
    CrashIf(groupId == nullptr);
    std::vector<NotificationWnd*> toRemove;
    for (auto* wnd : this->wnds) {
        if (wnd->groupId == groupId) {
            toRemove.push_back(wnd);
        }
    }
    for (auto* wnd : toRemove) {
        this->RemoveNotification(wnd);
    }
}

void Notifications::RemoveNotification(NotificationWnd* wnd) {
    if (this->Contains(wnd)) {
        this->Remove(wnd);
        delete wnd;
    }
}

void Notifications::Relayout() {
    if (this->wnds.empty()) {
        return;
    }

    auto* first = this->wnds[0];
    HWND hwndCanvas = GetParent(first->hwnd);
    ClientRect frame(hwndCanvas);
    for (auto* wnd : this->wnds) {
        RectI rect = WindowRect(wnd->hwnd);
        rect = MapRectToWindow(rect, HWND_DESKTOP, hwndCanvas);
        if (IsUIRightToLeft()) {
            int cxVScroll = GetSystemMetrics(SM_CXVSCROLL);
            rect.x = frame.dx - rect.dx - TOP_LEFT_MARGIN - cxVScroll;
        } else {
            rect.x = TOP_LEFT_MARGIN;
        }
        UINT flags = SWP_NOSIZE | SWP_NOZORDER;
        SetWindowPos(wnd->hwnd, nullptr, rect.x, rect.y, 0, 0, flags);
    }
}

void NotificationWnd::UpdateProgress(int current, int total) {
    CrashIf(total <= 0);
    if (total <= 0) {
        total = 1;
    }
    progress = limitValue(100 * current / total, 0, 100);
    if (hasProgress && progressMsg) {
        AutoFreeWstr message(str::Format(progressMsg, current, total));
        this->UpdateMessage(message);
    }
}

bool NotificationWnd::WasCanceled() {
    return this->isCanceled;
}
