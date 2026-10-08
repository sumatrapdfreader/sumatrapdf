/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "gui/GpuiBridge.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "Theme.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "SumatraPDF.h"
#include "gui/AppShell.h"
#include "gui/DocCanvas.h"
#include "gui/DialogWidgets.h"
#include "HomePage.h"
#include "OverlayScrollbar.h"

bool gOverlayScrollbarSuppressThick = false;

// all live overlay scrollbars
static Vec<OverlayScrollbar*> gAllScrollbars;
// ng: the bar that has the mouse (orig's SetCapture) for a thumb drag or an
// auto-repeat
static OverlayScrollbar* gCapture = nullptr;

static constexpr int kMinThumbSize = 20;
static constexpr u8 kAlphaThin = 180;
static constexpr u8 kAlphaThick = 220;

using State = OverlayScrollbar::State;

// ng: win32's MulDiv (a * b / c, rounded) for a positive c; the other
// platforms have none
static int MulDivI(int a, int b, int c) {
    if (c <= 0) {
        return -1;
    }
    i64 n = (i64)a * (i64)b;
    n = n >= 0 ? n + (c / 2) : n - (c / 2);
    return (int)(n / c);
}

static bool IsLive(OverlayScrollbar* sb) {
    if (!sb) {
        return false;
    }
    for (OverlayScrollbar* o : gAllScrollbars) {
        if (o == sb) {
            return true;
        }
    }
    return false;
}

static bool IsThick(OverlayScrollbar* sb) {
    return sb->state == State::SmartThick || sb->state == State::AlwaysThick;
}

static bool IsVisible(OverlayScrollbar* sb) {
    return sb->state == State::SmartThin || sb->state == State::SmartThick || sb->state == State::AlwaysThick;
}

// scrollbar is active: shown or auto-hidden but ready to appear
static bool IsActive(OverlayScrollbar* sb) {
    return sb->state != State::Hidden;
}

// ng: the modes that never hide and never go thin
static bool IsAlwaysThickMode(OverlayScrollbar* sb) {
    return sb->mode == OverlayScrollbar::Mode::Thick || sb->mode == OverlayScrollbar::Mode::Windows;
}

static int ScaledWidth(OverlayScrollbar* sb, bool thick) {
    return thick ? sb->thickWidth : sb->thinWidth;
}

static bool IsVert(OverlayScrollbar* sb) {
    return sb->type == OverlayScrollbar::Type::Vert;
}

bool IsOverlayScrollbarThick(OverlayScrollbar* sb) {
    return sb && IsThick(sb);
}

// returns true if scrollbar is visible (thin, thick, or always thick)
bool IsOverlayScrollbarVisible(OverlayScrollbar* sb) {
    return sb && IsVisible(sb);
}

int OverlayScrollbarWidth(OverlayScrollbar* sb) {
    return ScaledWidth(sb, IsThick(sb));
}

static Rect ClientRect(OverlayScrollbar* sb) {
    return Rect(0, 0, sb->clientDx, sb->clientDy);
}

// Get the track rect in client coords of the scrollbar
static Rect GetTrackRect(OverlayScrollbar* sb) {
    Rect rc = ClientRect(sb);
    int arrowSize = 0;
    int gap = 0;
    if (IsThick(sb)) {
        arrowSize = IsVert(sb) ? rc.dx : rc.dy;
        gap = DpiScale(2);
    }
    int total = arrowSize + gap;
    if (IsVert(sb)) {
        return {0, total, rc.dx, rc.dy - (2 * total)};
    }
    return {total, 0, rc.dx - (2 * total), rc.dy};
}

static int ThumbLen(OverlayScrollbar* sb, int trackLen) {
    int range = sb->nMax - sb->nMin + 1;
    int thumbLen = MulDivI(trackLen, sb->nPage, std::max(range, 1));
    return std::max(thumbLen, DpiScale(kMinThumbSize));
}

// Calculate thumb rect within the track
static Rect GetThumbRect(OverlayScrollbar* sb) {
    Rect track = GetTrackRect(sb);
    int range = sb->nMax - sb->nMin + 1;
    if (range <= 0 || sb->nPage >= range) {
        return track;
    }

    int trackLen = IsVert(sb) ? track.dy : track.dx;
    int thumbLen = ThumbLen(sb, trackLen);

    int scrollableTrack = trackLen - thumbLen;
    int scrollableRange = range - sb->nPage;
    int pos = sb->isDragging ? sb->nTrackPos : sb->nPos;
    int thumbOffset = 0;
    if (scrollableRange > 0) {
        thumbOffset = MulDivI(pos - sb->nMin, scrollableTrack, scrollableRange);
    }
    thumbOffset = limitValue(thumbOffset, 0, std::max(0, scrollableTrack));

    if (IsVert(sb)) {
        return {track.x, track.y + thumbOffset, track.dx, thumbLen};
    }
    return {track.x + thumbOffset, track.y, thumbLen, track.dy};
}

