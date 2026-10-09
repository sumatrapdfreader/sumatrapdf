/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Citation / reference hover — lifecycle and scheduling. Popup UI, async
// render, region detection, canvas wiring, and plain-text lookup live in
// sibling RefHover*.cpp files.

#include "base/Base.h"
#include "base/Pixmap.h"
#if OS_WIN
#include "base/Win.h"
#endif

#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "DisplayMode.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "gui/AppShell.h"
#include "RefHover.h"

#if OS_WIN
// Tests post WM_MOUSEWHEEL at the popup hwnd. The popup is an element, so a
// message-only window stands in for that hwnd.
struct RefHoverMsgWin {
    RefHoverState* s = nullptr;
    HWND hwnd = nullptr;
};

static Vec<RefHoverMsgWin> gRefHoverMsgWins;

static constexpr const WCHAR* kRefHoverMsgClass = L"SumatraPDFRefHoverMsg";

static RefHoverState* StateForMsgHwnd(HWND hwnd) {
    for (int i = 0; i < len(gRefHoverMsgWins); i++) {
        if (gRefHoverMsgWins[i].hwnd == hwnd) {
            return gRefHoverMsgWins[i].s;
        }
    }
    return nullptr;
}

static LRESULT CALLBACK RefHoverMsgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg != WM_MOUSEWHEEL && msg != WM_MOUSEHWHEEL) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    RefHoverState* s = StateForMsgHwnd(hwnd);
    if (!s || !s->visible || !s->hitEngine) {
        return 0;
    }
    int delta = GET_WHEEL_DELTA_WPARAM(wp);
    if (msg == WM_MOUSEHWHEEL) {
        RefHoverWheelScroll(s, s->hitEngine, -delta);
    } else if (((GET_KEYSTATE_WPARAM(wp) & MK_CONTROL) != 0) || IsCtrlPressed()) {
        RefHoverWheelZoom(s, s->hitEngine, delta);
    } else {
        RefHoverWheelScroll(s, s->hitEngine, delta);
    }
    if (s->win) {
        AppShellInvalidate(s->win);
    }
    return 0;
}

static void EnsureMsgClass() {
    static bool registered = false;
    if (registered) {
        return;
    }
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = RefHoverMsgProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kRefHoverMsgClass;
    registered = RegisterClassExW(&wc) != 0;
}

static void CreateMsgWin(RefHoverState* s) {
    EnsureMsgClass();
    HWND hwnd = CreateWindowExW(0, kRefHoverMsgClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                GetModuleHandleW(nullptr), nullptr);
    if (!hwnd) {
        return;
    }
    RefHoverMsgWin w;
    w.s = s;
    w.hwnd = hwnd;
    VecAppend(gRefHoverMsgWins, w);
}

static void DestroyMsgWin(RefHoverState* s) {
    for (int i = 0; i < len(gRefHoverMsgWins); i++) {
        if (gRefHoverMsgWins[i].s != s) {
            continue;
        }
        HWND hwnd = gRefHoverMsgWins[i].hwnd;
        VecRemoveAt(gRefHoverMsgWins, i);
        if (hwnd) {
            DestroyWindow(hwnd);
        }
        return;
    }
}
#endif

float CanvasScale(MainWindow* win);

// Tests pass a frame client point. The document hit-test wants canvas pixels.
static bool FrameClientToDoc(MainWindow* win, int px, int py, Point* out) {
    float scale = CanvasScale(win);
    if (scale <= 0.f) {
        scale = 1.f;
    }
    float dipX = (float)px * scale;
    float dipY = (float)py * scale;
    Rect rc = win->canvasRc;
    if (dipX < (float)rc.x || dipY < (float)rc.y || dipX >= (float)(rc.x + rc.dx) || dipY >= (float)(rc.y + rc.dy)) {
        return false;
    }
    *out = Point{(int)((dipX - (float)rc.x) / scale), (int)((dipY - (float)rc.y) / scale)};
    return true;
}

RefHoverState* RefHoverCreate(MainWindow* win) {
    auto* s = new RefHoverState();
    s->win = win;
    RefHoverRegisterLiveState(s);
#if OS_WIN
    CreateMsgWin(s);
#endif
    return s;
}

