/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/GdiPlusUtil.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"
#include "AppColors.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"

extern bool IsUIRightToLeft(); // SumatraPDF.h

#define NOTIFICATION_WND_CLASS_NAME L"SUMATRA_PDF_NOTIFICATION_WINDOW"

#define PROGRESS_WIDTH DpiScale(188)
#define PROGRESS_HEIGHT DpiScale(5)
#define PADDING DpiScale(6)
#define TOP_LEFT_MARGIN DpiScale(8)
#define CLOSE_LEFT_MARGIN DpiScale(32)

constexpr int TIMEOUT_TIMER_ID = 1;

static LRESULT CALLBACK NotificationWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

static Rect GetCloseRect(HWND hwnd) {
    int n = DpiScale(16);
    Rect r = ClientRect(hwnd);
    int x = r.dx - n - PADDING;
    // int y = PADDING;
    int y = (r.dy / 2) - (n / 2);
    int dx = n;
    int dy = n;
    return Rect(x, y, dx, dy);
}

NotificationWnd::NotificationWnd(HWND parent, int timeoutInMS) {
    this->parent = parent;
    this->timeoutInMS = timeoutInMS;
    this->hasClose = (0 == timeoutInMS);
}

NotificationWnd::~NotificationWnd() {
    DestroyWindow(this->hwnd);
    DeleteObject(this->font);
    str::Free(this->progressMsg);
}