static Rect GetArrowTopRect(OverlayScrollbar* sb) {
    Rect rc = ClientRect(sb);
    int arrowSize = IsVert(sb) ? rc.dx : rc.dy;
    if (IsVert(sb)) {
        return {0, 0, rc.dx, arrowSize};
    }
    return {0, 0, arrowSize, rc.dy};
}

static Rect GetArrowBottomRect(OverlayScrollbar* sb) {
    Rect rc = ClientRect(sb);
    int arrowSize = IsVert(sb) ? rc.dx : rc.dy;
    if (IsVert(sb)) {
        return {0, rc.dy - arrowSize, rc.dx, arrowSize};
    }
    return {rc.dx - arrowSize, 0, arrowSize, rc.dy};
}

static void SendScrollMsg(OverlayScrollbar* sb, ScrollMsg msg) {
    if (sb->onScroll) {
        sb->onScroll(sb->win, sb, msg);
    }
}

// ng: orig repaints its layered window; here the owner's frame is redrawn
static void PaintScrollbar(OverlayScrollbar* sb) {
    if (sb->win) {
        AppShellInvalidate(sb->win);
    }
}

static void SetState(OverlayScrollbar* sb, State newState) {
    if (sb->state == newState) {
        return;
    }
    sb->state = newState;
    if (!IsVisible(sb)) {
        sb->mouseOverThumb = false;
    }
    sb->hideLeftMs = (newState == State::SmartThin) ? sb->showAfterScrollMs : 0;
    PaintScrollbar(sb);
}

static void ShowScrollbarWindow(OverlayScrollbar* sb, bool thick) {
    // Don't revert to thin while user is dragging the thumb
    if (sb->isDragging && !thick) {
        return;
    }
    if (IsAlwaysThickMode(sb)) {
        SetState(sb, State::AlwaysThick);
    } else {
        SetState(sb, thick ? State::SmartThick : State::SmartThin);
    }
}

static void HideScrollbarWindow(OverlayScrollbar* sb) {
    // Don't hide while user is dragging the thumb
    if (sb->isDragging) {
        return;
    }
    if (IsAlwaysThickMode(sb)) {
        return; // never hide in Thick mode
    }
    SetState(sb, State::SmartInvisible);
}

void OverlayScrollbarHide(OverlayScrollbar* sb) {
    HideScrollbarWindow(sb);
}

// Restart the thin-bar auto-hide countdown (showAfterScrollMs). SetState only
// arms the timer on a state transition, so continuous scroll while already
// SmartThin would otherwise let the earlier mouse-stop / first-reveal timer
// fire and hide the bar mid-scroll.
static void RestartSmartThinAutoHide(OverlayScrollbar* sb) {
    if (sb->state != State::SmartThin) {
        return;
    }
    sb->hideLeftMs = sb->showAfterScrollMs;
}

OverlayScrollbar* OverlayScrollbarCreate(MainWindow* win, OverlayScrollbar::Type type, OverlayScrollbar::Mode mode,
                                         OverlayScrollbarScrollFn onScroll) {
    auto* sb = new OverlayScrollbar();
    sb->win = win;
    sb->onScroll = onScroll;
    sb->type = type;
    sb->mode = mode;
    sb->thinWidth = DpiScale(4);
    sb->thickWidth = DpiScale(16);
    int sysWidth = DpiGetSystemMetrics(IsVert(sb) ? SM_CXVSCROLL : SM_CYHSCROLL);
    if (sysWidth > 0) {
        sb->thickWidth = sysWidth;
    }
    sb->state = IsAlwaysThickMode(sb) ? State::AlwaysThick : State::SmartThin;
    sb->hideLeftMs = (sb->state == State::SmartThin) ? sb->showAfterScrollMs : 0;
    VecAppend(gAllScrollbars, sb);
    return sb;
}

void OverlayScrollbarDestroy(OverlayScrollbar* sb) {
    if (!sb) {
        return;
    }
    VecRemove(gAllScrollbars, sb);
    if (gCapture == sb) {
        gCapture = nullptr;
    }
    delete sb;
}

// Change the scrollbar mode (Smart vs Thick)
void OverlayScrollbarSetMode(OverlayScrollbar* sb, OverlayScrollbar::Mode mode) {
    if (!sb || sb->mode == mode) {
        return;
    }
    sb->mode = mode;
    if (!IsActive(sb)) {
        return;
    }
    // transition to the appropriate state for the new mode
    if (IsAlwaysThickMode(sb)) {
        SetState(sb, State::AlwaysThick);
    } else {
        // Smart mode: start as thin, will auto-hide
        SetState(sb, State::SmartThin);
    }
}

void OverlayScrollbarSetInfo(OverlayScrollbar* sb, int nMin, int nMax, int nPage, int nPos) {
    if (!sb) {
        return;
    }
    sb->nMin = nMin;
    sb->nMax = nMax;
    sb->nPage = nPage;
    sb->nPos = nPos;
}

// Show the thin smart overlay after scroll activity (mouse wheel, keys, etc.).
// Unlike mouse-move tracking, this does not require cursor motion (#5859).
void OverlayScrollbarNotifyScroll(OverlayScrollbar* sb) {
    if (!sb || !IsActive(sb) || sb->isDragging) {
        return;
    }
    if (IsAlwaysThickMode(sb)) {
        return;
    }
    // Leave thick-from-proximity alone; only (re)show the thin indicator.
    if (IsThick(sb)) {
        return;
    }
    if (sb->state != State::SmartThin) {
        ShowScrollbarWindow(sb, false);
    } else {
        RestartSmartThinAutoHide(sb);
    }
}

