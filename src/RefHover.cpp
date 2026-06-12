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
    constexpr int kCursorPad = 30;
    int spaceBelow = bottomBound - (screenPt.y + kCursorPad);
    int spaceAbove = (screenPt.y - kCursorPad) - topBound;
    int y;
    if (spaceBelow >= popupH) {
        y = screenPt.y + kCursorPad;
    } else if (spaceAbove >= popupH) {
        y = screenPt.y - popupH - kCursorPad;
    } else if (spaceBelow >= spaceAbove) {
        if (spaceBelow > 0) {
            popupH = spaceBelow;
        }
        y = screenPt.y + kCursorPad;
    } else {
        if (spaceAbove > 0) {
            popupH = spaceAbove;
        }
        y = screenPt.y - popupH - kCursorPad;
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
    float clientW = (float)((rc.right - rc.left) - 2 * kBorder);
    float clientH = (float)((rc.bottom - rc.top) - 2 * kBorder);
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

    RenderPageArgs args(s->displayed.destPage, zoom, 0, &region);
    RenderedBitmap* bmp = engine->RenderPage(args);
    if (!bmp) {
        return false;
    }
    delete s->bmp;
    s->bmp = bmp;
    // Persist the popup-fitting dx/dy so a subsequent scroll keeps rendering
    // a bitmap that fills the popup. Without this, scroll would fall back to
    // the original (smaller) entry-box dimensions and leave visible padding.
    s->displayed.region = region;
    InvalidateRect(s->hwndPopup, nullptr, TRUE);
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
    float scrollPt = 60.f * ((float)wheelDelta / (float)WHEEL_DELTA) / zoom;
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

    RenderPageArgs args(page, zoom, 0, &region);
    RenderedBitmap* bmp = engine->RenderPage(args);
    if (!bmp) {
        return false;
    }
    delete s->bmp;
    s->bmp = bmp;
    s->displayed.destPage = page;
    s->displayed.region = region;
    InvalidateRect(s->hwndPopup, nullptr, TRUE);
    return true;
}

void RefHoverSchedule(RefHoverState* s, HWND hwndCanvas, Point screenPt, int destPage, float destX, float destY,
                      float destZoom, int srcPage, RectF srcRect, Rect pageScreenRect) {
    if (!s) {
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
    SetTimer(hwndCanvas, kRefHoverTimerID, kRefHoverDelayMs, nullptr);
}

void RefHoverHide(RefHoverState* s, HWND hwndCanvas) {
    if (!s) {
        return;
    }
    KillTimer(hwndCanvas, kRefHoverTimerID);
    s->pending.destPage = -1;
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
    int popupWCap = kMaxPopupWidth;
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
    if (region.dy > 250.f && s->pending.pageScreenRect.dy > 0) {
        Rect pr = s->pending.pageScreenRect;
        int curY = s->pending.screenPt.y;
        int spaceAbove = curY - pr.y - 30;
        int spaceBelow = (pr.y + pr.dy) - curY - 30;
        int maxSpace = (spaceAbove > spaceBelow) ? spaceAbove : spaceBelow;
        if (maxSpace < 0) {
            maxSpace = 0;
        }
        int pageBased = pr.dy * 75 / 100;
        popupHCap = (pageBased > maxSpace) ? pageBased : maxSpace;
    } else {
        popupHCap = kMaxPopupHeight;
        if (s->pending.pageScreenRect.dy > 0) {
            int pageBased = s->pending.pageScreenRect.dy * 45 / 100;
            if (pageBased < popupHCap) {
                popupHCap = pageBased;
            }
        }
    }
    float availH = (float)(popupHCap - 2 * kBorder);
    float availW = (float)(popupWCap - 2 * kBorder);
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
    RenderPageArgs args(destPage, s->displayed.baseZoom * s->displayed.userZoom, 0, &region);
    RenderedBitmap* bmp = engine->RenderPage(args);
    if (!bmp) {
        return;
    }

    delete s->bmp;
    s->bmp = bmp;
    s->displayed.destPage = destPage;
    s->displayed.destX = destX;
    s->displayed.destY = destY;
    s->displayed.region = region;

    ShowPopup(s, s->pending.screenPt);
}