static void UpdateWindowPosition(NotificationWnd* wnd, const WCHAR* message, bool init) {
    // compute the length of the message
    RECT rc = ToRECT(ClientRect(wnd->hwnd));

    HDC hdc = GetDC(wnd->hwnd);
    HFONT oldfnt = SelectFont(hdc, wnd->font);
    DrawText(hdc, message, -1, &rc, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
    SelectFont(hdc, oldfnt);
    ReleaseDC(wnd->hwnd, hdc);

    Rect rMsg = Rect::FromRECT(rc);
    if (wnd->hasClose) {
        rMsg.dy = std::max(rMsg.dy, DpiScale(16));
        rMsg.dx += CLOSE_LEFT_MARGIN;
    }
    rMsg.Inflate(PADDING, PADDING);

    if (wnd->shrinkLimit < 1.0f) {
        Rect rcOrig = ClientRect(wnd->hwnd);
        if (rMsg.dx < rcOrig.dx && rMsg.dx > rcOrig.dx * wnd->shrinkLimit) {
            rMsg.dx = rcOrig.dx;
        }
    }

    // adjust the window to fit the message (only shrink the window when there's no progress bar)
    uint flags = SWP_NOMOVE | SWP_NOZORDER;
    if (!wnd->hasProgress) {
        SetWindowPos(wnd->hwnd, nullptr, 0, 0, rMsg.dx, rMsg.dy, flags);
    } else if (init) {
        Rect r = WindowRect(wnd->hwnd);
        r.dx = std::max(wnd->progressWidth + 2 * PADDING, rMsg.dx);
        r.dy = rMsg.dy + PROGRESS_HEIGHT + PADDING / 2;
        SetWindowPos(wnd->hwnd, nullptr, 0, 0, r.dx, r.dy, flags);
    } else if (rMsg.dx > wnd->progressWidth + 2 * PADDING) {
        SetWindowPos(wnd->hwnd, nullptr, 0, 0, rMsg.dx, WindowRect(wnd->hwnd).dy, flags);
    }

    // move the window to the right for a right-to-left layout
    if (IsUIRightToLeft()) {
        HWND parent = GetParent(wnd->hwnd);
        Rect r = MapRectToWindow(WindowRect(wnd->hwnd), HWND_DESKTOP, parent);
        int cxVScroll = GetSystemMetrics(SM_CXVSCROLL);
        r.x = WindowRect(parent).dx - r.dx - TOP_LEFT_MARGIN - cxVScroll;
        flags = SWP_NOSIZE | SWP_NOZORDER;
        SetWindowPos(wnd->hwnd, nullptr, r.x, r.y, 0, 0, flags);
    }
}

bool NotificationWnd::Create(const WCHAR* msg, const WCHAR* progressMsg) {
    static ATOM atom = 0;
    if (atom == 0) {
        WNDCLASSEX wcex = {};
        FillWndClassEx(wcex, NOTIFICATION_WND_CLASS_NAME, NotificationWndProc);
        wcex.style = 0; // no CS_HREDRAW | CS_VREDRAW
        wcex.hCursor = LoadCursor(nullptr, IDC_APPSTARTING);
        atom = RegisterClassExW(&wcex);
        CrashIf(!atom);
    }

    if (progressMsg != nullptr) {
        hasClose = true;
        hasProgress = true;
        this->progressMsg = str::Dup(progressMsg);
    }

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
    SetRtl(hwnd, IsUIRightToLeft());
    UpdateWindowPosition(this, msg, true);
    ShowWindow(this->hwnd, SW_SHOW);

    if (this->timeoutInMS != 0) {
        SetTimer(this->hwnd, TIMEOUT_TIMER_ID, this->timeoutInMS, nullptr);
    }
    return true;
}

void NotificationWnd::UpdateMessage(const WCHAR* message, int timeoutInMS, bool highlight) {
    win::SetText(hwnd, message);
    this->highlight = highlight;
    if (timeoutInMS != 0) {
        hasClose = false;
    }
    SetRtl(hwnd, IsUIRightToLeft());
    UpdateWindowPosition(this, message, false);
    InvalidateRect(hwnd, nullptr, FALSE);
    if (timeoutInMS != 0) {
        SetTimer(hwnd, TIMEOUT_TIMER_ID, timeoutInMS, nullptr);
    }
}

// using namespace Gdiplus;

using Gdiplus::ARGB;
using Gdiplus::Bitmap;
using Gdiplus::Brush;
using Gdiplus::Color;
using Gdiplus::Font;
using Gdiplus::FontStyle;
using Gdiplus::FontStyleRegular;
using Gdiplus::FontStyleUnderline;
using Gdiplus::Graphics;
using Gdiplus::LinearGradientBrush;
using Gdiplus::LinearGradientMode;
using Gdiplus::Ok;
using Gdiplus::Pen;
using Gdiplus::SolidBrush;
using Gdiplus::Status;

using Gdiplus::CombineModeReplace;
using Gdiplus::CompositingQualityHighQuality;
using Gdiplus::GraphicsPath;
using Gdiplus::Image;
using Gdiplus::Matrix;
using Gdiplus::PenAlignmentInset;
using Gdiplus::Region;
using Gdiplus::SmoothingModeAntiAlias;
using Gdiplus::StringFormat;
using Gdiplus::StringFormatFlagsDirectionRightToLeft;
using Gdiplus::TextRenderingHintClearTypeGridFit;
using Gdiplus::UnitPixel;

static void NotificationWndOnPaint(HWND hwnd, NotificationWnd* wnd) {
    PAINTSTRUCT ps = {0};
    HDC hdcWnd = BeginPaint(hwnd, &ps);

    Rect rect = ClientRect(hwnd);
    DoubleBuffer buffer(hwnd, rect);
    HDC hdc = buffer.GetDC();
    HFONT oldfnt = SelectFont(hdc, wnd->font);

    RECT rTmp = ToRECT(rect);

    COLORREF colBg = GetAppColor(AppColor::NotificationsBg);
    COLORREF colTxt = GetAppColor(AppColor::NotificationsText);
    if (wnd->highlight) {
        colBg = GetAppColor(AppColor::NotificationsHighlightBg);
        colTxt = GetAppColor(AppColor::NotificationsHighlightText);
    }
    // COLORREF colBg = MkRgb(0xff, 0xff, 0x5c);
    // COLORREF colBg = MkGray(0xff);

    Graphics graphics(hdc);

    SolidBrush br(GdiRgbFromCOLORREF(colBg));
    graphics.FillRectangle(&br, Gdiplus::Rect(0, 0, rTmp.right - rTmp.left, rTmp.bottom - rTmp.top));

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, colTxt);

    rect.Inflate(-PADDING, -PADDING);
    Rect rectMsg = rect;
    if (wnd->hasProgress) {
        rectMsg.dy -= PROGRESS_HEIGHT + PADDING / 2;
    }
    if (wnd->hasClose) {
        rectMsg.dx -= CLOSE_LEFT_MARGIN;
    }
    AutoFreeWstr text(win::GetText(hwnd));
    rTmp = ToRECT(rectMsg);
    DrawText(hdc, text, -1, &rTmp, DT_SINGLELINE | DT_NOPREFIX);

    if (wnd->hasClose) {
        rTmp = ToRECT(GetCloseRect(hwnd));
        DrawFrameControl(hdc, &rTmp, DFC_CAPTION, DFCS_CAPTIONCLOSE | DFCS_FLAT);
    }

    if (wnd->hasProgress) {
        rect.dx = wnd->progressWidth;
        rect.y += rectMsg.dy + PADDING / 2;
        rect.dy = PROGRESS_HEIGHT;

        COLORREF col = GetAppColor(AppColor::NotifcationsProgress);
        Pen pen(GdiRgbFromCOLORREF(col));
        graphics.DrawRectangle(&pen, Gdiplus::Rect(rect.x, rect.y, rect.dx, rect.dy));

        rect.x += 2;
        rect.dx = (wnd->progressWidth - 3) * wnd->progress / 100;
        rect.y += 2;
        rect.dy -= 3;

        br.SetColor(GdiRgbFromCOLORREF(col));
        graphics.FillRectangle(&br, Gdiplus::Rect(rect.x, rect.y, rect.dx, rect.dy));
    }

    SelectFont(hdc, oldfnt);

    buffer.Flush(hdcWnd);
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK NotificationWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    NotificationWnd* wnd = (NotificationWnd*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (WM_ERASEBKGND == msg) {
        // do nothing, helps to avoid flicker
        return TRUE;
    }

    if (WM_TIMER == msg && TIMEOUT_TIMER_ID == wp) {
        if (wnd->wndRemovedCb) {
            wnd->wndRemovedCb(wnd);
        } else {
            delete wnd;
        }
        return 0;
    }

    if (WM_PAINT == msg && wnd) {
        NotificationWndOnPaint(hwnd, wnd);
        return 0;
    }

    if (WM_SETCURSOR == msg && wnd->hasClose) {
        Point pt;
        if (GetCursorPosInHwnd(hwnd, pt) && GetCloseRect(hwnd).Contains(pt)) {
            SetCursorCached(IDC_HAND);
            return TRUE;
        }
    }

    if (WM_LBUTTONUP == msg && wnd->hasClose) {
        Point pt = Point(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        if (GetCloseRect(hwnd).Contains(pt)) {
            if (wnd->wndRemovedCb) {
                wnd->wndRemovedCb(wnd);
            } else {
                delete wnd;
            }
            return 0;
        }
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

Notifications::~Notifications() {
    DeleteVecMembers(wnds);
}

bool Notifications::Contains(NotificationWnd* wnd) const {
    return wnds.Contains(wnd);
}

int GetWndX(Notifications* notifs, NotificationWnd* wnd) {
    Rect rect = WindowRect(wnd->hwnd);
    rect = MapRectToWindow(rect, HWND_DESKTOP, GetParent(wnd->hwnd));
    return rect.x;
}

void Notifications::MoveBelow(NotificationWnd* fix, NotificationWnd* move) {
    Rect rect = WindowRect(fix->hwnd);
    rect = MapRectToWindow(rect, HWND_DESKTOP, GetParent(fix->hwnd));
    uint flags = SWP_NOSIZE | SWP_NOZORDER;
    auto x = GetWndX(this, move);
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
        uint flags = SWP_NOSIZE | SWP_NOZORDER;
        auto x = GetWndX(this, first);
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

    if (!wnds.IsEmpty()) {
        auto lastIdx = this->wnds.size() - 1;
        MoveBelow(this->wnds[lastIdx], wnd);
    }
    this->wnds.Append(wnd);
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
    Vec<NotificationWnd*> toRemove;
    for (auto* wnd : this->wnds) {
        if (wnd->groupId == groupId) {
            toRemove.Append(wnd);
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
    if (this->wnds.IsEmpty()) {
        return;
    }

    auto* first = this->wnds[0];
    HWND hwndCanvas = GetParent(first->hwnd);
    Rect frame = ClientRect(hwndCanvas);
    for (auto* wnd : this->wnds) {
        Rect rect = WindowRect(wnd->hwnd);
        rect = MapRectToWindow(rect, HWND_DESKTOP, hwndCanvas);
        if (IsUIRightToLeft()) {
            int cxVScroll = GetSystemMetrics(SM_CXVSCROLL);
            rect.x = frame.dx - rect.dx - TOP_LEFT_MARGIN - cxVScroll;
        } else {
            rect.x = TOP_LEFT_MARGIN;
        }
        uint flags = SWP_NOSIZE | SWP_NOZORDER;
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