// orig's MouseTrackTimerProc, per bar: a 50 ms poll of the cursor. Here the
// owner's mouse move is the poll, so `mouseMoved` is always true when it is
// the mouse that called.
void OverlayScrollbarOnMouse(OverlayScrollbar* sb, bool overOwner, bool overScrollbar, bool mouseMoved) {
    if (!sb || !IsActive(sb)) {
        return;
    }
    if (sb->isDragging) {
        // Don't change state while dragging
        return;
    }
    if (gOverlayScrollbarSuppressThick) {
        return;
    }
    if (overScrollbar) {
        // Mouse is over the scrollbar area - show thick
        if (!IsThick(sb)) {
            ShowScrollbarWindow(sb, true);
        }
        return;
    }
    if (overOwner && mouseMoved) {
        // Mouse is over owner and moving, but not over the scrollbar - show thin
        // IsThick() means transitioning from thick to thin
        if (IsThick(sb) || sb->state != State::SmartThin) {
            ShowScrollbarWindow(sb, false);
        }
        // Reset the auto-hide timer since mouse is moving
        if (sb->state == State::SmartThin) {
            sb->hideLeftMs = sb->hideAfterMouseStopMs;
        }
        return;
    }
    if (IsThick(sb) && !overOwner) {
        // Mouse left the owner area while thick - transition to hidden
        HideScrollbarWindow(sb);
    }
}

// orig's SPI_GETKEYBOARDDELAY / SPI_GETKEYBOARDSPEED timing for the repeat
static int RepeatDelayMs(bool initial) {
#if OS_WIN
    if (initial) {
        UINT delayMs = 0;
        SystemParametersInfoW(SPI_GETKEYBOARDDELAY, 0, &delayMs, 0);
        return 250 + ((int)delayMs * 250); // 0-3 maps to 250-1000ms
    }
    UINT repeatMs = 0;
    SystemParametersInfoW(SPI_GETKEYBOARDSPEED, 0, &repeatMs, 0);
    // SPI_GETKEYBOARDSPEED returns 0-31, map to ~33-500ms (same as OS key repeat)
    return 400 - ((int)repeatMs * 12);
#else
    return initial ? 500 : 28;
#endif
}

static void StartRepeat(OverlayScrollbar* sb, ScrollMsg code) {
    sb->repeatScrollCode = code;
    sb->repeatIsInitial = true;
    sb->repeatLeftMs = RepeatDelayMs(true);
    gCapture = sb;
}

static void StopRepeat(OverlayScrollbar* sb) {
    sb->repeatScrollCode = ScrollMsg::None;
    sb->repeatLeftMs = 0;
}

// ng: a window's own scrollbar stops paging once the thumb is under the
// cursor; orig's overlay bar pages for as long as the button is down
static bool ThumbReachedClick(OverlayScrollbar* sb) {
    if (sb->mode != OverlayScrollbar::Mode::Windows) {
        return false;
    }
    Rect thumbRc = GetThumbRect(sb);
    int start = IsVert(sb) ? thumbRc.y : thumbRc.x;
    int end = start + (IsVert(sb) ? thumbRc.dy : thumbRc.dx);
    switch (sb->repeatScrollCode) {
        case ScrollMsg::PageUp:
        case ScrollMsg::PageLeft:
            return sb->repeatClickPos >= start;
        case ScrollMsg::PageDown:
        case ScrollMsg::PageRight:
            return sb->repeatClickPos < end;
        default:
            return false;
    }
}

bool OverlayScrollbarTick(OverlayScrollbar* sb, int elapsedMs, bool ownerActive) {
    if (!sb) {
        return false;
    }
    State was = sb->state;
    // orig's kTimerRepeatScroll
    if (sb->repeatScrollCode != ScrollMsg::None) {
        sb->repeatLeftMs -= elapsedMs;
        if (sb->repeatLeftMs <= 0) {
            if (!ThumbReachedClick(sb)) {
                SendScrollMsg(sb, sb->repeatScrollCode);
            }
            // switch from initial delay to repeat rate
            sb->repeatIsInitial = false;
            sb->repeatLeftMs = RepeatDelayMs(false);
        }
    }
    // orig's poll: only scrollbars whose owner is in the active window show
    if (!ownerActive && IsActive(sb) && IsVisible(sb) && !sb->isDragging) {
        HideScrollbarWindow(sb);
    }
    // orig's kTimerAutoHide
    if (sb->hideLeftMs > 0) {
        sb->hideLeftMs -= elapsedMs;
        if (sb->hideLeftMs <= 0) {
            sb->hideLeftMs = 0;
            if (!sb->isDragging) {
                HideScrollbarWindow(sb);
            }
        }
    }
    return was != sb->state;
}

