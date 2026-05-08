/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "RefHover.h"

#define REF_HOVER_CLASS L"SumatraPDFRefHover"

// Larger fallback used when detection found something tiny (likely a figure
// fragment) — gives enough height to capture a diagram + caption.
static constexpr float kFigureStripHeightPt = 280.f;
static constexpr float kAnchorTopMarginPt = 6.f;
static constexpr float kRightMarginPt = 4.f;
// Single-line detected entries below this point-height are treated as
// ambiguous (likely a table caption or section header rather than a real
// bibliography entry) and rendered with the full-page landscape box.
static constexpr float kSingleLinePt = 30.f;
// pt of padding around the detected entry box.
static constexpr float kEntryPadPt = 6.f;
// upper bound for the auto-fit base zoom. We render at min(kRenderZoom,
// fit-to-popup-max), then multiply by RefHoverState::userZoom (the
// mouse-wheel adjustment).
static constexpr float kRenderZoom = 1.5f;
// upper bounds for the popup window in screen pixels.
static constexpr int kMaxPopupWidth = 1200;
static constexpr int kMaxPopupHeight = 600;
static constexpr int kBorder = 4;
// user-zoom (mouse-wheel) bounds and step.
static constexpr float kMinUserZoom = 0.4f;
static constexpr float kMaxUserZoom = 3.0f;
static constexpr float kUserZoomStep = 1.15f;

static bool gClassRegistered = false;

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
            HDC bmpDC = CreateCompatibleDC(hdc);
            HGDIOBJ oldBmp = bmpDC ? SelectObject(bmpDC, s->bmp->GetBitmap()) : nullptr;
            if (oldBmp) {
                BitBlt(hdc, kBorder, kBorder, bmpSize.dx, bmpSize.dy, bmpDC, 0, 0, SRCCOPY);
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

static void RegisterClassIfNeeded() {
    if (gClassRegistered) {
        return;
    }
    WNDCLASSW wc{};
    wc.lpfnWndProc = RefHoverWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = REF_HOVER_CLASS;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&wc);
    gClassRegistered = true;
}

RefHoverState* RefHoverCreate(HWND hwndCanvas) {
    RegisterClassIfNeeded();
    auto* s = new RefHoverState();
    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, REF_HOVER_CLASS, nullptr, WS_POPUP | WS_BORDER, 0, 0, 10, 10,
                                hwndCanvas, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!hwnd) {
        delete s;
        return nullptr;
    }
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)s);
    s->hwndPopup = hwnd;
    return s;
}

