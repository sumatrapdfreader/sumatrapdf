/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "base/ScopedWin.h"
#include "base/Dpi.h"
#include "base/UITask.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/PlatformFont.h"
#include "wingui/Gfx.h"
#include "wingui/VirtCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"

#include "Notifications.h"
#include "Theme.h"

using Gdiplus::Graphics;
using Gdiplus::Pen;
using Gdiplus::SolidBrush;

static StrNode* gDelayedNotifications = nullptr;

Kind kNotifCursorPos = "cursorPosHelper";
Kind kNotifActionResponse = "responseToAction";
Kind kNotifPageInfo = "pageInfoHelper";
// can have multiple of those
Kind kNotifAdHoc = "notifAdHoc";
// debug-only: continuous layout re-done because newly visible pages got measured
Kind kNotifLazyLayout = "notifLazyLayout";

static Kind kindNotifText = "notifText";
static Kind kindNotifProgress = "notifProgress";

constexpr int kPadding = 6;
constexpr int kTopLeftMargin = 8;

constexpr UINT_PTR kNotifTimerTimeoutId = 1;
constexpr UINT_PTR kNotifTimerDelayId = 2;

struct NotificationWnd;

// the colors a notification paints with; they depend on `highlight`
struct NotifColors {
    COLORREF bg;
    COLORREF txt;
    COLORREF link;
};

// the message text. A plain message is a single DrawText; one with links, bold
// runs or key-caps becomes a VirtRichText child, which draws and runs its own
// links
struct NotifTextCtrl : VirtCtrl {
    NotificationWnd* notif = nullptr;
    VirtRichText* rich = nullptr; // owned, as our only child
    // DT_* format for drawing a plain message, set in NotificationWnd::Layout()
    uint txtFmt = DT_SINGLELINE | DT_NOPREFIX;
    Size idealSize;

    NotifTextCtrl();
    ~NotifTextCtrl() override = default;

    Size GetIdealSize() override;
    void Paint(VirtPaintCtx&) override;
};

// the progress bar below the message (only when progressPerc >= 0)
struct NotifProgressCtrl : VirtCtrl {
    NotificationWnd* notif = nullptr;

    NotifProgressCtrl();
    ~NotifProgressCtrl() override = default;

    void Paint(VirtPaintCtx&) override;
};

struct NotificationWnd : WindowBase {
    NotificationWnd() = default;
    ~NotificationWnd() override;

    HWND Create(const NotificationCreateArgs& args);

    void OnPaint(WindowBase::PaintEvent* ev);
    void OnTimer(WindowBase::TimerEvent* ev);

    void UpdateMessage(Str msg, int timeoutMs = 0, bool highlight = false);

    bool HasProgress() const { return progressPerc >= 0; }
    void Layout(Str message);
    void BuildTree(ILayout* customContent);
    NotifColors Colors() const;
    void ScheduleRemove();

    int timeoutMs = kNotifDefaultTimeOut; // 0 means no timeout

    bool highlight = false; // TODO: should really be a color
    bool noClose = false;

    NotificationWndRemoved wndRemovedCb;

    // there can only be a single notification of a given group
    Kind groupId = nullptr;

    // if set, only shown while this is the active tab (see
    // ShowNotificationsForActiveTab)
    WindowTab* tab = nullptr;

    // which canvas corner to anchor to, and distance from the edges
    NotifCorner corner = NotifCorner::TopLeft;
    int xMargin = kNotifDefaultMargin;
    int yMargin = kNotifDefaultMargin;

    // to reduce flicker, we might ask the window to shrink the size less often
    // (notifcation windows are only shrunken if by less than factor shrinkLimit)
    float shrinkLimit = 1.0f;

    int progressPerc = -1;
    int delayInMs = 0;
    UINT_PTR delayTimerId = 0;

    // the controls hosted in our HWND. `container` only owns them - this window
    // positions them itself
    VBox* container = nullptr;
    // either the built-in text or a caller-supplied tree, never both
    NotifTextCtrl* txtCtrl = nullptr;
    ILayout* contentWnd = nullptr;
    VirtCloseButton* closeCtrl = nullptr;
    NotifProgressCtrl* progressCtrl = nullptr;
};

static void NotifCloseClicked(NotificationWnd* wnd, VirtMouseEvent*) {
    wnd->ScheduleRemove();
}

// the control showing the message (built-in) or the caller's custom tree
static ILayout* NotifBody(NotificationWnd* wnd) {
    return wnd->contentWnd ? wnd->contentWnd : (ILayout*)wnd->txtCtrl;
}