void RefHoverDestroy(RefHoverState* s) {
    if (!s) {
        return;
    }
#if OS_WIN
    DestroyMsgWin(s);
#endif
    RefHoverUnregisterLiveState(s);
    RefHoverDropQueuedRender(s);
    RefHoverFreeRenderImage(s);
    FreePixmap(s->bmp);
    s->bmp = nullptr;
    if (s->hitEngine) {
        s->hitEngine->Release();
        s->hitEngine = nullptr;
    }
    RefHoverFreeLookupCache(s);
    delete s;
}

// delayMs: how long the cursor must hover before the popup shows
// (the CitationHoverDelay advanced setting)
void RefHoverSchedule(RefHoverState* s, int delayMs, Point screenPt, int destPage, float destX, float destY,
                      float destZoom, int srcPage, RectF srcRect, Rect pageScreenRect) {
    if (!s || delayMs < 0) {
        return;
    }
    s->showLeftMs = -1;
    s->hideLeftMs = -1;

    bool sameSrc = s->displayed.srcPage == srcPage && s->displayed.srcRect == srcRect;
    if (s->visible && s->displayed.destPageRaw == destPage && s->displayed.destX == destX &&
        s->displayed.destY == destY && sameSrc) {
        return;
    }
    s->pending.screenPt = screenPt;
    s->pending.destPage = destPage;
    s->pending.destX = destX;
    s->pending.destY = destY;
    s->pending.destZoom = destZoom;
    s->pending.srcPage = srcPage;
    s->pending.srcRect = srcRect;
    s->pending.pageScreenRect = pageScreenRect;
    if (s->visible) {
        delayMs = 0;
    }
    s->showLeftMs = delayMs;
}

void RefHoverHide(RefHoverState* s) {
    if (!s) {
        return;
    }
    s->showLeftMs = -1;
    s->hideLeftMs = -1;
    s->pending.destPage = -1;
    s->renderGen++;
    RefHoverDropQueuedRender(s);
    if (s->visible) {
        s->visible = false;
        s->displayed.destPage = -1;
        AppShellInvalidate(s->win);
    }
}

static constexpr int kRefHoverHidePollMs = 150;
static constexpr int kRefHoverHideMinMs = 250;

// Like RefHoverHide but deferred: cancels any pending show immediately, then
// hides the visible popup after delayMs. While the timer is pending, moving
// the cursor onto the popup (e.g. to click a DOI link inside it) keeps it
// alive. Lets the cursor cross the gap between the link and the popup without
// the popup vanishing. Cancelled by a new RefHoverSchedule / RefHoverHide.
void RefHoverScheduleHide(RefHoverState* s, int delayMs) {
    if (!s) {
        return;
    }
    s->showLeftMs = -1;
    s->pending.destPage = -1;
    s->renderGen++;
    RefHoverDropQueuedRender(s);
    if (!s->visible) {
        s->displayed.destPage = -1;
        return;
    }
    delayMs = std::max(delayMs, kRefHoverHideMinMs);
    s->hideLeftMs = delayMs;
}

// Fired by the hide timer: hides the popup unless the cursor is now over it
// (in which case it re-arms and keeps the popup up).
void RefHoverOnHideTimer(RefHoverState* s) {
    if (!s) {
        return;
    }
    s->hideLeftMs = -1;
    if (!s->visible) {
        return;
    }
    if (s->cursorOverPopup) {
        s->hideLeftMs = kRefHoverHidePollMs;
        return;
    }
    s->visible = false;
    s->displayed.destPage = -1;
    AppShellInvalidate(s->win);
}

// Open a launch link (external URL / file) hit-tested inside the popup.
void RefHoverHandlePopupClick(RefHoverState* s, IPageDestination* dest) {
    if (!s || !dest || !s->ctrl) {
        return;
    }
    RefHoverHide(s);
    s->ctrl->HandleLink(dest, s->linkHandler);
}

int RefHoverPopupWidthCap(RefHoverState* s, int minWidth) {
    if (!s || !s->win || s->win->canvasRc.dx <= 0) {
        return minWidth;
    }
    int canvasWidth = s->win->canvasRc.dx;
    int width = std::max(canvasWidth * 95 / 100, minWidth);
    return std::min(width, canvasWidth);
}