void RefHoverDestroy(RefHoverState* s) {
    if (!s) {
        return;
    }
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
    int popupW = bmpSize.dx + 2 * kBorder;
    int popupH = bmpSize.dy + 2 * kBorder;

    HMONITOR hmon = MonitorFromPoint({screenPt.x, screenPt.y}, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(hmon, &mi);
    int monW = mi.rcWork.right - mi.rcWork.left;
    int monH = mi.rcWork.bottom - mi.rcWork.top;
    if (popupW > monW) {
        popupW = monW;
    }
    if (popupH > monH) {
        popupH = monH;
    }

    int x = screenPt.x + 16;
    int y = screenPt.y + 16;
    if (x + popupW > mi.rcWork.right) {
        x = screenPt.x - popupW - 4;
    }
    if (y + popupH > mi.rcWork.bottom) {
        y = screenPt.y - popupH - 4;
    }

    SetWindowPos(s->hwndPopup, HWND_TOPMOST, x, y, popupW, popupH, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(s->hwndPopup, nullptr, TRUE);
}

// Re-render at the adjusted zoom; popup window keeps its initial size.
// The rendered region is sized to exactly fill the popup at the new zoom
// (anchored at the original detected top-left), so wheel-down brings in
// new page content from below/right and wheel-up shows less of the page
// at higher detail. Either way the popup stays full — no blank area.
// Returns true if the zoom actually changed.
bool RefHoverWheelZoom(RefHoverState* s, EngineBase* engine, int wheelDelta) {
    if (!s || !s->hwndPopup || s->displayedDestPage <= 0 || !engine) {
        return false;
    }
    float factor = (wheelDelta > 0) ? kUserZoomStep : (1.f / kUserZoomStep);
    float newZoom = s->userZoom * factor;
    if (newZoom < kMinUserZoom) {
        newZoom = kMinUserZoom;
    } else if (newZoom > kMaxUserZoom) {
        newZoom = kMaxUserZoom;
    }
    if (newZoom == s->userZoom) {
        return false;
    }
    s->userZoom = newZoom;

    RECT rc;
    GetClientRect(s->hwndPopup, &rc);
    float clientW = (float)((rc.right - rc.left) - 2 * kBorder);
    float clientH = (float)((rc.bottom - rc.top) - 2 * kBorder);
    float zoom = s->baseZoom * s->userZoom;
    if (zoom <= 0.f || clientW <= 0.f || clientH <= 0.f) {
        return false;
    }

    RectF mediabox = engine->PageMediabox(s->displayedDestPage);
    RectF region = s->lastRegion;
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

    RenderPageArgs args(s->displayedDestPage, zoom, 0, &region);
    RenderedBitmap* bmp = engine->RenderPage(args);
    if (!bmp) {
        return false;
    }
    delete s->bmp;
    s->bmp = bmp;
    InvalidateRect(s->hwndPopup, nullptr, TRUE);
    return true;
}

void RefHoverSchedule(RefHoverState* s, HWND hwndCanvas, Point screenPt, int destPage, float destX, float destY) {
    if (!s) {
        return;
    }
    KillTimer(hwndCanvas, kRefHoverTimerID);

    if (IsWindowVisible(s->hwndPopup) && s->displayedDestPage == destPage) {
        return;
    }
    s->pendingScreenPt = screenPt;
    s->pendingDestPage = destPage;
    s->pendingDestX = destX;
    s->pendingDestY = destY;
    SetTimer(hwndCanvas, kRefHoverTimerID, kRefHoverDelayMs, nullptr);
}

void RefHoverHide(RefHoverState* s, HWND hwndCanvas) {
    if (!s) {
        return;
    }
    KillTimer(hwndCanvas, kRefHoverTimerID);
    s->pendingDestPage = -1;
    if (s->hwndPopup && IsWindowVisible(s->hwndPopup)) {
        ShowWindow(s->hwndPopup, SW_HIDE);
        s->displayedDestPage = -1;
    }
}

// Wide strip from destX to the right page margin — used only when detection
// found a small box that looks like a figure caption number, so that the
// popup captures the surrounding figure + caption.
static RectF FigureStripBox(RectF mediabox, float destX, float destY, float heightPt) {
    float lx = (destX >= 0.f) ? destX - 12.f : 0.f;
    if (lx < 0.f) {
        lx = 0.f;
    }
    float ty = (destY >= 0.f) ? destY - kAnchorTopMarginPt : 0.f;
    if (ty < 0.f) {
        ty = 0.f;
    }
    float w = mediabox.dx - lx - kRightMarginPt;
    if (w < 100.f) {
        lx = 0.f;
        w = mediabox.dx;
    }
    if (ty + heightPt > mediabox.dy) {
        heightPt = mediabox.dy - ty;
        if (heightPt < 0.f) {
            heightPt = mediabox.dy;
            ty = 0.f;
        }
    }
    return RectF{lx, ty, w, heightPt};
}

// Used when the link doesn't resolve to a recognizable bibliography entry —
// TOC targets, topbar/section links, table or figure captions, image-only
// PDFs. Returns a landscape view of the page (full page width × half page
// height) anchored at destY, so the popup shows enough surrounding context
// for the user to recognize where the link points (e.g. the table rows
// below a caption, the section content below a heading).
static RectF LandscapeBox(RectF mediabox, float destX, float destY) {
    float w = mediabox.dx;
    float h = mediabox.dy / 2.f;
    if (h > mediabox.dy) {
        h = mediabox.dy;
    }
    float lx = 0.f;
    float ty = (destY >= 0.f) ? destY - kAnchorTopMarginPt : 0.f;
    if (ty < 0.f) {
        ty = 0.f;
    }
    if (ty + h > mediabox.dy) {
        ty = mediabox.dy - h;
    }
    if (ty < 0.f) {
        ty = 0.f;
    }
    return RectF{lx, ty, w, h};
}

// Find the bounding box of a single bibliography entry on the destination
// page. Uses per-glyph text+coords from the engine's text cache:
//   1. Locate the leftmost glyph with y in a small band around destY (entry start).
//   2. Scan forward; stop at "[N" near the same left margin (next entry) or
//      a vertical paragraph gap.
//   3. Return the min/max bounding box of glyphs in [start, end), padded.
// Falls back to LandscapeBox() when the link is not a bibliography reference
// (TOC, topbar, cross-ref, table caption). The landscape box renders a half-
// page-tall slice of the page anchored on the destination so the user sees
// surrounding context (e.g. the table rows under a caption).
static RectF DetectEntryBox(EngineBase* engine, int destPage, float destX, float destY) {
    RectF mediabox = engine->PageMediabox(destPage);
    if (destY < 0.f) {
        return LandscapeBox(mediabox, destX, destY);
    }

    int textLen = 0;
    Rect* coords = nullptr;
    const WCHAR* text = engine->GetTextForPage(destPage, &textLen, &coords);
    if (!text || textLen <= 0 || !coords) {
        return LandscapeBox(mediabox, destX, destY);
    }

    int dY = (int)destY;
    int dX = (int)destX;
    // Constrain to the destination's column — for 2-column layouts this
    // prevents the search from latching onto same-Y body text in another
    // column. We allow a small left tolerance so a "[1]" whose [ starts
    // a few pt left of destX still matches.
    int columnLeft = (destX >= 0.f) ? dX - 15 : INT_MIN;

    // 1. Find the start glyph: top-left non-whitespace glyph with
    //    y in [destY-5, destY+30] and x at-or-right-of columnLeft.
    int startIdx = -1;
    int bestY = INT_MAX;
    int bestX = INT_MAX;
    for (int i = 0; i < textLen; i++) {
        WCHAR c = text[i];
        if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
            continue;
        }
        Rect r = coords[i];
        if (r.y < dY - 5 || r.y > dY + 30) {
            continue;
        }
        if (r.x < columnLeft) {
            continue;
        }
        if (r.y < bestY || (r.y == bestY && r.x < bestX)) {
            bestY = r.y;
            bestX = r.x;
            startIdx = i;
        }
    }
    if (startIdx < 0) {
        return LandscapeBox(mediabox, destX, destY);
    }

    int firstLineLeftX = coords[startIdx].x;
    int firstLineY = coords[startIdx].y;
    int firstLineDy = coords[startIdx].dy;
    if (firstLineDy <= 0) {
        firstLineDy = 12;
    }

    // 2. Scan forward to find the end of the entry.
    int endIdx = textLen;
    int prevY = firstLineY;
    int prevBottom = firstLineY + firstLineDy;
    int lineHeight = firstLineDy;

    // Track leftmost X on the current line vs the previous line so we can
    // detect indent changes (the most reliable signal for author-year bibs).
    int currentLineLeftX = firstLineLeftX;
    int prevLineLeftX = INT_MAX;
    // X of the entry's continuation lines (captured from line 2). -1 = unknown.
    int indentX = -1;

    for (int i = startIdx + 1; i < textLen; i++) {
        WCHAR c = text[i];
        if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
            continue;
        }
        Rect r = coords[i];

        // Stop on column wrap: y goes significantly above the current row.
        if (r.y < firstLineY - 5) {
            endIdx = i;
            break;
        }
        // Skip glyphs in other columns (left of the entry's column).
        if (r.x < firstLineLeftX - 20) {
            continue;
        }

        bool isNewLine = (r.y > prevY + 2);
        if (isNewLine) {
            prevLineLeftX = currentLineLeftX;
            currentLineLeftX = r.x;
        } else if (r.x < currentLineLeftX) {
            currentLineLeftX = r.x;
        }

        bool pastFirstLine = (r.y > firstLineY + firstLineDy * 3 / 4 + 2);
        bool atFirstLineLeftX = (r.x >= firstLineLeftX - 5 && r.x <= firstLineLeftX + 5);

        // Capture the continuation X from the entry's second line.
        if (isNewLine && pastFirstLine && indentX < 0 && !atFirstLineLeftX) {
            indentX = r.x;
        }

        // (a) "[" at the entry's first-line X = next entry marker. Works for
        // both numeric "[123]" and alphanumeric "[Foo+09]" / "[Bib05]" styles
        // — body-text "[…]" can't trigger this because body sits at indentX,
        // not firstLineLeftX.
        if (c == L'[' && atFirstLineLeftX) {
            endIdx = i;
            break;
        }

        // (b) Indent change: a new line back at the entry's first-line X
        // after a continuation line at a different X. Catches author-year
        // hanging-indent bibliographies where there's no [N] marker — this
        // is the primary signal for the *next* entry's start.
        if (isNewLine && atFirstLineLeftX && pastFirstLine && prevLineLeftX != INT_MAX &&
            (prevLineLeftX < firstLineLeftX - 5 || prevLineLeftX > firstLineLeftX + 5)) {
            endIdx = i;
            break;
        }

        // (c) Vertical paragraph break (no-indent style fallback).
        if (r.y > prevBottom + lineHeight * 5 / 4) {
            endIdx = i;
            break;
        }

        // (d) Single-line-entry case: a new line back at firstLineLeftX before
        // we discovered a continuation indent. The previous "entry" was one
        // line. Common pattern: stacked numbered footnotes "¹url\n²url\n³url".
        if (isNewLine && pastFirstLine && atFirstLineLeftX && indentX < 0 && prevLineLeftX != INT_MAX) {
            endIdx = i;
            break;
        }

        // Track current line height as we go (catches changing leading).
        if (isNewLine) {
            int dy = r.y - prevY;
            if (dy > 4 && dy < 60) {
                lineHeight = dy;
            }
            prevY = r.y;
            prevBottom = r.y + r.dy;
        }
    }

    // 3. Compute bounding box of glyphs in [startIdx, endIdx).
    int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;
    for (int i = startIdx; i < endIdx; i++) {
        WCHAR c = text[i];
        if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
            continue;
        }
        Rect r = coords[i];
        // Exclude glyphs that aren't in the entry's column.
        if (r.x < firstLineLeftX - 20) {
            continue;
        }
        if (r.y < firstLineY - 5) {
            continue;
        }
        if (r.x < minX) {
            minX = r.x;
        }
        if (r.y < minY) {
            minY = r.y;
        }
        if (r.x + r.dx > maxX) {
            maxX = r.x + r.dx;
        }
        if (r.y + r.dy > maxY) {
            maxY = r.y + r.dy;
        }
    }
    if (minX == INT_MAX) {
        return LandscapeBox(mediabox, destX, destY);
    }

    RectF box{(float)minX - kEntryPadPt, (float)minY - kEntryPadPt, (float)(maxX - minX) + 2.f * kEntryPadPt,
              (float)(maxY - minY) + 2.f * kEntryPadPt};
    if (box.x < 0.f) {
        box.dx += box.x;
        box.x = 0.f;
    }
    if (box.y < 0.f) {
        box.dy += box.y;
        box.y = 0.f;
    }
    if (box.x + box.dx > mediabox.dx) {
        box.dx = mediabox.dx - box.x;
    }
    if (box.y + box.dy > mediabox.dy) {
        box.dy = mediabox.dy - box.y;
    }
    if (box.dx < 50.f || box.dy < 20.f) {
        return LandscapeBox(mediabox, destX, destY);
    }
    // Single-line detected entries with no continuation indent are usually
    // not bibliography references — could be a table/figure caption, a
    // section heading, or an in-text cross-ref destination. Show the
    // landscape view so the user sees what's below the caption (the table,
    // the section content, etc.) rather than just the caption text itself.
    // Exception: an entry starting with "[" is a description-list-style
    // citation marker ("[Nyg11]", "[1]", …) — those are real references
    // even when single-line, keep their fitted box.
    if (box.dy < kSingleLinePt && indentX < 0 && text[startIdx] != L'[') {
        return LandscapeBox(mediabox, destX, destY);
    }
    // If detection produced a small box (text-only scan got trapped in one
    // fragment of a figure / diagram), expand to a generous strip so the
    // surrounding visual content is included in the render.
    if (box.dx < 150.f && box.dy < 60.f) {
        return FigureStripBox(mediabox, destX, destY, kFigureStripHeightPt);
    }
    return box;
}