constexpr int kMaxNotifs = 128;
static NotificationWnd* gNotifs[kMaxNotifs];
static int gNotifsCount = 0;

static int GetForHwnd(HWND hwnd, NotificationWnd* wnds[kMaxNotifs]) {
    int n = 0;
    for (int i = 0; i < gNotifsCount; i++) {
        auto* wnd = gNotifs[i];
        HWND parent = HwndGetParent(wnd->hwnd);
        if (parent == hwnd) {
            wnds[n++] = wnd;
        }
    }
    return n;
}

static int NotificationIndexOf(NotificationWnd* wnd) {
    for (int i = 0; i < gNotifsCount; i++) {
        if (gNotifs[i] == wnd) {
            return i;
        }
    }
    return -1;
}

static int GetForSameHwnd(NotificationWnd* wnd, NotificationWnd* wnds[kMaxNotifs]) {
    HWND parent = GetParent(wnd->hwnd);
    return GetForHwnd(parent, wnds);
}

// hwndCanvas is the parent of the notification windows
void RelayoutNotifications(HWND hwndCanvas) {
    NotificationWnd* wnds[kMaxNotifs];
    int nWnds = GetForHwnd(hwndCanvas, wnds);
    if (nWnds == 0) {
        return;
    }

    Rect frame = HwndClientRect(hwndCanvas);
    // Canvas can be empty during early startup / DeferWindowPos; nothing to
    // anchor to yet. A later UpdateCanvasSize / RelayoutFrame will retry.
    if (frame.IsEmpty()) {
        return;
    }
    int dyPadding = DpiScale(hwndCanvas, kPadding);
    bool isRtl = IsUIRtl();
    // running vertical offset per position so multiple notifications in the same
    // spot stack toward the opposite edge
    int yOffset[(int)NotifCorner::Count] = {};
    for (int i = 0; i < nWnds; i++) {
        NotificationWnd* wnd = wnds[i];
        if (wnd->delayTimerId != 0) {
            // still in delay period, not yet visible
            continue;
        }
        // Use the window's own WS_VISIBLE, not IsWindowVisible(): the latter
        // walks parents and returns false while the frame/canvas is mid-show
        // (first document open). That skipped positioning so bottom-right
        // toasts (e.g. "Errors in document") stayed at 0,0 / off-canvas until
        // a later reload when parents were already visible.
        if (!(GetWindowStyle(wnd->hwnd) & WS_VISIBLE)) {
            // explicitly hidden (e.g. tied to a non-active tab)
            continue;
        }
        int xMargin = DpiScale(hwndCanvas, wnd->xMargin);
        int yMargin = DpiScale(hwndCanvas, wnd->yMargin);
        NotifCorner corner = wnd->corner;
        bool isBar = (corner == NotifCorner::BottomBar);
        Rect rect = HwndWindowRect(wnd->hwnd);
        // re-measure when the canvas is too small for the current size, or when
        // the notif was first laid out against an empty/wrong parent size. A
        // full-width bar re-measures whenever its width no longer matches the
        // canvas (i.e. the window was resized).
        int maxDx = frame.dx - (2 * xMargin);
        bool needRelayout = rect.dx <= 0 || rect.dy <= 0;
        if (isBar) {
            needRelayout |= (rect.dx != frame.dx);
        } else {
            needRelayout |= (maxDx > 0 && rect.dx > maxDx);
        }
        if (needRelayout) {
            wnd->Layout(HwndGetTextTemp(wnd->hwnd));
            rect = HwndWindowRect(wnd->hwnd);
        }

        bool atRight = (corner == NotifCorner::TopRight) || (corner == NotifCorner::BottomRight);
        bool atBottom = isBar || (corner == NotifCorner::BottomLeft) || (corner == NotifCorner::BottomRight);
        if (isRtl && !isBar) {
            atRight = !atRight; // mirror horizontally for right-to-left UI (a full-width bar is symmetric)
        }
        // the bottom bar spans the full width, so it always sits flush at the
        // left edge; Layout() sized it to the canvas width
        int x = xMargin;
        if (isBar) {
            x = 0;
        } else if (atRight) {
            x = frame.dx - rect.dx - xMargin;
        }
        int idx = (int)corner;
        // the bar is attached flush to the bottom edge (no margin); corner
        // toasts keep their margin
        int barYMargin = isBar ? 0 : yMargin;
        int y = atBottom ? (frame.dy - rect.dy - barYMargin - yOffset[idx]) : (yMargin + yOffset[idx]);
        // keep fully inside the canvas (guards empty/partial layout races)
        x = std::max(x, 0);
        y = std::max(y, 0);
        // SWP_NOCOPYBITS: repaint from scratch instead of copying stale bits, so
        // notifications that shift when another is dismissed draw correctly
        // (OnPaint is double-buffered, so no flicker)
        uint flags = SWP_NOSIZE | SWP_NOZORDER | SWP_NOCOPYBITS;
        SetWindowPos(wnd->hwnd, nullptr, x, y, 0, 0, flags);
        yOffset[idx] += rect.dy + dyPadding;
    }
}

