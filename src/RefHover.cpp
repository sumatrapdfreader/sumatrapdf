/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Citation / reference hover — manual test checklist.
// Each item names a hover target and the popup behavior we want.
// Verify by hovering the described link and watching the popup.
//
// Bibliography / reference list:
//   [ ] [Nyg11]-style description-list (numeric "[1]" or alphanumeric
//       "[Foo+09]"): popup shows just the one entry, not adjacent ones.
//   [ ] Author-year hanging-indent bib ("Smith, J. (2020). ..."): popup
//       shows just the entry, including all wrapped continuation lines.
//   [ ] Single-line author-year entry: same as above.
//   [ ] Abbreviation / glossary list ("JVM  Java Virtual Machine. 19, 36"):
//       popup shows just the one entry, even when single-line with no
//       bracket prefix.
//   [ ] Numbered footnotes / endnotes: popup shows just the one entry.
//
// Caption / heading / table / code-listing destination:
//   [ ] "Figure N.M" / "Table N.M" reference whose dest lands at the
//       caption text: landscape view (full page width) with caption +
//       figure body.
//   [ ] "Figure N.M" reference whose dest lands at a code-listing body
//       above the caption: landscape view — popup includes the caption
//       below the code.
//   [ ] "Section N.M" / "Chapter N" reference (numbered heading):
//       landscape view, anchored at the heading down to the page text.
//   [ ] Table reference (rows arranged in columns at the dest): landscape.
//   [ ] Trailing blank page margin trimmed off the popup bottom (region
//       ends just below the last text glyph, not at the page boundary).
//
// Positioning / interaction:
//   [ ] Popup not clipped at screen edges when cursor is near a corner
//       and the popup has to flip to the alternate side.
//   [ ] Two adjacent links to the same destination page but different
//       positions each render their own content (no stale popup on the
//       second hover).
//   [ ] Popup hides on mouse-out, re-appears on re-enter.
//   [ ] Mouse-wheel over popup scrolls; rolls over to the prev / next page
//       when the viewport reaches a page edge.
//   [ ] Ctrl+mouse-wheel over popup zooms in / out.
//   [ ] Popup height grows on bigger monitors (capped at ~90% of work
//       area, max 1400px).
//
// Page-level link destinations (no specific destY) — we extract the source
// link's text from its source rect and search the destination page for that
// text, using the leftmost match's Y as destY. This covers the common
// abbreviation / glossary case (hovering "AKM" in body text → popup crops
// to the AKM entry on the abbreviations page).
//
// Known limits:
//   - Page-level link whose source rect doesn't isolate a unique key (e.g.
//     a TOC line "1.2 Foo .... 12" — many such lines may share a prefix
//     across the doc): falls back to full-page landscape view.
//   - PDFs whose link destinations are authored at an unexpected Y (e.g.
//     mid-paragraph): popup anchors there; this code can't fix a
//     misauthored destination.

#include "utils/BaseUtil.h"
#include "utils/Dpi.h"
#include "utils/ThreadUtil.h"
#include "utils/UITask.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "RefHover.h"
#include "RefHoverDetect.h"

#define REF_HOVER_CLASS L"SumatraPDFRefHover"

// upper bound for the auto-fit base zoom. We render at min(kRenderZoom,
// fit-to-popup-max), then multiply by RefHoverState::Displayed::userZoom
// (the mouse-wheel adjustment).
static constexpr float kRenderZoom = 1.5f;
// pixel metrics below are for 96 dpi, DpiScale()'d at point of use.
// upper bounds for the popup window.
static constexpr int kMaxPopupWidth = 1200;
static constexpr int kMaxPopupHeight = 600;
static constexpr int kBorder = 4;
// vertical gap kept between the cursor and the popup.
static constexpr int kCursorPad = 30;
// pixels scrolled per mouse-wheel notch.
static constexpr int kScrollStepPx = 60;
// user-zoom (mouse-wheel) bounds and step.
static constexpr float kMinUserZoom = 0.4f;
static constexpr float kMaxUserZoom = 3.0f;
static constexpr float kUserZoomStep = 1.15f;

// 0 = not tried, 1 = registered, -1 = failed
static int gClassRegistered = 0;