void RefHoverOnTimer(RefHoverState* s, HWND hwndCanvas, EngineBase* engine, float pageZoom) {
    KillTimer(hwndCanvas, kRefHoverTimerID);
    if (!s || !engine || s->pendingDestPage <= 0) {
        return;
    }
    int destPage = s->pendingDestPage;
    float destX = s->pendingDestX;
    float destY = s->pendingDestY;

    RectF mediabox = engine->PageMediabox(destPage);
    if (mediabox.dx <= 0.f || mediabox.dy <= 0.f) {
        return;
    }

    RectF region = DetectEntryBox(engine, destPage, destX, destY);
    // New destination — reset user-driven zoom. baseZoom matches the
    // document's current display zoom for the destination page, so popup
    // text height is comparable to the visible page text. Shrink baseZoom
    // if either dimension would exceed the popup max; landscape-style
    // regions for non-reference targets are typically wider than tall, so
    // the width cap matters here.
    s->userZoom = 1.f;
    float baseZoom = (pageZoom > 0.f) ? pageZoom : kRenderZoom;
    float availH = (float)(kMaxPopupHeight - 2 * kBorder);
    float availW = (float)(kMaxPopupWidth - 2 * kBorder);
    if (region.dy > 0.f && region.dy * baseZoom > availH) {
        baseZoom = availH / region.dy;
    }
    if (region.dx > 0.f && region.dx * baseZoom > availW) {
        baseZoom = availW / region.dx;
    }
    if (baseZoom < kMinUserZoom) {
        baseZoom = kMinUserZoom;
    }
    s->baseZoom = baseZoom;
    RenderPageArgs args(destPage, s->baseZoom * s->userZoom, 0, &region);
    RenderedBitmap* bmp = engine->RenderPage(args);
    if (!bmp) {
        return;
    }

    delete s->bmp;
    s->bmp = bmp;
    s->displayedDestPage = destPage;
    s->lastRegion = region;

    ShowPopup(s, s->pendingScreenPt);
}