// show notifications tied to activeTab (and untied ones), hide those tied to
// other tabs; call when the active tab changes
void ShowNotificationsForActiveTab(HWND hwndCanvas, WindowTab* activeTab) {
    NotificationWnd* wnds[kMaxNotifs];
    int nWnds = GetForHwnd(hwndCanvas, wnds);
    for (int i = 0; i < nWnds; i++) {
        NotificationWnd* wnd = wnds[i];
        if (wnd->tab == nullptr) {
            continue; // not tied to a tab, always visible
        }
        if (wnd->delayTimerId != 0) {
            continue; // not yet shown; its delay timer controls visibility
        }
        bool show = (wnd->tab == activeTab);
        ShowWindow(wnd->hwnd, show ? SW_SHOW : SW_HIDE);
    }
    RelayoutNotifications(hwndCanvas);
}

static void NotifsRemoveNotification(NotificationWnd* wnd);

// remove notifications tied to a tab (call when the tab is closed)
void RemoveNotificationsForTab(WindowTab* tab) {
    if (!tab) {
        return;
    }
    NotificationWnd* toRemove[kMaxNotifs];
    int nRemove = 0;
    for (int i = 0; i < gNotifsCount; i++) {
        if (gNotifs[i]->tab == tab) {
            toRemove[nRemove++] = gNotifs[i];
        }
    }
    for (int i = 0; i < nRemove; i++) {
        NotifsRemoveNotification(toRemove[i]);
    }
}

static void NotifsRemoveNotification(NotificationWnd* wnd) {
    int pos = NotificationIndexOf(wnd);
    if (pos < 0) {
        return;
    }
    gNotifsCount--;
    if (pos < gNotifsCount) {
        gNotifs[pos] = gNotifs[gNotifsCount];
    }
    gNotifs[gNotifsCount] = nullptr;
    RelayoutNotifications(HwndGetParent(wnd->hwnd));
    delete wnd;
}

__unused static int GetWndX(NotificationWnd* wnd) {
    Rect rect = HwndWindowRect(wnd->hwnd);
    rect = HwndMapRectToWindow(rect, HWND_DESKTOP, GetParent(wnd->hwnd));
    return rect.x;
}

NotificationWnd::~NotificationWnd() {
    if (delayTimerId != 0) {
        KillTimer(hwnd, delayTimerId);
        delayTimerId = 0;
    }
    // ~WindowBase deletes vroot and `layout`, which owns the controls -
    // a caller-supplied contentWnd included
}

NotifColors NotificationWnd::Colors() const {
    NotifColors c;
    if (highlight) {
        c.bg = ThemeNotificationsHighlightColor();
        c.txt = ThemeNotificationsHighlightTextColor();
        c.link = ThemeNotificationsHighlightLinkColor();
    } else {
        c.bg = ThemeNotificationsBackgroundColor();
        c.txt = ThemeNotificationsTextColor();
        c.link = ThemeWindowLinkColor();
    }
    return c;
}

// builds the tree hosted in our HWND: the body (message text or a caller
// supplied tree), the close button and the progress bar
void NotificationWnd::BuildTree(ILayout* customContent) {
    container = new VBox();

    if (customContent) {
        contentWnd = customContent;
        container->AddChild(contentWnd);
    } else {
        txtCtrl = new NotifTextCtrl();
        txtCtrl->notif = this;
        container->AddChild(txtCtrl);
    }

    progressCtrl = new NotifProgressCtrl();
    progressCtrl->notif = this;
    progressCtrl->visibility = Visibility::Collapse;
    container->AddChild(progressCtrl);

    if (!noClose) {
        // last, so it hit-tests on top of the body
        closeCtrl = new VirtCloseButton();
        closeCtrl->onClick = MkFunc1(NotifCloseClicked, this);
        container->AddChild(closeCtrl);
    }
    layout = container;
}