// Derive scrollbar colors from current theme
static Color ThemeTrackColor() {
    Color bg = ThemeControlBackgroundColor();
    return bg;
}

static Color ThemeThumbColor() {
    Color bg = ThemeControlBackgroundColor();
    return AccentColor(bg, 100);
}

static Color ThemeThumbHoverColor() {
    Color bg = ThemeControlBackgroundColor();
    return AccentColor(bg, 140);
}

static gp::Rgba WithAlpha(Color c, u8 a) {
    gp::Rgba r = ToGpui(c);
    r.a = a;
    return r;
}

// ng: orig fills a GDI+ polygon (the "thick arrows", like Windows Terminal);
// the canvas has no polygon, so the triangle is filled a pixel row at a time.
// (tipX, tipY) is the tip; the base is `half` to each side, `depth` away
// along the y axis (`alongY`) or the x axis, in direction `dir`.
static void FillTriangle(gp::PaintCtx* ctx, gp::Bounds b, float k, float tipX, float tipY, float half, float depth,
                         bool alongY, int dir, gp::Rgba col) {
    int n = (int)(depth + 0.5f);
    for (int i = 0; i < n; i++) {
        float w = half * ((float)i + 0.5f) / (float)n;
        float tip = alongY ? tipY : tipX;
        float along = (dir > 0) ? (tip + (float)i) : (tip - (float)i - 1);
        if (alongY) {
            gp::CanvasFillRect(ctx, b.x + (tipX - w) * k, b.y + along * k, 2 * w * k, k, col);
        } else {
            gp::CanvasFillRect(ctx, b.x + along * k, b.y + (tipY - w) * k, k, 2 * w * k, col);
        }
    }
}

// orig's PaintScrollbar
static void OnPaintScrollbar(gp::PaintCtx* ctx, gp::El* e, void* user) {
    auto* sb = (OverlayScrollbar*)user;
    if (!IsLive(sb) || !IsVisible(sb)) {
        return;
    }
    gp::Bounds b = e->Bounds();
    float k = sb->k;
    bool thick = IsThick(sb);
    u8 alpha = kAlphaThin;
    if (thick) {
        // non-default themes define exact colors, so draw thick scrollbar fully opaque
        alpha = IsCurrentThemeDefault() ? kAlphaThick : (u8)255;
    }
    if (sb->mode == OverlayScrollbar::Mode::Windows) {
        // ng: a window's own scrollbar isn't see-through
        alpha = 255;
    }

    auto fillRect = [&](Rect r, Color color) {
        gp::CanvasFillRect(ctx, b.x + (float)r.x * k, b.y + (float)r.y * k, (float)r.dx * k, (float)r.dy * k,
                           WithAlpha(color, alpha));
    };

    if (thick) {
        fillRect(ClientRect(sb), ThemeTrackColor());
    }

    Rect thumbRc = GetThumbRect(sb);
    Color thumbCol = sb->mouseOverThumb ? ThemeThumbHoverColor() : ThemeThumbColor();
    fillRect(thumbRc, thumbCol);

    if (!thick) {
        return;
    }
    gp::Rgba arrowCol = WithAlpha(ThemeThumbHoverColor(), alpha);
    Rect arrowTop = GetArrowTopRect(sb);
    Rect arrowBot = GetArrowBottomRect(sb);
    // filled triangles (like Windows Terminal)
    if (IsVert(sb)) {
        float sz = (float)arrowTop.dx / 3.0f;
        // up triangle
        float cx = (float)(arrowTop.x + (arrowTop.dx / 2));
        float cy = (float)(arrowTop.y + (arrowTop.dy / 2));
        FillTriangle(ctx, b, k, cx, cy - (sz * 0.7f), sz, sz * 1.4f, true, 1, arrowCol);
        // down triangle
        cx = (float)(arrowBot.x + (arrowBot.dx / 2));
        cy = (float)(arrowBot.y + (arrowBot.dy / 2));
        FillTriangle(ctx, b, k, cx, cy + (sz * 0.7f), sz, sz * 1.4f, true, -1, arrowCol);
        return;
    }
    float sz = (float)arrowTop.dy / 3.0f;
    // left triangle
    float cx = (float)(arrowTop.x + (arrowTop.dx / 2));
    float cy = (float)(arrowTop.y + (arrowTop.dy / 2));
    FillTriangle(ctx, b, k, cx - (sz * 0.7f), cy, sz, sz * 1.4f, false, 1, arrowCol);
    // right triangle
    cx = (float)(arrowBot.x + (arrowBot.dx / 2));
    cy = (float)(arrowBot.y + (arrowBot.dy / 2));
    FillTriangle(ctx, b, k, cx + (sz * 0.7f), cy, sz, sz * 1.4f, false, -1, arrowCol);
}

// --- mouse (orig's WndProcOverlayScrollbar) ----------------------------------