// ng: orig's kRefHoverTimerID / kRefHoverHideTimerID, counted down by the
// shell's tick (RefHoverOnCanvasTimer in orig's RefHoverCanvas.cpp)
void RefHoverTick(MainWindow* win, int elapsedMs) {
    RefHoverState* s = win ? win->refHover : nullptr;
    if (!s) {
        return;
    }
    if (s->hideLeftMs >= 0) {
        s->hideLeftMs -= elapsedMs;
        if (s->hideLeftMs <= 0) {
            RefHoverOnHideTimer(s);
        }
    }
    if (s->showLeftMs < 0) {
        return;
    }
    s->showLeftMs -= elapsedMs;
    if (s->showLeftMs > 0) {
        return;
    }
    s->showLeftMs = -1;
    DisplayModel* dm = win->AsFixed();
    int destPage = s->pending.destPage;
    if (!dm || !dm->ValidPageNo(destPage)) {
        RefHoverHide(s);
        return;
    }
    RefHoverOnTimer(s, dm->GetEngine(), dm->GetZoomReal(destPage));
}

bool RefHoverTakePostedWheel(MainWindow* win, bool horizontal, int delta, bool isCtrl, bool isShift, int clientX,
                             int clientY) {
    RefHoverState* s = win ? win->refHover : nullptr;
    if (!s || !s->visible) {
        return false;
    }
    // a plain vertical wheel still scrolls the document
    if (!horizontal && !isCtrl && !isShift) {
        return false;
    }
    Point doc;
    if (!FrameClientToDoc(win, clientX, clientY, &doc)) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    int srcPage = s->displayed.srcPage;
    if (!dm || !dm->ValidPageNo(srcPage) || !s->hitEngine) {
        return false;
    }
    PointF pagePt = dm->CvtFromScreen(doc, srcPage);
    if (!s->displayed.srcRect.Contains(pagePt)) {
        return false;
    }
    if (horizontal) {
        RefHoverWheelScroll(s, s->hitEngine, -delta);
    } else if (isCtrl) {
        RefHoverWheelZoom(s, s->hitEngine, delta);
    } else {
        RefHoverWheelScroll(s, s->hitEngine, delta);
    }
    AppShellInvalidate(win);
    return true;
}

int RefHoverPopupHwndInt(RefHoverState* s) {
#if OS_WIN
    for (int i = 0; i < len(gRefHoverMsgWins); i++) {
        if (gRefHoverMsgWins[i].s == s) {
            return (int)(INT_PTR)gRefHoverMsgWins[i].hwnd;
        }
    }
#else
    (void)s;
#endif
    return 0;
}

// Citation hover popup state. "show" opens the popup for the link at the
// frame point, because a test cursor cannot hold a hover. tests/issue-6252.ts.
TempStr RefHoverResultTemp(Str action, int x, int y, int* exitCodeOut) {
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (str::Eq(action, StrL("show"))) {
        if (!win || !dm) {
            if (exitCodeOut) {
                *exitCodeOut = 2;
            }
            return str::DupTemp(StrL("NOTREADY no-document"));
        }
        Point doc;
        if (!FrameClientToDoc(win, x, y, &doc)) {
            if (exitCodeOut) {
                *exitCodeOut = 1;
            }
            return fmt("ERROR no-link x=%d y=%d", x, y);
        }
        int srcPage = 0;
        IPageElement* el = dm->GetElementAtPos(doc, &srcPage);
        RefHoverOnCanvasMouseMove(win->refHover, win, win->ctrl, win->linkHandler, dm, doc.x, doc.y, el, srcPage, 0);
        RefHoverState* s = win->refHover;
        if (!s || (s->showLeftMs < 0 && !s->visible)) {
            if (exitCodeOut) {
                *exitCodeOut = 1;
            }
            return fmt("ERROR no-link x=%d y=%d", x, y);
        }
        if (s->showLeftMs >= 0) {
            RefHoverTick(win, 1000);
        }
    }
    RefHoverState* s = win ? win->refHover : nullptr;
    if (!s || !s->visible) {
        return str::DupTemp(StrL("OK visible=0"));
    }
    auto& d = s->displayed;
    return fmt("OK visible=1 hwnd=%d page=%d y=%d zoom=%d", RefHoverPopupHwndInt(s), d.destPage, (int)d.region.y,
               (int)(d.userZoom * 100));
}