HWND NotificationWnd::Create(const NotificationCreateArgs& args) {
    highlight = args.warning;
    noClose = args.noClose;
    ReportIf(noClose && args.timeoutMs <= 0);
    shrinkLimit = args.shrinkLimit;
    if (shrinkLimit < 0.2f) {
        ReportIf(shrinkLimit < 0.2f);
        shrinkLimit = 1.f;
    }
    if (args.onRemoved.IsValid()) {
        wndRemovedCb = args.onRemoved;
    } else {
        wndRemovedCb = MkFunc1Void(NotifsRemoveNotification);
    }
    timeoutMs = args.timeoutMs;
    tab = args.tab;
    corner = args.corner;
    xMargin = args.xMargin;
    yMargin = args.yMargin;

    onPaint = MkMethod1<NotificationWnd, WindowBase::PaintEvent*, &NotificationWnd::OnPaint>(this);
    onTimer = MkMethod1<NotificationWnd, WindowBase::TimerEvent*, &NotificationWnd::OnTimer>(this);

    CreateCustomArgs cargs;
    cargs.parent = args.hwndParent;
    cargs.font = args.font;
    cargs.exStyle = WS_EX_TOPMOST;
    cargs.style = WS_CHILD | SS_CENTER;
    cargs.title = args.msg;
    if (cargs.font == nullptr) {
        cargs.font = GetAppBiggerFont(args.hwndParent);
    }
    cargs.pos = Rect(0, 0, 0, 0);
    cargs.visible = args.delayInMs == 0;
    cargs.isRtl = IsUIRtl();

    CreateCustom(cargs);
    if (!hwnd) {
        // we took ownership of the content, so it dies with us
        delete args.content;
        return nullptr;
    }

    BuildTree(args.content);
    Layout(args.msg);
    delayInMs = args.delayInMs;
    if (delayInMs > 0) {
        // create hidden, will show after delay
        delayTimerId = SetTimer(hwnd, kNotifTimerDelayId, delayInMs, nullptr);
    } else {
        ShowWindow(hwnd, SW_SHOW);
        if (timeoutMs != 0) {
            SetTimer(hwnd, kNotifTimerTimeoutId, timeoutMs, nullptr);
        }
    }
    return hwnd;
}

// returns 0% - 100%
int CalcPerc(int current, int total) {
    ReportIf(total <= 0 || current < 0);
    ReportIf(total < current);
    if (total <= 0) {
        total = 1;
    }
    int perc = limitValue(100 * current / total, 0, 100);
    return perc;
}

constexpr int kCloseLeftMargin = 16;
constexpr int kProgressDy = 5;