// the position that puts the thumb's center at `clickPos` (client coords)
static int PosForThumbCenteredAt(OverlayScrollbar* sb, int clickPos) {
    Rect track = GetTrackRect(sb);
    int range = sb->nMax - sb->nMin + 1;
    int trackLen = IsVert(sb) ? track.dy : track.dx;
    int thumbLen = ThumbLen(sb, trackLen);
    int scrollableTrack = trackLen - thumbLen;
    int scrollableRange = range - sb->nPage;
    int clickInTrack = clickPos - (IsVert(sb) ? track.y : track.x);
    int thumbOffset = clickInTrack - (thumbLen / 2);
    thumbOffset = limitValue(thumbOffset, 0, std::max(0, scrollableTrack));
    int newPos = sb->nMin;
    if (scrollableTrack > 0 && scrollableRange > 0) {
        newPos = sb->nMin + MulDivI(thumbOffset, scrollableRange, scrollableTrack);
    }
    return newPos;
}

static void OnLeftButtonDown(OverlayScrollbar* sb, int mx, int my, bool isShift) {
    Point pt(mx, my);
    if (IsThick(sb)) {
        Rect arrowTop = GetArrowTopRect(sb);
        Rect arrowBot = GetArrowBottomRect(sb);

        if (arrowTop.Contains(pt)) {
            ScrollMsg code = IsVert(sb) ? ScrollMsg::LineUp : ScrollMsg::LineLeft;
            SendScrollMsg(sb, code);
            StartRepeat(sb, code);
            return;
        }
        if (arrowBot.Contains(pt)) {
            ScrollMsg code = IsVert(sb) ? ScrollMsg::LineDown : ScrollMsg::LineRight;
            SendScrollMsg(sb, code);
            StartRepeat(sb, code);
            return;
        }
    }

    // Shift+click: jump thumb center to click position
    if (isShift) {
        sb->nTrackPos = PosForThumbCenteredAt(sb, IsVert(sb) ? my : mx);
        PaintScrollbar(sb);
        SendScrollMsg(sb, ScrollMsg::ThumbTrack);
        return;
    }

    Rect thumbRc = GetThumbRect(sb);
    if (thumbRc.Contains(pt)) {
        sb->isDragging = true;
        sb->dragStartY = IsVert(sb) ? my : mx;
        sb->dragStartPos = sb->nPos;
        sb->nTrackPos = sb->nPos;
        gCapture = sb;
        return;
    }

    Rect track = GetTrackRect(sb);
    if (track.Contains(pt)) {
        int clickPos = IsVert(sb) ? my : mx;
        int thumbMid = IsVert(sb) ? (thumbRc.y + (thumbRc.dy / 2)) : (thumbRc.x + (thumbRc.dx / 2));
        ScrollMsg code;
        if (clickPos < thumbMid) {
            code = IsVert(sb) ? ScrollMsg::PageUp : ScrollMsg::PageLeft;
        } else {
            code = IsVert(sb) ? ScrollMsg::PageDown : ScrollMsg::PageRight;
        }
        SendScrollMsg(sb, code);
        StartRepeat(sb, code);
        sb->repeatClickPos = clickPos;
    }
}

// `ptInTrack` is along the bar's axis, in the units and origin of dragStartY
static void OnDragMove(OverlayScrollbar* sb, int ptInTrack) {
    Rect track = GetTrackRect(sb);
    int range = sb->nMax - sb->nMin + 1;
    int trackLen = IsVert(sb) ? track.dy : track.dx;
    int thumbLen = ThumbLen(sb, trackLen);
    int scrollableTrack = trackLen - thumbLen;
    int scrollableRange = range - sb->nPage;

    int dragDelta = ptInTrack - sb->dragStartY;
    int newPos = sb->dragStartPos;
    if (scrollableTrack > 0 && scrollableRange > 0) {
        newPos = sb->dragStartPos + MulDivI(dragDelta, scrollableRange, scrollableTrack);
    }
    newPos = limitValue(newPos, sb->nMin, std::max(sb->nMin, sb->nMax - sb->nPage + 1));
    sb->nTrackPos = newPos;
    PaintScrollbar(sb);
    SendScrollMsg(sb, ScrollMsg::ThumbTrack);
}

static void OnLeftButtonUp(OverlayScrollbar* sb) {
    if (sb->repeatScrollCode != ScrollMsg::None) {
        StopRepeat(sb);
    }
    if (sb->isDragging) {
        sb->isDragging = false;
        sb->nPos = sb->nTrackPos;
        PaintScrollbar(sb);
    }
    if (gCapture == sb) {
        gCapture = nullptr;
    }
}

// ng: the "windows" mode's right-click menu is the one win32 gives a window's
// own scrollbars; an item's arg is the ScrollMsg, kScrollHere the click position
constexpr int kScrollHere = -1;
// the bar whose menu is open
static OverlayScrollbar* gMenuScrollbar = nullptr;

struct ScrollbarView {
    static void OnDown(ScrollbarView*, gp::Ctx* cx, const gp::MouseDownEvent* ev, int64_t arg);
    static void OnUp(ScrollbarView*, gp::Ctx* cx, const gp::MouseUpEvent* ev, int64_t arg);
    static void OnMove(ScrollbarView*, gp::Ctx* cx, const gp::MouseMoveEvent* ev, int64_t arg);
    static void OnHover(ScrollbarView*, gp::Ctx* cx, const gp::HoverEvent* ev, int64_t arg);
    static void OnMenu(ScrollbarView*, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t arg);
    static void OnWinMove(ScrollbarView*, gp::Ctx* cx, const gp::MouseMoveEvent* ev);
    static void OnWinUp(ScrollbarView*, gp::Ctx* cx, const gp::MouseUpEvent* ev);
    static void OnWinExit(ScrollbarView*, gp::Ctx* cx, const gp::MouseExitEvent* ev);
};