static LRESULT CALLBACK RefHoverWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);

        HBRUSH hbg = CreateSolidBrush(RGB(255, 252, 200));
        FillRect(hdc, &rc, hbg);
        DeleteObject(hbg);

        RefHoverState* s = (RefHoverState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        if (s && s->bmp) {
            // Draw the bitmap at native pixel size, top-left aligned.
            // GDI clips at the popup edges when the bitmap exceeds the
            // client area (which happens when the user wheel-zooms in —
            // that's how zoomed-in content "fills" the previously blank
            // bottom/right of the popup).
            Size bmpSize = s->bmp->GetSize();
            int border = DpiScale(hwnd, kBorder);
            HDC bmpDC = CreateCompatibleDC(hdc);
            HGDIOBJ oldBmp = bmpDC ? SelectObject(bmpDC, s->bmp->GetBitmap()) : nullptr;
            if (oldBmp) {
                BitBlt(hdc, border, border, bmpSize.dx, bmpSize.dy, bmpDC, 0, 0, SRCCOPY);
                SelectObject(bmpDC, oldBmp);
            }
            if (bmpDC) {
                DeleteDC(bmpDC);
            }
        }

        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_ERASEBKGND) {
        return 1;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// returns false if registration failed (and won't be retried)
static bool RegisterClassIfNeeded() {
    if (gClassRegistered != 0) {
        return gClassRegistered > 0;
    }
    WNDCLASSW wc{};
    wc.lpfnWndProc = RefHoverWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = REF_HOVER_CLASS;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    ATOM atom = RegisterClassW(&wc);
    gClassRegistered = atom ? 1 : -1;
    return gClassRegistered > 0;
}

// live RefHoverState instances (one per MainWindow). An async render
// completion looks its state up here so a popup destroyed while a render
// was in flight (e.g. the window was closed) is detected and the result
// dropped instead of dereferencing freed memory.
constexpr int kMaxLiveStates = 32;
static RefHoverState* gLiveStates[kMaxLiveStates];

static bool IsLiveState(RefHoverState* s) {
    for (RefHoverState* live : gLiveStates) {
        if (live == s) {
            return true;
        }
    }
    return false;
}

static void DropQueuedRender(RefHoverState* s) {
    if (s->queuedRender.valid && s->queuedRender.engine) {
        s->queuedRender.engine->Release();
    }
    s->queuedRender.valid = false;
    s->queuedRender.engine = nullptr;
}

RefHoverState* RefHoverCreate(HWND hwndCanvas) {
    if (!RegisterClassIfNeeded()) {
        // don't keep retrying window creation on every mouse move
        return nullptr;
    }
    auto* s = new RefHoverState();
    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, REF_HOVER_CLASS, nullptr, WS_POPUP | WS_BORDER, 0, 0, 10, 10,
                                hwndCanvas, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!hwnd) {
        delete s;
        return nullptr;
    }
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)s);
    s->hwndPopup = hwnd;
    for (RefHoverState*& slot : gLiveStates) {
        if (!slot) {
            slot = s;
            break;
        }
    }
    return s;
}

void RefHoverDestroy(RefHoverState* s) {
    if (!s) {
        return;
    }
    for (RefHoverState*& slot : gLiveStates) {
        if (slot == s) {
            slot = nullptr;
            break;
        }
    }
    DropQueuedRender(s);
    if (s->hwndPopup) {
        DestroyWindow(s->hwndPopup);
        s->hwndPopup = nullptr;
    }
    delete s->bmp;
    s->bmp = nullptr;
    delete s;
}