void NotificationWnd::Layout(Str message) {
    if (!message) {
        message = StrL("");
    }
    int padX = DpiScale(hwnd, 12);
    int padY = DpiScale(hwnd, 8);
    int closeDx = DpiScale(hwnd, 16);
    int closeLeftMargin = DpiScale(hwnd, kCloseLeftMargin) - padX;

    // limit the width to the parent window so that the close button
    // stays reachable even for very long messages (issue #2916)
    int topLeftMargin = DpiScale(hwnd, kTopLeftMargin);
    Rect rParent = HwndClientRect(GetParent(hwnd));
    int maxTextDx = rParent.dx - (2 * topLeftMargin) - (2 * padX);
    if (!noClose) {
        maxTextDx -= closeLeftMargin + closeDx + padX;
    }

    bool isRtl = IsUIRtl();

    Size szText;
    if (contentWnd) {
        // caller-supplied tree: it decides its own size, we only cap the width
        Constraints bc = Loose({maxTextDx > 0 ? maxTextDx : Inf, Inf});
        szText = contentWnd->Layout(bc);
    } else {
        // parse the message for the extended tip syntax (links, Key/ shortcuts)
        VirtRichText* parsed = ParseTip(message);
        // rich path also for bold-only messages (e.g. the F7 help), not just links
        bool drawRich = parsed->HasRichContent();
        if (txtCtrl->rich) {
            txtCtrl->RemoveChild(txtCtrl->rich, true);
            txtCtrl->rich = nullptr;
        }

        if (drawRich) {
            // rich text: the words lay themselves out (links included). Note: no
            // RTL word reordering, same as the home page tips.
            txtCtrl->txtFmt = DT_LEFT | DT_NOPREFIX;
            parsed->font = GetPlatformFont(font);
            // link commands go to the top-level window (the main frame)
            parsed->hwndForCmds = GetAncestor(hwnd, GA_ROOT);
            txtCtrl->rich = parsed;
            txtCtrl->AddChild(parsed);
            int areaWidth = (maxTextDx > 0) ? maxTextDx : (1 << 20);
            parsed->LayoutText(areaWidth);
            szText.dx = parsed->totalDx;
            szText.dy = parsed->totalDy;
        } else {
            // plain text: render exactly like before (RTL handling, wrapping). Only
            // substitute the parsed text when there were (Key/...) / (Kbd/...) so
            // ordinary messages keep their original whitespace/newlines.
            if (str::Contains(message, StrL("(Key/")) || str::Contains(message, StrL("(Kbd/"))) {
                message = parsed->PlainTextTemp();
                HwndSetText(hwnd, message);
            }
            uint fmt = DT_SINGLELINE | DT_NOPREFIX;
            if (isRtl) {
                fmt |= DT_RIGHT | DT_RTLREADING;
            }
            HDC hdc = GetDC(hwnd);
            szText = HdcMeasureText(hdc, message, fmt, font);
            if (maxTextDx > 0 && szText.dx > maxTextDx) {
                // too wide: word-wrap the message; DT_WORD_ELLIPSIS truncates
                // words too long to wrap (e.g. long file paths)
                fmt = DT_WORDBREAK | DT_WORD_ELLIPSIS | DT_NOPREFIX;
                szText = HdcMeasureText(hdc, message, maxTextDx, fmt, font);
                szText.dx = std::min(szText.dx, maxTextDx);
            }
            ReleaseDC(hwnd, hdc);
            txtCtrl->txtFmt = fmt;
            delete parsed;
        }
        txtCtrl->idealSize = szText;
    }

    Rect rTxt;
    Rect rClose;
    Rect rProgress;
    int dx = padX + szText.dx + padX;
    int dy = padY + szText.dy + padY;
    int closeBlockDx = closeLeftMargin + closeDx + padX;
    if (!noClose) {
        if (isRtl) {
            rClose = {padX, padY, closeDx, closeDx + 2};
            int textX = padX + closeBlockDx;
            rTxt = {textX, padY, szText.dx, szText.dy};
            dx = textX + szText.dx + padX;
        } else {
            rTxt = {padX, padY, szText.dx, szText.dy};
            rClose = {dx + closeLeftMargin, padY, closeDx, closeDx + 2};
            dx += closeBlockDx;
        }
    } else {
        rTxt = {padX, padY, szText.dx, szText.dy};
        rClose = {};
        dx += padX;
    }
    int progressDy = DpiScale(hwnd, kProgressDy);
    rProgress = {rTxt.x, dy, rTxt.dx, progressDy};
    if (HasProgress()) {
        dy += padY + progressDy + padY;
    }

    Rect rCurr = HwndWindowRect(hwnd);
    // for less flicker we don't want to shrink the window when the text shrinks
    if (dx < rCurr.dx) {
        int diff = rCurr.dx - dx;
        if (isRtl) {
            rTxt.dx += diff;
        } else {
            rClose.x += diff;
        }
        dx = rCurr.dx;
    }
    // but never wider than the parent window (issue #2916)
    int maxDx = rParent.dx - (2 * topLeftMargin);
    if (maxDx > 0 && dx > maxDx) {
        int diff = dx - maxDx;
        if (isRtl) {
            rTxt.dx -= diff;
        } else {
            rClose.x -= diff;
        }
        dx = maxDx;
    }
#if 0
    if (dy < rCurr.dy) {
        dy = rCurr.dy;
    }
#endif
#if 0
    if (wnd->shrinkLimit < 1.0f) {
        Rect rcOrig = HwndClientRect(wnd->hwnd);
        if (rMsg.dx < rcOrig.dx && rMsg.dx > rcOrig.dx * wnd->shrinkLimit) {
            rMsg.dx = rcOrig.dx;
        }
    }
#endif

    // full-width bottom bar: stretch to the canvas width and center the text
    // (the close button, if any, stays pinned to the right edge)
    if (corner == NotifCorner::BottomBar && rParent.dx > 0) {
        dx = rParent.dx;
        int reserveRight = noClose ? 0 : closeBlockDx;
        rTxt.x = std::max((dx - reserveRight - szText.dx) / 2, padX);
        rTxt.y = padY;
        if (!noClose) {
            rClose.x = dx - closeDx - padX;
        }
    }

    // y-center close
    if (!noClose) {
        rClose.y = ((dy - rClose.dx) / 2) + 1;
    }

    // hand the computed geometry to the controls; this window positions them
    // itself, so the tree is only collected, not laid out
    RefreshVirtTops(hwnd, layout, Rect{0, 0, dx, dy}, &vroot);
    ILayout* body = NotifBody(this);
    body->SetBounds(rTxt);
    if (txtCtrl && txtCtrl->rich) {
        txtCtrl->rich->SetBounds(rTxt);
    }
    if (closeCtrl) {
        closeCtrl->idealSize = {rClose.dx, rClose.dy};
        closeCtrl->SetBounds(rClose);
    }
    progressCtrl->visibility = HasProgress() ? Visibility::Visible : Visibility::Collapse;
    progressCtrl->SetBounds(rProgress);

    if (dx == rCurr.dx && dy == rCurr.dy) {
        return;
    }

    // adjust the window to fit the message (only shrink the window when there's no progress bar)
    uint flags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE;
    SetWindowPos(hwnd, nullptr, 0, 0, dx, dy, flags);

    // move the window to the right for a right-to-left layout
    if (IsUIRtl()) {
        HWND parent = GetParent(hwnd);
        Rect r = HwndMapRectToWindow(HwndWindowRect(hwnd), HWND_DESKTOP, parent);
        int cxVScroll = DpiGetSystemMetrics(hwnd, SM_CXVSCROLL);
        r.x = HwndWindowRect(parent).dx - r.dx - DpiScale(hwnd, kTopLeftMargin) - cxVScroll;
        flags = SWP_NOSIZE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOACTIVATE | SWP_DEFERERASE;
        SetWindowPos(hwnd, nullptr, r.x, r.y, 0, 0, flags);
    }
}

