/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "gui/Dpi.h"
#include "base/UITask.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"

#include "Notifications.h"
#include "Theme.h"

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
constexpr int kCloseLeftMargin = 16;
constexpr int kProgressDy = 5;

constexpr UINT_PTR kNotifTimerTimeoutId = 1;
constexpr UINT_PTR kNotifTimerDelayId = 2;

struct NotificationWnd;

// the colors a notification paints with; they depend on `highlight`
struct NotifColors {
    Color bg;
    Color txt;
    Color link;
};

// the message text. A plain message is a single DrawText; one with links, bold
// runs or key-caps becomes a VirtRichText child, which draws and runs its own
// links
struct NotifTextCtrl : VirtCtrl {
    NotificationWnd* notif = nullptr;
    VirtRichText* rich = nullptr; // owned, as our only child
    // gfxText* flags for drawing a plain message, set in NotificationWnd::Layout()
    u32 txtFmt = gfxTextSingleLine;
    Size idealSize;

    NotifTextCtrl();
    ~NotifTextCtrl() override = default;

    Size GetIdealSize() override;
    void SetBounds(Rect) override;
    void Paint(VirtPaintCtx&) override;
};

// the progress bar below the message (only when progressPerc >= 0)
struct NotifProgressCtrl : VirtCtrl {
    NotificationWnd* notif = nullptr;

    NotifProgressCtrl();
    ~NotifProgressCtrl() override = default;

    Size GetIdealSize() override;
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
    void ScheduleRemove(VirtMouseEvent* ev = nullptr);

    int timeoutMs = kNotifDefaultTimeOut; // 0 means no timeout

    bool highlight = false; // TODO: should really be a color
    bool noClose = false;
    // message is shown verbatim, no tip markup parsed (see NotificationCreateArgs)
    bool plainText = false;
    // caller-built message; not owned (it's txtCtrl's child once laid out) but
    // reused across layouts because, unlike `msg`, it can't be re-parsed
    VirtRichText* richMsg = nullptr;

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

    // VBox of (message row, progress); WindowBase::layout owns it
    VBox* container = nullptr;
    // either the built-in text or a caller-supplied tree, never both
    NotifTextCtrl* txtCtrl = nullptr;
    ILayout* contentWnd = nullptr;
    VirtCloseButton* closeCtrl = nullptr;
    NotifProgressCtrl* progressCtrl = nullptr;
    // padding + progress; collapsed when there is no progress so the gap goes too
    ILayout* progressBlock = nullptr;
};

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
    int dyPadding = DpiScale(kPadding);
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
        int xMargin = DpiScale(wnd->xMargin);
        int yMargin = DpiScale(wnd->yMargin);
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