static gp::Entity<ScrollbarView> gView;

static gp::Entity<ScrollbarView> View(gp::App* app) {
    if (!gView.IsValid() || !gView.Get(app)) {
        gView = gp::EntityNewState<ScrollbarView>(app);
    }
    return gView;
}

static OverlayScrollbar* FromArg(int64_t arg) {
    auto* sb = (OverlayScrollbar*)(intptr_t)arg;
    return IsLive(sb) ? sb : nullptr;
}

void ScrollbarView::OnDown(ScrollbarView*, gp::Ctx* cx, const gp::MouseDownEvent* ev, int64_t arg) {
    OverlayScrollbar* sb = FromArg(arg);
    if (!sb) {
        return;
    }
    // the press is the bar's: not the canvas', not gpui's ContextMenu's
    gp::WindowStopPropagation(cx);
    if (IsContextClick(ev->button, ev->modifiers) || ev->button != gp::MouseButton::Left) {
        return;
    }
    int mx = (int)((ev->x - ev->el.x) / sb->k);
    int my = (int)((ev->y - ev->el.y) / sb->k);
    OnLeftButtonDown(sb, mx, my, ev->modifiers.shift);
    if (sb->isDragging) {
        // ng: the drag is followed in window coordinates
        sb->dragStartY = (int)((IsVert(sb) ? ev->y : ev->x) / sb->k);
    }
    PaintScrollbar(sb);
}

void ScrollbarView::OnUp(ScrollbarView*, gp::Ctx* cx, const gp::MouseUpEvent* ev, int64_t arg) {
    OverlayScrollbar* sb = FromArg(arg);
    if (!sb || !IsContextClick(ev->button, ev->modifiers) || sb->mode != OverlayScrollbar::Mode::Windows) {
        return;
    }
    gp::Entity<gp::PopupMenuState> popup;
    popup.id.index = sb->popupIdx;
    popup.id.gen = sb->popupGen;
    if (!popup.IsValid()) {
        return;
    }
    float x = ev->x - ev->el.x;
    float y = ev->y - ev->el.y;
    sb->menuPos = (int)((IsVert(sb) ? y : x) / sb->k);
    gMenuScrollbar = sb;
    OpenPopupMenuAt(cx, popup, x, y);
    PaintScrollbar(sb);
}

void ScrollbarView::OnMove(ScrollbarView*, gp::Ctx*, const gp::MouseMoveEvent* ev, int64_t arg) {
    OverlayScrollbar* sb = FromArg(arg);
    if (!sb || sb->isDragging) {
        return;
    }
    // orig's poll: the cursor is over the scrollbar area
    OverlayScrollbarOnMouse(sb, true, true, true);
    if (!IsThick(sb)) {
        return;
    }
    int mx = (int)((ev->x - ev->el.x) / sb->k);
    int my = (int)((ev->y - ev->el.y) / sb->k);
    Rect thumbRc = GetThumbRect(sb);
    bool wasOver = sb->mouseOverThumb;
    sb->mouseOverThumb = thumbRc.Contains(Point(mx, my));
    if (wasOver != sb->mouseOverThumb) {
        PaintScrollbar(sb);
    }
}

void ScrollbarView::OnHover(ScrollbarView*, gp::Ctx*, const gp::HoverEvent* ev, int64_t arg) {
    OverlayScrollbar* sb = FromArg(arg);
    if (!sb || ev->hovered || !sb->mouseOverThumb) {
        return;
    }
    sb->mouseOverThumb = false;
    PaintScrollbar(sb);
}

// orig's MouseTrackTimerProc polls the cursor for every bar. The document
// canvas reports its own mouse moves; the start page is many elements, so its
// window's moves are looked at here. (x, y) are in dips.
static void TrackStartPageMouse(gp::Window* gw, float x, float y, bool inWindow) {
    for (OverlayScrollbar* sb : gAllScrollbars) {
        MainWindow* win = sb->win;
        if (!win || win->gpuiWin != gw) {
            continue;
        }
        if (!IsMainWindowValid(win) || !win->IsCurrentTabAbout()) {
            return;
        }
        Rect rc = win->canvasRc;
        bool overOwner =
            inWindow && x >= (float)rc.x && y >= (float)rc.y && x < (float)(rc.x + rc.dx) && y < (float)(rc.y + rc.dy);
        float k = CanvasScale(win);
        OverlayScrollbarsOnMouse(win, (int)((x - (float)rc.x) / k), (int)((y - (float)rc.y) / k), overOwner);
        return;
    }
}

void ScrollbarView::OnWinExit(ScrollbarView*, gp::Ctx* cx, const gp::MouseExitEvent*) {
    TrackStartPageMouse(cx->win, 0, 0, false);
}