// TODO: figure out why it flickers
void NotificationWnd::OnPaint(WindowBase::PaintEvent* ev) {
    HDC hdcIn = ev->hdc;
    Rect rc = HwndClientRect(hwnd);
    DoubleBuffer buffer(hwnd, rc);
    HDC hdc = buffer.GetDC();
    // HDC hdc = hdcIn;

    ScopedSelectObject fontPrev(hdc, font);

    NotifColors cols = Colors();
    HdcFillRect(hdc, rc, cols.bg);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, cols.txt);
    // the controls (message / custom content, close button, progress) paint
    // themselves
    GfxHdc gfx(hdc);
    if (vroot) {
        vroot->Paint(&gfx, rc);
    }

    buffer.Flush(hdcIn);
}

//--- the VirtCtrl controls making up a notification

NotifTextCtrl::NotifTextCtrl() {
    kind = kindNotifText;
}

Size NotifTextCtrl::GetIdealSize() {
    return idealSize;
}

void NotifTextCtrl::Paint(VirtPaintCtx& ctx) {
    if (rich) {
        // the child paints itself; keep its colors in step with the theme
        NotifColors cols = notif->Colors();
        rich->textColor = cols.txt;
        rich->linkColor = cols.link;
        rich->bgColor = cols.bg;
        return;
    }
    TempStr text = HwndGetTextTemp(notif->hwnd);
    HdcDrawText(GfxGetHdc(ctx.gfx), text, ctx.content, txtFmt, notif->font);
}

NotifProgressCtrl::NotifProgressCtrl() {
    kind = kindNotifProgress;
    flags |= vwfNoHitTest;
}

void NotifProgressCtrl::Paint(VirtPaintCtx& ctx) {
    Rect rc = ctx.bounds;
    int progressWidth = rc.dx;

    COLORREF col = ThemeNotificationsProgressColor();
    Graphics graphics(GfxGetHdc(ctx.gfx));
    Pen pen(GdiRgbFromCOLORREF(col));
    auto grc = Gdiplus::Rect(rc.x, rc.y, rc.dx, rc.dy);
    graphics.DrawRectangle(&pen, grc);

    rc.x += 2;
    rc.dx = (progressWidth - 3) * notif->progressPerc / 100;
    rc.y += 2;
    rc.dy -= 3;

    SolidBrush br(GdiRgbFromCOLORREF(col));
    grc = {rc.x, rc.y, rc.dx, rc.dy};
    graphics.FillRectangle(&br, grc);
}

void NotificationWnd::UpdateMessage(Str msg, int timeoutMs, bool highlight) {
    HwndSetText(hwnd, msg);
    this->highlight = highlight;
    this->timeoutMs = timeoutMs;
    HwndSetRtl(hwnd, IsUIRtl());
    Layout(msg);
    HwndRepaintNow(hwnd);
    if (timeoutMs != 0) {
        SetTimer(hwnd, kNotifTimerTimeoutId, timeoutMs, nullptr);
    }
}