static void ShowPopup(RefHoverState* s, Point screenPt) {
    if (!s || !s->hwndPopup || !s->bmp) {
        return;
    }
    Size bmpSize = s->bmp->GetSize();
    int border = DpiScale(s->hwndPopup, kBorder);
    int popupW = bmpSize.dx + 2 * border;
    int popupH = bmpSize.dy + 2 * border;

    HMONITOR hmon = MonitorFromPoint({screenPt.x, screenPt.y}, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(hmon, &mi);

    // Horizontal bounds: monitor work area (popup is allowed to extend
    // into the gray margins beyond the page text column).
    // Vertical bounds: monitor work area intersected with the page screen
    // rect, so the popup stays within the visible page vertically and
    // doesn't spill into the next-page area or the page header / footer
    // gap above / below.
    int leftBound = mi.rcWork.left;
    int rightBound = mi.rcWork.right;
    int topBound = mi.rcWork.top;
    int bottomBound = mi.rcWork.bottom;
    Rect pr = s->pending.pageScreenRect;
    if (pr.dy > 0) {
        if (pr.y > topBound) {
            topBound = pr.y;
        }
        if (pr.y + pr.dy < bottomBound) {
            bottomBound = pr.y + pr.dy;
        }
    }
    int boundW = rightBound - leftBound;
    int boundH = bottomBound - topBound;
    if (popupW > boundW) {
        popupW = boundW;
    }
    if (popupH > boundH) {
        popupH = boundH;
    }

    // Horizontally: center the popup on the source page (not the canvas /
    // monitor edge) so when the popup is wider than the page text column it
    // expands symmetrically into the gray margins. The popup's X is
    // independent of the cursor so consecutive hovers on the same page
    // don't make the popup jump horizontally.
    int pageCenterX = (pr.dx > 0) ? (pr.x + pr.dx / 2) : screenPt.x;
    int x = pageCenterX - popupW / 2;
    // Vertically: gap above/below the cursor so 1-2 lines of context around
    // the hovered word stay visible. Prefer below cursor; flip above if
    // overflow. If neither side fits the full popup, shrink popupH to
    // whichever side has more room — popup is cut (bitmap clipped at popup
    // edges in WM_PAINT) rather than overlapping the cursor.
    int cursorPad = DpiScale(s->hwndPopup, kCursorPad);
    int spaceBelow = bottomBound - (screenPt.y + cursorPad);
    int spaceAbove = (screenPt.y - cursorPad) - topBound;
    int y;
    if (spaceBelow >= popupH) {
        y = screenPt.y + cursorPad;
    } else if (spaceAbove >= popupH) {
        y = screenPt.y - popupH - cursorPad;
    } else if (spaceBelow >= spaceAbove) {
        if (spaceBelow > 0) {
            popupH = spaceBelow;
        }
        y = screenPt.y + cursorPad;
    } else {
        if (spaceAbove > 0) {
            popupH = spaceAbove;
        }
        y = screenPt.y - popupH - cursorPad;
    }
    // Horizontal clamp to monitor work area.
    if (x < leftBound) {
        x = leftBound;
    }
    if (x + popupW > rightBound) {
        x = rightBound - popupW;
    }
    // Final vertical clamp (defensive).
    if (y < topBound) {
        y = topBound;
    }
    if (y + popupH > bottomBound) {
        popupH = bottomBound - y;
    }

    // degenerate bounds (page barely visible, no room on either side of
    // the cursor): hide instead of showing a 0-sized topmost window
    if (popupW <= 0 || popupH <= 0) {
        ShowWindow(s->hwndPopup, SW_HIDE);
        return;
    }

    SetWindowPos(s->hwndPopup, HWND_TOPMOST, x, y, popupW, popupH, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(s->hwndPopup, nullptr, TRUE);
}

// Renders run on a background thread: a complex page would otherwise stall
// the UI thread for the duration of engine->RenderPage() on every hover and
// every wheel notch (everything else in the app renders via the background
// RenderCache). The job owns an engine reference for the render duration
// (same cross-thread AddRef/Release pattern as Print.cpp); the result is
// marshalled back to the UI thread via uitask and dropped if it's stale
// (renderGen moved on) or the RefHoverState is gone.
struct RefHoverRenderJob {
    RefHoverState* s = nullptr;
    RefHoverState::RenderRequest req;
    RenderedBitmap* bmp = nullptr;
};

static void RefHoverStartRenderJob(RefHoverRenderJob* job);

// runs on the UI thread
static void RefHoverRenderDone(RefHoverRenderJob* job) {
    RefHoverState* s = job->s;
    if (!IsLiveState(s)) {
        delete job->bmp;
        delete job;
        return;
    }
    s->renderInFlight = false;
    if (job->bmp && job->req.gen == s->renderGen) {
        delete s->bmp;
        s->bmp = job->bmp;
        if (job->req.showPopup) {
            s->displayed.destPage = job->req.pageNo;
            // store the raw link destination, not the detection-resolved
            // values: RefHoverSchedule() compares them against incoming raw
            // link values to skip a re-render for the same link. With the
            // resolved values stored, page-level destinations (destY <= 0,
            // e.g. abbreviation links) never compared equal, so every pause
            // of the mouse within the same link re-ran detection plus a
            // render and re-positioned the popup at the new cursor position.
            s->displayed.destX = job->req.destXRaw;
            s->displayed.destY = job->req.destYRaw;
            s->displayed.region = job->req.region;
            ShowPopup(s, job->req.screenPt);
        } else {
            InvalidateRect(s->hwndPopup, nullptr, TRUE);
        }
    } else {
        delete job->bmp;
    }
    delete job;
    // a request arrived while this render was in flight: start it now
    // (wheel zoom / scroll streams coalesce to the latest request)
    if (s->queuedRender.valid) {
        if (s->queuedRender.gen == s->renderGen) {
            auto* next = new RefHoverRenderJob();
            next->s = s;
            next->req = s->queuedRender;
            s->queuedRender.valid = false;
            s->queuedRender.engine = nullptr;
            RefHoverStartRenderJob(next);
        } else {
            DropQueuedRender(s);
        }
    }
}

// runs on a background thread
static void RefHoverRenderThread(RefHoverRenderJob* job) {
    RenderPageArgs args(job->req.pageNo, job->req.zoom, 0, &job->req.region);
    job->bmp = job->req.engine->RenderPage(args);
    job->req.engine->Release();
    job->req.engine = nullptr;
    auto fn = MkFunc0<RefHoverRenderJob>(RefHoverRenderDone, job);
    uitask::Post(fn, "RefHoverRenderDone");
}

static void RefHoverStartRenderJob(RefHoverRenderJob* job) {
    job->s->renderInFlight = true;
    auto fn = MkFunc0<RefHoverRenderJob>(RefHoverRenderThread, job);
    RunAsync(fn, "RefHoverRender");
}

// Queue a render of (pageNo, zoom, region). Older in-flight results are
// invalidated; if a render is already running the request is parked in
// queuedRender (overwriting any previously parked one) and started when
// the running render completes.
static void RefHoverRequestRender(RefHoverState* s, EngineBase* engine, RefHoverState::RenderRequest req) {
    s->renderGen++;
    req.valid = true;
    req.gen = s->renderGen;
    engine->AddRef();
    req.engine = engine;
    if (s->renderInFlight) {
        DropQueuedRender(s);
        s->queuedRender = req;
        return;
    }
    auto* job = new RefHoverRenderJob();
    job->s = s;
    job->req = req;
    RefHoverStartRenderJob(job);
}

// Re-render at the adjusted zoom; popup window keeps its initial size.
// The rendered region is sized to exactly fill the popup at the new zoom
// (anchored at the original detected top-left), so wheel-down brings in
// new page content from below/right and wheel-up shows less of the page
// at higher detail. Either way the popup stays full — no blank area.
// Returns true if the zoom actually changed.
bool RefHoverWheelZoom(RefHoverState* s, EngineBase* engine, int wheelDelta) {
    if (!s || !s->hwndPopup || s->displayed.destPage <= 0 || !engine) {
        return false;
    }
    float factor = (wheelDelta > 0) ? kUserZoomStep : (1.f / kUserZoomStep);
    float newZoom = s->displayed.userZoom * factor;
    if (newZoom < kMinUserZoom) {
        newZoom = kMinUserZoom;
    } else if (newZoom > kMaxUserZoom) {
        newZoom = kMaxUserZoom;
    }
    if (newZoom == s->displayed.userZoom) {
        return false;
    }
    s->displayed.userZoom = newZoom;

    RECT rc;
    GetClientRect(s->hwndPopup, &rc);
    int border = DpiScale(s->hwndPopup, kBorder);
    float clientW = (float)((rc.right - rc.left) - 2 * border);
    float clientH = (float)((rc.bottom - rc.top) - 2 * border);
    float zoom = s->displayed.baseZoom * s->displayed.userZoom;
    if (zoom <= 0.f || clientW <= 0.f || clientH <= 0.f) {
        return false;
    }

    RectF mediabox = engine->PageMediabox(s->displayed.destPage);
    RectF region = s->displayed.region;
    region.dx = clientW / zoom;
    region.dy = clientH / zoom;
    // Clamp to page bounds.
    if (region.x + region.dx > mediabox.dx) {
        region.dx = mediabox.dx - region.x;
    }
    if (region.y + region.dy > mediabox.dy) {
        region.dy = mediabox.dy - region.y;
    }
    if (region.dx <= 0.f || region.dy <= 0.f) {
        return false;
    }

    // Commit the new region immediately (the next wheel notch must compound
    // on it, not on the still-displayed old one) and render asynchronously;
    // the popup repaints with the new bitmap when the render completes.
    // Persisting the popup-fitting dx/dy also keeps a subsequent scroll
    // rendering a bitmap that fills the popup — without it, scroll would
    // fall back to the original (smaller) entry-box dimensions and leave
    // visible padding.
    s->displayed.region = region;
    RefHoverState::RenderRequest req;
    req.pageNo = s->displayed.destPage;
    req.zoom = zoom;
    req.region = region;
    RefHoverRequestRender(s, engine, req);
    return true;
}

// Scroll the rendered region by one wheel notch. Rolls over to the previous
// or next page when the region would cross a page edge — the popup behaves
// like a small continuous-scroll viewport into the document.
bool RefHoverWheelScroll(RefHoverState* s, EngineBase* engine, int wheelDelta) {
    if (!s || !s->hwndPopup || s->displayed.destPage <= 0 || !engine) {
        return false;
    }
    float zoom = s->displayed.baseZoom * s->displayed.userZoom;
    if (zoom <= 0.f) {
        return false;
    }
    int pageCount = engine->PageCount();
    int page = s->displayed.destPage;
    RectF region = s->displayed.region;
    RectF mediabox = engine->PageMediabox(page);
    if (mediabox.dx <= 0.f || mediabox.dy <= 0.f) {
        return false;
    }

    // ~60 screen pixels per WHEEL_DELTA notch, expressed in page points so the
    // perceived scroll speed stays roughly constant across zoom levels.
    // Positive wheelDelta → wheel forward → scroll toward earlier content
    // (region.y decreases). Negative → later content (region.y increases).
    float scrollStep = (float)DpiScale(s->hwndPopup, kScrollStepPx);
    float scrollPt = scrollStep * ((float)wheelDelta / (float)WHEEL_DELTA) / zoom;
    float newY = region.y - scrollPt;

    if (newY < 0.f) {
        // Overflow off the page top — carry the remainder onto the previous
        // page if there is one. Position the new region near the prev page's
        // bottom and offset upward by the overflow so the seam between pages
        // feels continuous across the wheel.
        if (page > 1) {
            float overflow = -newY;
            page--;
            mediabox = engine->PageMediabox(page);
            newY = mediabox.dy - region.dy - overflow;
            if (newY < 0.f) {
                newY = 0.f;
            }
        } else {
            newY = 0.f;
        }
    } else if (newY + region.dy > mediabox.dy) {
        if (page < pageCount) {
            float overflow = (newY + region.dy) - mediabox.dy;
            page++;
            mediabox = engine->PageMediabox(page);
            newY = overflow;
            if (newY + region.dy > mediabox.dy) {
                newY = mediabox.dy - region.dy;
            }
            if (newY < 0.f) {
                newY = 0.f;
            }
        } else {
            newY = mediabox.dy - region.dy;
            if (newY < 0.f) {
                newY = 0.f;
            }
        }
    }

    if (page == s->displayed.destPage && newY == region.y) {
        return false;
    }
    region.y = newY;
    if (region.dy > mediabox.dy) {
        region.dy = mediabox.dy;
    }
    if (region.x + region.dx > mediabox.dx) {
        region.x = mediabox.dx - region.dx;
        if (region.x < 0.f) {
            region.x = 0.f;
            region.dx = mediabox.dx;
        }
    }

    // Commit page/region immediately so the next wheel notch compounds on
    // them; the popup repaints when the async render delivers the bitmap.
    s->displayed.destPage = page;
    s->displayed.region = region;
    RefHoverState::RenderRequest req;
    req.pageNo = page;
    req.zoom = zoom;
    req.region = region;
    RefHoverRequestRender(s, engine, req);
    return true;
}

void RefHoverSchedule(RefHoverState* s, HWND hwndCanvas, int delayMs, Point screenPt, int destPage, float destX,
                      float destY, float destZoom, int srcPage, RectF srcRect, Rect pageScreenRect) {
    if (!s || delayMs < 0) {
        return;
    }
    KillTimer(hwndCanvas, kRefHoverTimerID);

    if (IsWindowVisible(s->hwndPopup) && s->displayed.destPage == destPage && s->displayed.destX == destX &&
        s->displayed.destY == destY) {
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
    SetTimer(hwndCanvas, kRefHoverTimerID, (UINT)delayMs, nullptr);
}

void RefHoverHide(RefHoverState* s, HWND hwndCanvas) {
    if (!s) {
        return;
    }
    KillTimer(hwndCanvas, kRefHoverTimerID);
    s->pending.destPage = -1;
    // invalidate any in-flight render result and drop a parked one — a slow
    // render finishing after hide must not pop the window back up
    s->renderGen++;
    DropQueuedRender(s);
    if (s->hwndPopup && IsWindowVisible(s->hwndPopup)) {
        ShowWindow(s->hwndPopup, SW_HIDE);
        s->displayed.destPage = -1;
    }
}

// When the link's destination is page-level (destY < 0), try to recover a
// specific Y by reading the source link's text from srcPage at srcRect, then
// searching for that text on destPage. Returns -1 if no match.
//
// Typical case: hovering an abbreviation in body text (e.g. "AKM") whose link
// points to the abbreviations page without a specific Y. Source rect covers
// "AKM"; we look for "AKM" at the leftmost position on destPage (the
// description-list entry start) and return its Y. With a recovered destY,
// DetectEntryBox can crop the popup to the matching entry.
static float ResolveDestYFromSourceText(EngineBase* engine, int srcPage, RectF srcRect, int destPage) {
    if (srcPage <= 0 || destPage <= 0 || srcRect.dx <= 0.f || srcRect.dy <= 0.f) {
        return -1.f;
    }
    int srcLen = 0;
    Rect* srcCoords = nullptr;
    const WCHAR* srcText = engine->GetTextForPage(srcPage, &srcLen, &srcCoords);
    if (!srcText || srcLen <= 0 || !srcCoords) {
        return -1.f;
    }
    // Tight margin: include glyphs partially overlapping the rect (handles
    // 1-2pt slop from tool-authored rects) without sweeping in adjacent
    // body text. A wider margin would let the longest-alnum heuristic
    // latch onto unrelated nearby words ("ADR" near "linking" → picks
    // "linking" → no match on dest page → fall to landscape view).
    int srcL = (int)srcRect.x - 2;
    int srcT = (int)srcRect.y - 2;
    int srcR = (int)(srcRect.x + srcRect.dx) + 2;
    int srcB = (int)(srcRect.y + srcRect.dy) + 2;

    WCHAR rawText[512];
    int rawLen = 0;
    for (int i = 0; i < srcLen && rawLen < 511; i++) {
        Rect r = srcCoords[i];
        // Include glyph if its bounding box overlaps the search rect — more
        // forgiving than a center-in-rect check when the link rect is
        // tightly authored or slightly offset from the actual glyph.
        if (r.x + r.dx < srcL || r.x > srcR) {
            continue;
        }
        if (r.y + r.dy < srcT || r.y > srcB) {
            continue;
        }
        rawText[rawLen++] = srcText[i];
    }

    auto isAlnum = [](WCHAR c) {
        return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9');
    };

    // Collect alphanumeric candidate runs from the source rect as search
    // needles. Strips surrounding punctuation ("(AKM)" → "AKM"). Tokens
    // flanked by parentheses (the typical "definition" convention for
    // expanding a phrase, e.g. "Architectural Knowledge Management (AKM)")
    // are preferred — that keeps the resolver from latching onto a citation
    // key like "KLV06" inside "[KLV06]" when both appear near the link rect.
    // Each candidate is tried against the dest page in priority order;
    // first match wins. Trying multiple candidates handles cases where the
    // longest run isn't on the dest page but a shorter run is (e.g. Bluey
    // "Jump to all (a-z)" → "Jump" not on dest, "all" matches "All
    // Characters").
    struct Cand {
        int start;
        int len;
        bool flanked;
    };
    constexpr int kMaxCands = 16;
    Cand cands[kMaxCands];
    int ncands = 0;
    int curStart = -1;
    int curLen = 0;
    for (int i = 0; i <= rawLen; i++) {
        bool alnum = (i < rawLen) && isAlnum(rawText[i]);
        if (alnum) {
            if (curStart < 0) {
                curStart = i;
            }
            curLen++;
        } else {
            if (curLen >= 2 && ncands < kMaxCands) {
                bool flanked = (curStart > 0 && rawText[curStart - 1] == L'(' && i < rawLen && rawText[i] == L')');
                cands[ncands++] = {curStart, curLen, flanked};
            }
            curStart = -1;
            curLen = 0;
        }
    }
    if (ncands == 0) {
        return -1.f;
    }
    // Sort: parens-flanked first, then by length descending.
    for (int i = 0; i < ncands - 1; i++) {
        for (int j = i + 1; j < ncands; j++) {
            bool swap = false;
            if (cands[j].flanked && !cands[i].flanked) {
                swap = true;
            } else if (cands[j].flanked == cands[i].flanked && cands[j].len > cands[i].len) {
                swap = true;
            }
            if (swap) {
                Cand t = cands[i];
                cands[i] = cands[j];
                cands[j] = t;
            }
        }
    }

    int destLen = 0;
    Rect* destCoords = nullptr;
    const WCHAR* destText = engine->GetTextForPage(destPage, &destLen, &destCoords);
    if (!destText || destLen <= 0 || !destCoords) {
        return -1.f;
    }
    // Prefer line-start matches (no other glyph at smaller x with same y)
    // over mid-line matches. A "Figure 7.1" caption sits at line start; a
    // body-text mention "in Figure 7.1 below" sits mid-line. Same logic
    // benefits abbreviation entries vs. body mentions of an abbreviation.
    auto isLineStartMatch = [&](int idx) -> bool {
        int sy = destCoords[idx].y;
        int sx = destCoords[idx].x;
        for (int i = 0; i < destLen; i++) {
            if (i == idx) {
                continue;
            }
            if (destCoords[i].y != sy) {
                continue;
            }
            WCHAR c = destText[i];
            if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
                continue;
            }
            if (destCoords[i].x < sx) {
                return false;
            }
        }
        return true;
    };

    for (int ci = 0; ci < ncands; ci++) {
        int bestStart = cands[ci].start;
        int bestLen = cands[ci].len;
        auto matchAt = [&](int idx) -> bool {
            if (idx + bestLen > destLen) {
                return false;
            }
            for (int j = 0; j < bestLen; j++) {
                WCHAR a = destText[idx + j];
                WCHAR b = rawText[bestStart + j];
                if (a >= L'A' && a <= L'Z') {
                    a = (WCHAR)(a + 32);
                }
                if (b >= L'A' && b <= L'Z') {
                    b = (WCHAR)(b + 32);
                }
                if (a != b) {
                    return false;
                }
            }
            if (idx > 0 && isAlnum(destText[idx - 1])) {
                return false;
            }
            if (idx + bestLen < destLen && isAlnum(destText[idx + bestLen])) {
                return false;
            }
            return true;
        };

        int bestX_lineStart = INT_MAX;
        int bestY_lineStart = -1;
        int bestX_any = INT_MAX;
        int bestY_any = -1;
        for (int i = 0; i < destLen; i++) {
            if (!matchAt(i)) {
                continue;
            }
            Rect r = destCoords[i];
            if (isLineStartMatch(i)) {
                if (r.x < bestX_lineStart) {
                    bestX_lineStart = r.x;
                    bestY_lineStart = r.y;
                }
            } else if (r.x < bestX_any) {
                bestX_any = r.x;
                bestY_any = r.y;
            }
        }
        int bestY = (bestY_lineStart >= 0) ? bestY_lineStart : bestY_any;
        if (bestY >= 0) {
            return (float)bestY;
        }
    }
    return -1.f;
}

void RefHoverOnTimer(RefHoverState* s, HWND hwndCanvas, EngineBase* engine, float pageZoom) {
    KillTimer(hwndCanvas, kRefHoverTimerID);
    if (!s || !engine || s->pending.destPage <= 0) {
        return;
    }
    int destPage = s->pending.destPage;
    float destX = s->pending.destX;
    float destY = s->pending.destY;

    RectF mediabox = engine->PageMediabox(destPage);
    if (mediabox.dx <= 0.f || mediabox.dy <= 0.f) {
        return;
    }
    // PageDestGetDestPoint returns {0,0,0,0} when the link has no specific
    // anchor (page-level destination) — that's the typical case for body-text
    // abbreviation / glossary links and for some TOC-derived bib refs. In
    // those cases destY == 0 (not < 0), so we have to treat <= 0 as "no
    // anchor" and try to recover a specific Y from the source link's text.
    // Some PDFs author /XYZ with y just past the page bottom (negative in
    // PDF user space, top-down flips it past mediabox.dy) when they mean
    // "top of page" — e.g. Bluey.pdf "JUMP TO ALL (A-Z)" uses
    // /XYZ 0 -2.58 0. Treat past-page-bottom destY the same as page-level.
    if (destY <= 0.f || destY >= mediabox.dy - 1.f) {
        destY = 0.f;
        float resolved = ResolveDestYFromSourceText(engine, s->pending.srcPage, s->pending.srcRect, destPage);
        if (resolved >= 0.f) {
            destY = resolved;
            if (destX < 0.f) {
                destX = 0.f;
            }
        }
    }

    // When the link supplies an /XYZ zoom hint, honour it: render the
    // destination region anchored at the link's (destX, destY) at the
    // requested zoom rather than auto-fitting a detected entry box.
    // This matches the navigation behaviour (DisplayModel::ScrollTo
    // also reads the link zoom).
    float linkZoom = s->pending.destZoom;
    bool useLinkZoom = (linkZoom > 0.f);
    // Popup must not look smaller than the page does on screen — if the
    // user is already viewing the document at a higher zoom than the
    // link's /XYZ hint, render the popup at the current display zoom
    // instead (still anchored at the link's top). Otherwise the preview
    // would feel like a zoom-OUT, defeating the point of XYZ.
    if (useLinkZoom && pageZoom > linkZoom) {
        linkZoom = pageZoom;
    }

    RectF region;
    if (useLinkZoom) {
        // Span full page width — strict /XYZ would crop at destX, but for
        // a hover preview that just chops the left-most letters of the
        // target lines. Top anchor (destY) is preserved.
        // dx/dy placeholders; resized against popup caps below.
        region = RectF{0.f, destY, mediabox.dx, mediabox.dy - destY};
    } else {
        int textLen = 0;
        Rect* coords = nullptr;
        const WCHAR* text = engine->GetTextForPage(destPage, &textLen, &coords);
        // Equation cross-reference: tight box around the labelled line.
        // Falls through to DetectEntryBox when no eq label is found.
        region = DetectEquationBox(text, coords, textLen, mediabox, destX, destY);
        if (region.dx <= 0.f || region.dy <= 0.f) {
            region = DetectEntryBox(text, coords, textLen, mediabox, destX, destY);
        }
    }
    // New destination — reset user-driven zoom. baseZoom matches the
    // document's current display zoom for the destination page, so popup
    // text height is comparable to the visible page text. Shrink baseZoom
    // if either dimension would exceed the popup max; landscape-style
    // regions for non-reference targets are typically wider than tall, so
    // the width cap matters here.
    s->displayed.userZoom = 1.f;
    float baseZoom = useLinkZoom ? linkZoom : ((pageZoom > 0.f) ? pageZoom : kRenderZoom);
    // Popup max size:
    //   width  ~95% of monitor work area — popup may span beyond the page
    //                text column into the surrounding gray margins so the
    //                rendered figure / caption / table is at a readable
    //                size, not shrunk to fit a narrow text column.
    //   height 45% of source page height — keeps the bottom of the page
    //                visible below the popup so the line under the cursor
    //                and surrounding context stay readable.
    int popupWCap = DpiScale(s->hwndPopup, kMaxPopupWidth);
    {
        POINT mp = {s->pending.screenPt.x, s->pending.screenPt.y};
        HMONITOR hmon = MonitorFromPoint(mp, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(hmon, &mi)) {
            int monW = mi.rcWork.right - mi.rcWork.left;
            int dyn = monW * 95 / 100;
            if (dyn > popupWCap) {
                popupWCap = dyn;
            }
        }
    }
    // Default popup height cap: 45% of source page height, fall back to
    // kMaxPopupHeight when page rect is unknown. For tall regions (figure
    // with caption), grow the cap into whichever side of the cursor has
    // more available space within the page rect, so the figure body and
    // its full caption fit. Cursor at the bottom of the page → popup
    // expands upward into the page top; cursor at top → expands downward.
    int popupHCap;
    int cursorPad = DpiScale(s->hwndPopup, kCursorPad);
    if (region.dy > 250.f && s->pending.pageScreenRect.dy > 0) {
        Rect pr = s->pending.pageScreenRect;
        int curY = s->pending.screenPt.y;
        int spaceAbove = curY - pr.y - cursorPad;
        int spaceBelow = (pr.y + pr.dy) - curY - cursorPad;
        int maxSpace = (spaceAbove > spaceBelow) ? spaceAbove : spaceBelow;
        if (maxSpace < 0) {
            maxSpace = 0;
        }
        int pageBased = pr.dy * 75 / 100;
        popupHCap = (pageBased > maxSpace) ? pageBased : maxSpace;
    } else {
        popupHCap = DpiScale(s->hwndPopup, kMaxPopupHeight);
        if (s->pending.pageScreenRect.dy > 0) {
            int pageBased = s->pending.pageScreenRect.dy * 45 / 100;
            if (pageBased < popupHCap) {
                popupHCap = pageBased;
            }
        }
    }
    int border = DpiScale(s->hwndPopup, kBorder);
    float availH = (float)(popupHCap - 2 * border);
    float availW = (float)(popupWCap - 2 * border);
    if (useLinkZoom) {
        // Keep the link's requested zoom; size the region so it fills the
        // popup without overflowing. Clamp to what's left of the page so
        // we don't render past the mediabox edge.
        float wantW = availW / baseZoom;
        float wantH = availH / baseZoom;
        float maxW = mediabox.dx - region.x;
        float maxH = mediabox.dy - region.y;
        if (wantW > maxW) {
            wantW = maxW;
        }
        if (wantH > maxH) {
            wantH = maxH;
        }
        if (wantW < 1.f) {
            wantW = 1.f;
        }
        if (wantH < 1.f) {
            wantH = 1.f;
        }
        region.dx = wantW;
        region.dy = wantH;
    } else {
        if (region.dy > 0.f && region.dy * baseZoom > availH) {
            baseZoom = availH / region.dy;
        }
        if (region.dx > 0.f && region.dx * baseZoom > availW) {
            baseZoom = availW / region.dx;
        }
    }
    if (baseZoom < kMinUserZoom) {
        baseZoom = kMinUserZoom;
    }
    s->displayed.baseZoom = baseZoom;
    // Render on a background thread; displayed.* is committed and the popup
    // shown by the completion handler, so a slow render never blocks the UI
    // and a hover that moved on (renderGen bumped by hide / a new request)
    // is dropped instead of flashing a stale popup.
    RefHoverState::RenderRequest req;
    req.pageNo = destPage;
    req.zoom = s->displayed.baseZoom * s->displayed.userZoom;
    req.region = region;
    req.showPopup = true;
    req.screenPt = s->pending.screenPt;
    req.destXRaw = s->pending.destX;
    req.destYRaw = s->pending.destY;
    RefHoverRequestRender(s, engine, req);
}