void OverlayScrollbarOnWindowMove(gp::Window* gw, float x, float y) {
    if (!gCapture) {
        TrackStartPageMouse(gw, x, y, true);
    }
    OverlayScrollbar* sb = gCapture;
    if (!IsLive(sb) || !sb->isDragging) {
        return;
    }
    OnDragMove(sb, (int)((IsVert(sb) ? y : x) / sb->k));
}

void OverlayScrollbarOnWindowUp(gp::Window* gw, gp::MouseButton button) {
    (void)gw;
    OverlayScrollbar* sb = gCapture;
    if (!IsLive(sb) || button != gp::MouseButton::Left) {
        return;
    }
    OnLeftButtonUp(sb);
}

void ScrollbarView::OnWinMove(ScrollbarView*, gp::Ctx* cx, const gp::MouseMoveEvent* ev) {
    OverlayScrollbarOnWindowMove(cx->win, ev->x, ev->y);
}

void ScrollbarView::OnWinUp(ScrollbarView*, gp::Ctx* cx, const gp::MouseUpEvent* ev) {
    OverlayScrollbarOnWindowUp(cx->win, ev->button);
}

// ng: a gpui menu item's action is dispatched from the focused element, which
// the bar is no ancestor of, so the items call back directly
void ScrollbarView::OnMenu(ScrollbarView*, gp::Ctx*, const gp::ClickEvent*, int64_t arg) {
    OverlayScrollbar* sb = gMenuScrollbar;
    gMenuScrollbar = nullptr;
    if (!IsLive(sb)) {
        return;
    }
    if ((int)arg == kScrollHere) {
        sb->nTrackPos = PosForThumbCenteredAt(sb, sb->menuPos);
        SendScrollMsg(sb, ScrollMsg::ThumbTrack);
    } else {
        SendScrollMsg(sb, (ScrollMsg)arg);
    }
    PaintScrollbar(sb);
}

void OverlayScrollbarHookWindow(gp::Window* gw) {
    gp::Entity<ScrollbarView> view = View(gw->app);
    // move and up are owned by the canvas, which forwards here: a window has
    // one of each listener, and a resize drag has to see the ones the canvas misses
    gp::WindowOnMouseExit(gw, gp::ListenTo(view, &ScrollbarView::OnWinExit));
}

// the menu win32 shows for a right click on a window's scrollbar
static gpc::PopupMenu* BuildScrollbarMenu(gp::Ctx* cx, OverlayScrollbar* sb, Str id) {
    gpc::PopupMenu* menu = gpc::PopupMenu::New(cx, GpuiDup(cx->a, fmt("%s-menu", id)))->MinW(160);
    gp::Entity<ScrollbarView> view = View(cx->app);
    bool vert = IsVert(sb);
    auto add = [&](gp::Str vertLabel, gp::Str horzLabel, ScrollMsg vertMsg, ScrollMsg horzMsg) {
        int64_t arg = (int64_t)(vert ? vertMsg : horzMsg);
        menu->Menu(vert ? vertLabel : horzLabel)->OnClick(gp::ListenTo(view, &ScrollbarView::OnMenu, arg));
    };
    menu->Menu(GStrL("Scroll Here"))->OnClick(gp::ListenTo(view, &ScrollbarView::OnMenu, (int64_t)kScrollHere));
    menu->Separator();
    add(GStrL("Top"), GStrL("Left Edge"), ScrollMsg::Top, ScrollMsg::Left);
    add(GStrL("Bottom"), GStrL("Right Edge"), ScrollMsg::Bottom, ScrollMsg::Right);
    menu->Separator();
    add(GStrL("Page Up"), GStrL("Page Left"), ScrollMsg::PageUp, ScrollMsg::PageLeft);
    add(GStrL("Page Down"), GStrL("Page Right"), ScrollMsg::PageDown, ScrollMsg::PageRight);
    menu->Separator();
    add(GStrL("Scroll Up"), GStrL("Scroll Left"), ScrollMsg::LineUp, ScrollMsg::LineLeft);
    add(GStrL("Scroll Down"), GStrL("Scroll Right"), ScrollMsg::LineDown, ScrollMsg::LineRight);
    return menu;
}