bool UpdateNotificationProgress(NotificationWnd* wnd, Str msg, int perc) {
    if (NotificationIndexOf(wnd) < 0) {
        return false;
    }
    ReportIf(perc < 0 || perc > 100);
    wnd->progressPerc = perc;
    wnd->UpdateMessage(msg);
    return true;
}

static void NotifRemove(NotificationWnd* wnd) {
    wnd->wndRemovedCb.Call(wnd);
}

static void NotifDelete(NotificationWnd* wnd) {
    delete wnd;
}

// Delete off the stack of the message being dispatched (uitask runs after
// dispatch), so the wnd survives until the handler returns.
void NotificationWnd::ScheduleRemove() {
    if (wndRemovedCb.IsValid()) {
        auto fn = MkFunc0<NotificationWnd>(NotifRemove, this);
        uitask::Post(fn, "TaskNotifRemove");
    } else {
        auto fn = MkFunc0<NotificationWnd>(NotifDelete, this);
        uitask::Post(fn, "TaskNotifDelete");
    }
}

void NotificationWnd::OnTimer(WindowBase::TimerEvent* ev) {
    UINT_PTR timerId = ev->timerId;
    if (timerId == kNotifTimerDelayId) {
        // delay elapsed, now show the notification
        KillTimer(hwnd, delayTimerId);
        delayTimerId = 0;
        BringWindowToTop(hwnd);
        ShowWindow(hwnd, SW_SHOW);
        RelayoutNotifications(HwndGetParent(hwnd));
        if (timeoutMs != 0) {
            SetTimer(hwnd, kNotifTimerTimeoutId, timeoutMs, nullptr);
        }
        return;
    }
    ReportIf(kNotifTimerTimeoutId != timerId);
    ScheduleRemove();
}

// If onlyTab is non-null, only remove notifications in this group that are
// tied to onlyTab (or untied). Leaves other tabs' same-group notifs alone so
// multi-file load can keep a per-tab "Show Errors" toast.
static int NotifsRemoveForGroup(NotificationWnd** wnds, int nWnds, Kind groupId, WindowTab* onlyTab = nullptr) {
    ReportIf(groupId == nullptr);
    NotificationWnd* toRemove[kMaxNotifs];
    int nRemove = 0;
    for (int i = 0; i < nWnds; i++) {
        if (wnds[i]->groupId != groupId) {
            continue;
        }
        if (onlyTab && wnds[i]->tab && wnds[i]->tab != onlyTab) {
            continue;
        }
        toRemove[nRemove++] = wnds[i];
    }
    for (int i = 0; i < nRemove; i++) {
        NotifsRemoveNotification(toRemove[i]);
    }
    return nRemove;
}

static bool NotifsAdd(NotificationWnd** wnds, int nWnds, NotificationWnd* wnd, Kind groupId) {
    bool skipRemove = (groupId == nullptr) || (groupId == kNotifAdHoc);
    if (!skipRemove) {
        // Tab-tied notifs (e.g. document errors) replace only the same tab's
        // previous notif of that group, not other tabs'.
        NotifsRemoveForGroup(wnds, nWnds, groupId, wnd->tab);
    }
    if (gNotifsCount >= kMaxNotifs) {
        return false;
    }
    wnd->groupId = groupId;
    gNotifs[gNotifsCount] = wnd;
    gNotifsCount++;
    RelayoutNotifications(HwndGetParent(wnd->hwnd));
    return true;
}

static bool NotifsAdd(NotificationWnd* wnd, Kind groupId) {
    NotificationWnd* wnds[kMaxNotifs];
    int nWnds = GetForSameHwnd(wnd, wnds);
    return NotifsAdd(wnds, nWnds, wnd, groupId);
}

static NotificationWnd* NotifsGetForGroup(NotificationWnd** wnds, int nWnds, Kind groupId) {
    ReportIf(!groupId);
    for (int i = 0; i < nWnds; i++) {
        if (wnds[i]->groupId == groupId) {
            return wnds[i];
        }
    }
    return nullptr;
}