// builds the tree hosted in our HWND: a message row (body | close) and an
// optional progress bar under the body
void NotificationWnd::BuildTree(ILayout* customContent) {
    int padX = DpiScale(12);
    int padY = DpiScale(8);
    int closeDx = DpiScale(16);
    int closeGap = DpiScale(kCloseLeftMargin) - padX;
    closeGap = std::max(closeGap, 0);

    ILayout* body = nullptr;
    if (customContent) {
        contentWnd = customContent;
        body = contentWnd;
    } else {
        txtCtrl = new NotifTextCtrl();
        txtCtrl->notif = this;
        body = txtCtrl;
    }
    if (corner == NotifCorner::BottomBar) {
        auto* al = new Align(body);
        al->HAlign = AlignCenter;
        al->VAlign = AlignCenter;
        body = al;
    }

    progressCtrl = new NotifProgressCtrl();
    progressCtrl->notif = this;
    progressBlock = new Padding(progressCtrl, Insets{padY, 0, 0, 0});
    progressBlock->SetVisibility(Visibility::Collapse);

    auto* left = new VBox();
    left->alignCross = CrossAxisAlign::Stretch;
    left->AddChild(body);
    left->AddChild(progressBlock);

    auto* row = new HBox();
    row->alignCross = CrossAxisAlign::CrossCenter;
    row->rtl = IsUIRtl();
    row->AddChild(left, 1);
    if (!noClose) {
        closeCtrl = new VirtCloseButton();
        closeCtrl->onClick = MkMethod1<NotificationWnd, VirtMouseEvent*, &NotificationWnd::ScheduleRemove>(this);
        closeCtrl->idealSize = {closeDx, closeDx + 2};
        row->AddChild(new Spacer(closeGap, 0));
        row->AddChild(closeCtrl);
    }

    container = new VBox();
    container->alignCross = CrossAxisAlign::Stretch;
    container->AddChild(row);
    layout = new Padding(container, Insets{padY, padX, padY, padX});
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
    plainText = args.plainText;
    // `content` replaces the message entirely, so a richMsg would never be shown
    ReportIf(args.richMsg && args.content);
    richMsg = args.content ? nullptr : args.richMsg;

    onPaint = MkMethod1<NotificationWnd, WindowBase::PaintEvent*, &NotificationWnd::OnPaint>(this);
    onTimer = MkMethod1<NotificationWnd, WindowBase::TimerEvent*, &NotificationWnd::OnTimer>(this);

    CreateCustomArgs cargs;
    cargs.parent = args.hwndParent;
    cargs.font = args.font;
    cargs.exStyle = WS_EX_TOPMOST;
    cargs.style = WS_CHILD | SS_CENTER;
    cargs.title = args.msg;
    if (cargs.font == nullptr) {
        cargs.font = GetAppBiggerFont();
    }
    cargs.pos = Rect(0, 0, 0, 0);
    cargs.visible = args.delayInMs == 0;
    cargs.isRtl = IsUIRtl();

    CreateCustom(cargs);
    if (!hwnd) {
        // we took ownership of the content, so it dies with us
        delete args.content;
        delete args.richMsg;
        richMsg = nullptr;
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

void NotificationWnd::Layout(Str message) {
    if (!message) {
        message = StrL("");
    }
    int padX = DpiScale(12);
    int closeDx = DpiScale(16);
    int closeLeftMargin = DpiScale(kCloseLeftMargin) - padX;

    // limit the width to the parent window so that the close button
    // stays reachable even for very long messages (issue #2916)
    int topLeftMargin = DpiScale(kTopLeftMargin);
    Rect rParent = HwndClientRect(GetParent(hwnd));
    int maxTextDx = rParent.dx - (2 * topLeftMargin) - (2 * padX);
    if (!noClose) {
        maxTextDx -= closeLeftMargin + closeDx + padX;
    }

    bool isRtl = IsUIRtl();

    Size szText;
    if (!contentWnd) {
        if (txtCtrl->rich) {
            // a caller-built richMsg is reused: unlike `message`, it can't be
            // rebuilt here, so don't let RemoveChild() delete it
            txtCtrl->RemoveChild(txtCtrl->rich, txtCtrl->rich != richMsg);
            txtCtrl->rich = nullptr;
        }
        // parse the message for the extended tip syntax (links, Key/ shortcuts).
        // Not for a plainText message: it embeds text from outside the app,
        // which must not be able to inject a command link (GHSA-2wv2-qm2f-vmxh)
        VirtRichText* parsed = richMsg;
        if (!parsed && !plainText) {
            parsed = ParseTip(message);
        }
        // rich path also for bold-only messages (e.g. the F7 help), not just links
        bool drawRich = parsed && (parsed == richMsg || parsed->HasRichContent());

        if (drawRich) {
            // rich text: the words lay themselves out (links included). Note: no
            // RTL word reordering, same as the home page tips.
            txtCtrl->txtFmt = gfxTextLeft;
            parsed->font = font;
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
            if (parsed && (str::Contains(message, StrL("(Key/")) || str::Contains(message, StrL("(Kbd/")))) {
                message = parsed->PlainTextTemp();
                HwndSetText(hwnd, message);
            }
            uint fmt = DT_SINGLELINE | DT_NOPREFIX;
            u32 gfxFmt = gfxTextSingleLine;
            if (isRtl) {
                fmt |= DT_RIGHT | DT_RTLREADING;
                gfxFmt |= gfxTextRight | gfxTextRtl;
            }
            HDC hdc = GetDC(hwnd);
            szText = HdcMeasureText(hdc, message, 4096, fmt | DT_NOCLIP, GetHFont());
            if (maxTextDx > 0 && szText.dx > maxTextDx) {
                // too wide: word-wrap the message; DT_WORD_ELLIPSIS truncates
                // words too long to wrap (e.g. long file paths)
                fmt = DT_WORDBREAK | DT_WORD_ELLIPSIS | DT_NOPREFIX;
                gfxFmt = gfxTextWrap | gfxTextEllipsis;
                szText = HdcMeasureText(hdc, message, maxTextDx, fmt, GetHFont());
                szText.dx = std::min(szText.dx, maxTextDx);
            }
            ReleaseDC(hwnd, hdc);
            txtCtrl->txtFmt = gfxFmt;
            delete parsed;
        }
        txtCtrl->idealSize = szText;
    }

    if (closeCtrl) {
        closeCtrl->idealSize = {closeDx, closeDx + 2};
    }
    if (progressBlock) {
        progressBlock->SetVisibility(HasProgress() ? Visibility::Visible : Visibility::Collapse);
    }

    int maxDx = rParent.dx - (2 * topLeftMargin);
    Rect rCurr = HwndClientRect(hwnd);
    // measure at natural size first; a bounded max on a Stretch VBox would
    // expand the toast to the canvas width
    Size sz = layout->Layout(ExpandInf());
    if (corner == NotifCorner::BottomBar && rParent.dx > 0) {
        sz.dx = rParent.dx;
    } else {
        if (rCurr.dx > sz.dx) {
            sz.dx = rCurr.dx; // don't shrink when the text gets shorter
        }
        if (maxDx > 0 && sz.dx > maxDx) {
            sz.dx = maxDx;
        }
    }
    LayoutToSize(layout, sz);
    RefreshVirtTops(hwnd, layout, {0, 0, sz.dx, sz.dy}, &vroot);

    Rect rWin = HwndWindowRect(hwnd);
    if (sz.dx == rWin.dx && sz.dy == rWin.dy) {
        return;
    }

    uint flags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE;
    SetWindowPos(hwnd, nullptr, 0, 0, sz.dx, sz.dy, flags);

    // move the window to the right for a right-to-left layout
    if (IsUIRtl()) {
        HWND parent = GetParent(hwnd);
        Rect r = HwndMapRectToWindow(HwndWindowRect(hwnd), HWND_DESKTOP, parent);
        int cxVScroll = DpiGetSystemMetrics(SM_CXVSCROLL);
        r.x = HwndWindowRect(parent).dx - r.dx - DpiScale(kTopLeftMargin) - cxVScroll;
        flags = SWP_NOSIZE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOACTIVATE | SWP_DEFERERASE;
        SetWindowPos(hwnd, nullptr, r.x, r.y, 0, 0, flags);
    }
}

void NotificationWnd::OnPaint(WindowBase::PaintEvent* ev) {
    Rect rc = HwndClientRect(hwnd);

    NotifColors cols = Colors();
    Gfx* gfx = GfxCreateWithDoubleBuffer(this, ev->hdc);
    gfx->FillRect(rc, cols.bg);
    if (vroot) {
        vroot->Paint(gfx, rc);
    }
    delete gfx;
}

//--- the VirtCtrl controls making up a notification

NotifTextCtrl::NotifTextCtrl() {
    kind = kindNotifText;
}

Size NotifTextCtrl::GetIdealSize() {
    return idealSize;
}

void NotifTextCtrl::SetBounds(Rect r) {
    VirtCtrl::SetBounds(r);
    if (rich) {
        // same window rect as us so the words sit at {0,0} in our content
        rich->SetBounds(r);
    }
}

void NotifTextCtrl::Paint(VirtPaintCtx& ctx) {
    if (rich) {
        // the child paints itself; keep its colors in step with the theme
        NotifColors cols = notif->Colors();
        rich->SetColor(kColRichText, cols.txt);
        rich->SetColor(kColRichLink, cols.link);
        rich->SetColor(kColRichBg, cols.bg);
        return;
    }
    TempStr text = HwndGetTextTemp(notif->hwnd);
    NotifColors cols = notif->Colors();
    ctx.gfx->DrawText(text, ctx.content, txtFmt, notif->font, cols.txt);
}

NotifProgressCtrl::NotifProgressCtrl() {
    kind = kindNotifProgress;
    flags |= vwfNoHitTest;
}

Size NotifProgressCtrl::GetIdealSize() {
    int h = notif && notif->hwnd ? DpiScale(kProgressDy) : kProgressDy;
    return {0, h};
}

void NotifProgressCtrl::Paint(VirtPaintCtx& ctx) {
    Rect rc = ctx.bounds;
    int progressWidth = rc.dx;

    Color col = ThemeNotificationsProgressColor();
    ctx.gfx->DrawRect(rc, col);

    rc.x += 2;
    rc.dx = (progressWidth - 3) * notif->progressPerc / 100;
    rc.y += 2;
    rc.dy -= 3;
    ctx.gfx->FillRect(rc, col);
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
void NotificationWnd::ScheduleRemove(VirtMouseEvent*) {
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
// The tree is measured with the same PlatformFonts its controls were built with,
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

// for a message that embeds text we didn't author (a file path, document
// metadata, a server response): shown verbatim, so it can't smuggle in a
// clickable command link
NotificationWnd* ShowPlainNotification(HWND hwnd, Str msg, int timeoutMs) {
    if (timeoutMs <= 0) {
        timeoutMs = kNotifDefaultTimeOut;
    }
    NotificationCreateArgs args;
    args.hwndParent = hwnd;
    args.msg = msg;
    args.plainText = true;
    args.timeoutMs = timeoutMs;
    return ShowNotification(args);
}

NotificationWnd* ShowPlainWarningNotification(HWND hwndParent, Str msg, int timeoutMs) {
    if (timeoutMs < 0) {
        timeoutMs = kNotifDefaultTimeOut;
    }
    NotificationCreateArgs args;
    args.hwndParent = hwndParent;
    args.msg = msg;
    args.plainText = true;
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