gp::El* OverlayScrollbarBuild(gp::Ctx* cx, OverlayScrollbar* sb, Str id, int barLen, float k,
                              const gp::Listener* onWheel) {
    int w = OverlayScrollbarWidth(sb);
    sb->clientDx = IsVert(sb) ? w : barLen;
    sb->clientDy = IsVert(sb) ? barLen : w;
    sb->k = k;

    gp::Entity<ScrollbarView> view = View(cx->app);
    int64_t arg = (int64_t)(intptr_t)sb;
    gp::Str gid = GpuiDup(cx->a, id);
    gp::El* bar = gp::Div(cx->a)
                      ->SizeFull()
                      ->Id(gid)
                      ->Click(gp::HashClickId(gid))
                      ->OnMouseDown(gp::ListenTo(view, &ScrollbarView::OnDown, arg))
                      ->OnMouseUp(gp::ListenTo(view, &ScrollbarView::OnUp, arg))
                      ->OnMouseMove(gp::ListenTo(view, &ScrollbarView::OnMove, arg))
                      ->OnHover(gp::ListenTo(view, &ScrollbarView::OnHover, arg));
    if (onWheel) {
        bar->OnScrollWheel(*onWheel);
    }
    bar->customPaint = &OnPaintScrollbar;
    bar->customUser = sb;

    gp::El* box = gp::Div(cx->a)->W((float)sb->clientDx * k)->H((float)sb->clientDy * k)->Child(bar);
    if (sb->mode != OverlayScrollbar::Mode::Windows) {
        // orig's overlay bar has no menu
        sb->popupIdx = -1;
        sb->popupGen = 0;
        return box;
    }
    gpc::PopupMenu* menu = TrackPopup(cx, BuildScrollbarMenu(cx, sb, id));
    sb->popupIdx = menu->state.id.index;
    sb->popupGen = menu->state.id.gen;
    return gpc::ContextMenu::New(cx, GpuiDup(cx->a, fmt("%s-ctx", id)))->Child(box)->Menu(menu)->IntoEl();
}

// --- the two bars of a window -----------------------------------------------

static OverlayScrollbar::Mode ModeFromPrefs() {
    switch (ScrollbarModeFromPrefs()) {
        case kScrollbarOverlay:
            return OverlayScrollbar::Mode::Thick;
        case kScrollbarSmart:
            return OverlayScrollbar::Mode::Smart;
        default:
            return OverlayScrollbar::Mode::Windows;
    }
}

// orig's WM_VSCROLL / WM_HSCROLL, which go to whatever the canvas shows
static void OnOwnerScrollMsg(MainWindow* win, OverlayScrollbar* sb, ScrollMsg msg) {
    if (!IsMainWindowValid(win)) {
        return;
    }
    bool vert = sb->type == OverlayScrollbar::Type::Vert;
    if (win->AsFixed()) {
        if (vert) {
            CanvasOnVScroll(win, msg, sb->nTrackPos);
        } else {
            CanvasOnHScroll(win, msg, sb->nTrackPos);
        }
        return;
    }
    // orig's WndProcCanvasAbout
    if (vert && win->IsCurrentTabAbout()) {
        HomePageOnVScroll(win, msg, sb->nTrackPos);
    }
}

void OverlayScrollbarsSyncMode(MainWindow* win) {
    OverlayScrollbar::Mode mode = ModeFromPrefs();
    if (!win->overlayScrollV) {
        win->overlayScrollV = OverlayScrollbarCreate(win, OverlayScrollbar::Type::Vert, mode, OnOwnerScrollMsg);
        win->overlayScrollH = OverlayScrollbarCreate(win, OverlayScrollbar::Type::Horz, mode, OnOwnerScrollMsg);
        return;
    }
    OverlayScrollbarSetMode(win->overlayScrollV, mode);
    OverlayScrollbarSetMode(win->overlayScrollH, mode);
}

void OverlayScrollbarsDelete(MainWindow* win) {
    OverlayScrollbarDestroy(win->overlayScrollV);
    OverlayScrollbarDestroy(win->overlayScrollH);
    win->overlayScrollV = nullptr;
    win->overlayScrollH = nullptr;
}

void OverlayScrollbarsOnMouse(MainWindow* win, int x, int y, bool onCanvas) {
    if (!ScrollbarsUseOverlay()) {
        return;
    }
    OverlayScrollbarsSyncMode(win);
    // ng: (x, y) and the widths are pixels, canvasRc is in dips
    float k = CanvasScale(win);
    int canvasDx = (int)((float)win->canvasRc.dx / k);
    int canvasDy = (int)((float)win->canvasRc.dy / k);
    OverlayScrollbar* v = win->overlayScrollV;
    OverlayScrollbar* h = win->overlayScrollH;
    // orig's GetScrollbarScreenRect: the proximity band is always thick wide
    bool overV = onCanvas && x >= canvasDx - v->thickWidth;
    bool overH = onCanvas && y >= canvasDy - h->thickWidth;
    OverlayScrollbarOnMouse(v, onCanvas, overV, true);
    OverlayScrollbarOnMouse(h, onCanvas, overH, true);
}

void OverlayScrollbarsNotifyScroll(MainWindow* win) {
    if (!ScrollbarsUseOverlay()) {
        return;
    }
    OverlayScrollbarsSyncMode(win);
    OverlayScrollbarNotifyScroll(win->overlayScrollV);
    OverlayScrollbarNotifyScroll(win->overlayScrollH);
}

void OverlayScrollbarsTick(MainWindow* win, int elapsedMs) {
    if (!win->overlayScrollV) {
        return;
    }
    // orig's IsOrIsParentOf(GetForegroundWindow(), hwndOwner)
    bool ownerActive = win->gpuiWin && win->gpuiWin->active;
    bool changed = OverlayScrollbarTick(win->overlayScrollV, elapsedMs, ownerActive);
    changed |= OverlayScrollbarTick(win->overlayScrollH, elapsedMs, ownerActive);
    if (changed) {
        AppShellInvalidate(win);
    }
}