NotificationWnd* ShowNotification(const NotificationCreateArgs& args) {
    ReportIf(!args.hwndParent);

    NotificationWnd* wnd = new NotificationWnd();
    wnd->Create(args);
    if (!wnd->hwnd) {
        delete wnd;
        return nullptr;
    }
    // Tab-tied notifs must not paint on a different active tab (common when
    // multi-file open/session restore finishes a background load after the UI
    // already selected another tab). Hide if this tab is not current.
    if (args.tab && wnd->delayTimerId == 0) {
        MainWindow* win = FindMainWindowByHwnd(args.hwndParent);
        if (win && win->CurrentTab() != args.tab) {
            ShowWindow(wnd->hwnd, SW_HIDE);
        } else {
            BringWindowToTop(wnd->hwnd);
        }
    } else if (wnd->delayTimerId == 0) {
        BringWindowToTop(wnd->hwnd);
    }
    bool ok = NotifsAdd(wnd, args.groupId);
    if (!ok) {
        delete wnd;
        return nullptr;
    }
    RelayoutNotifications(args.hwndParent);
    return wnd;
}

// show a temporary notification that will go away after a timeout
NotificationWnd* ShowTemporaryNotification(HWND hwnd, Str msg, int timeoutMs) {
    if (timeoutMs <= 0) {
        timeoutMs = kNotifDefaultTimeOut;
    }
    NotificationCreateArgs args;
    args.hwndParent = hwnd;
    args.msg = msg;
    args.timeoutMs = timeoutMs;
    return ShowNotification(args);
}

// show a notification whose content is an arbitrary VirtCtrl tree. Ownership of
// `content` passes to the notification (it is freed with it, also on failure).
// The tree is measured with whatever HWND / HFONT its controls were built with,
// so build it against the parent window
NotificationWnd* ShowCustomNotification(HWND hwndParent, ILayout* content, int timeoutMs) {
    NotificationCreateArgs args;
    args.hwndParent = hwndParent;
    args.groupId = kNotifAdHoc;
    args.content = content;
    args.timeoutMs = timeoutMs;
    return ShowNotification(args);
}

NotificationWnd* ShowWarningNotification(HWND hwndParent, Str msg, int timeoutMs) {
    if (timeoutMs < 0) {
        timeoutMs = kNotifDefaultTimeOut;
    }
    NotificationCreateArgs args;
    args.hwndParent = hwndParent;
    args.msg = msg;
    args.warning = true;
    args.timeoutMs = timeoutMs;
    return ShowNotification(args);
}

void NotificationUpdateMessage(NotificationWnd* wnd, Str msg, int timeoutInMS, bool highlight) {
    if (!wnd) {
        return;
    }
    wnd->UpdateMessage(msg, timeoutInMS, highlight);
}

TempStr NotificationGetMessageTemp(NotificationWnd* wnd) {
    if (!wnd) {
        return {};
    }
    return HwndGetTextTemp(wnd->hwnd);
}

void RemoveNotification(NotificationWnd* wnd) {
    NotifsRemoveNotification(wnd);
}

bool RemoveNotificationsForGroup(HWND hwnd, Kind kind) {
    NotificationWnd* wnds[kMaxNotifs];
    int nWnds = GetForHwnd(hwnd, wnds);
    int n = NotifsRemoveForGroup(wnds, nWnds, kind);
    return n > 0;
}

void RemoveNotificationsForHwnd(HWND hwnd) {
    NotificationWnd* toRemove[kMaxNotifs];
    int nRemove = GetForHwnd(hwnd, toRemove);
    for (int i = 0; i < nRemove; i++) {
        NotifsRemoveNotification(toRemove[i]);
    }
}

NotificationWnd* GetNotificationForGroup(HWND hwnd, Kind kind) {
    NotificationWnd* wnds[kMaxNotifs];
    int nWnds = GetForHwnd(hwnd, wnds);
    return NotifsGetForGroup(wnds, nWnds, kind);
}

void MaybeDelayedWarningNotification(Str msg) {
    log(msg);

    HWND hwnd = GetHwndForNotification();
    if (hwnd) {
        ShowWarningNotification(hwnd, msg, kNotifNoTimeout);
    } else {
        StrNode* node = AllocStrNode(nullptr, msg);
        if (node) {
            ListInsertFront(&gDelayedNotifications, node);
        }
    }
}

void ShowMaybeDelayedNotifications(HWND hwndParent) {
    // reverse so notifications show in the order they were added
    ListReverse(&gDelayedNotifications);
    StrNode* curr = gDelayedNotifications;
    while (curr) {
        ShowWarningNotification(hwndParent, curr->s, kNotifNoTimeout);
        curr = curr->next;
    }
    FreeStrNode(nullptr, gDelayedNotifications);
    gDelayedNotifications = nullptr;
}
