/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
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
//   [ ] Mouse-wheel over popup zooms in / out.
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

#define REF_HOVER_CLASS L"SumatraPDFRefHover"

static constexpr float kAnchorTopMarginPt = 6.f;
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
    Rect pr = s->pendingPageScreenRect;
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

void RefHoverSchedule(RefHoverState* s, HWND hwndCanvas, Point screenPt, int destPage, float destX, float destY,
                      int srcPage, RectF srcRect, Rect pageScreenRect) {
    if (!s) {
        return;
    }
    KillTimer(hwndCanvas, kRefHoverTimerID);

    if (IsWindowVisible(s->hwndPopup) && s->displayedDestPage == destPage && s->displayedDestX == destX &&
        s->displayedDestY == destY) {
        return;
    }
    s->pendingScreenPt = screenPt;
    s->pendingDestPage = destPage;
    s->pendingDestX = destX;
    s->pendingDestY = destY;
    s->pendingSrcPage = srcPage;
    s->pendingSrcRect = srcRect;
    s->pendingPageScreenRect = pageScreenRect;
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

// Used when the link doesn't resolve to a recognizable bibliography entry —
// TOC targets, topbar/section links, table or figure captions, image-only
// PDFs. Returns a region that spans the full page width and goes from the
// destination Y down to the bottom of the last text glyph on the page —
// captures table caption / figure caption / section content below the
// link target without leaving a long blank margin at the popup bottom.
// Auto-fit in RefHoverOnTimer + the monitor-based popup height cap keep
// the popup a sensible size; the user can wheel-zoom in if text is too
// small.
static RectF LandscapeBox(RectF mediabox, float destX, float destY, const WCHAR* text, const Rect* coords,
                          int textLen) {
    float ty = (destY >= 0.f) ? destY - kAnchorTopMarginPt : 0.f;
    if (ty < 0.f) {
        ty = 0.f;
    }
    float h = mediabox.dy - ty;
    if (h <= 0.f) {
        h = mediabox.dy;
        ty = 0.f;
    }
    // Cap to a focused region size so the popup is wide and short rather
    // than narrow and tall.
    constexpr float kMaxLandscapePt = 200.f;
    if (h > kMaxLandscapePt) {
        h = kMaxLandscapePt;
    }
    // Caption extension: if a "Figure N.M" / "Table N.M" / "Listing N.M" /
    // "Algorithm N.M" caption appears within ~250pt below the capped region
    // bottom (typical figure body height), extend the region downward to
    // include the full caption block. Necessary for image-only figures
    // where the figure body has no extractable text at destY — the caller
    // falls to LandscapeBox without ever running the caption-aware
    // DetectEntryBox path.
    if (text && coords && textLen > 0) {
        auto isCaptionAt = [&](int idx) -> bool {
            if (idx > 0) {
                WCHAR prev = text[idx - 1];
                bool prevAlnum = (prev >= L'a' && prev <= L'z') || (prev >= L'A' && prev <= L'Z') ||
                                 (prev >= L'0' && prev <= L'9');
                if (prevAlnum) {
                    return false;
                }
            }
            auto matchWord = [&](const WCHAR* w, int n) -> bool {
                if (idx + n + 1 >= textLen) {
                    return false;
                }
                for (int j = 0; j < n; j++) {
                    WCHAR c = text[idx + j];
                    if (c >= L'A' && c <= L'Z') {
                        c = (WCHAR)(c + 32);
                    }
                    if (c != w[j]) {
                        return false;
                    }
                }
                int k = idx + n;
                while (k < textLen && (text[k] == L' ' || text[k] == L'\t')) {
                    k++;
                }
                return k < textLen && text[k] >= L'0' && text[k] <= L'9';
            };
            return matchWord(L"figure", 6) || matchWord(L"table", 5) || matchWord(L"listing", 7) ||
                   matchWord(L"algorithm", 9);
        };
        // Search to end of page so tall figures with captions far below the
        // initial 200pt cap still match. First "Figure N.M" line-start on the
        // page below the cap wins — typically the relevant caption.
        int searchTop = (int)(ty + h);
        int searchBot = (int)mediabox.dy;
        int capStartIdx = -1;
        for (int i = 0; i < textLen; i++) {
            if (coords[i].y < searchTop || coords[i].y > searchBot) {
                continue;
            }
            if (isCaptionAt(i)) {
                capStartIdx = i;
                break;
            }
        }
        if (capStartIdx >= 0) {
            int capStartY = coords[capStartIdx].y;
            int capLineH = coords[capStartIdx].dy;
            if (capLineH < 10) {
                capLineH = 12;
            }
            int lineSpacing = capLineH * 14 / 10;
            if (lineSpacing < 14) {
                lineSpacing = 14;
            }
            // Page right text margin: max right-X across all text glyphs on
            // the page. Justified body lines (filling the text column) end
            // within a few pt of this; LaTeX-style captions (raggedright)
            // typically don't reach it. Used below to distinguish caption
            // continuation from a justified body paragraph that follows.
            int pageRightX = 0;
            for (int j = 0; j < textLen; j++) {
                int rx = coords[j].x + coords[j].dx;
                if (rx > pageRightX) {
                    pageRightX = rx;
                }
            }
            // Scan line by line from capStartY. Always accept line 0
            // (the caption start itself). For each subsequent line within
            // capStartY + 3·lineSpacing, stop if its right edge reaches
            // the page right margin (= justified body paragraph). Captions
            // up to 3 lines are accepted as long as no line is justified.
            int captionEndY = capStartY + capLineH;
            for (int lineIdx = 0; lineIdx < 3; lineIdx++) {
                int expectedY = capStartY + lineIdx * lineSpacing;
                int rangeTop = expectedY - 3;
                int rangeBot = expectedY + 3;
                bool foundLine = false;
                int lineBottomY = expectedY + capLineH;
                int lineRightX = 0;
                for (int j = 0; j < textLen; j++) {
                    int gy = coords[j].y;
                    if (gy < rangeTop || gy > rangeBot) {
                        continue;
                    }
                    foundLine = true;
                    int gb = gy + coords[j].dy;
                    if (gb > lineBottomY) {
                        lineBottomY = gb;
                    }
                    int rx = coords[j].x + coords[j].dx;
                    if (rx > lineRightX) {
                        lineRightX = rx;
                    }
                }
                if (!foundLine) {
                    break;
                }
                if (lineIdx > 0 && lineRightX > pageRightX - 30) {
                    // Justified body line — stop before extending region
                    // into the next paragraph.
                    break;
                }
                captionEndY = lineBottomY;
            }
            float extendedH = (float)captionEndY + kAnchorTopMarginPt - ty;
            if (extendedH > h) {
                h = extendedH;
            }
        }
    }
    // Trim trailing blank margin: find the bottom of the last text glyph
    // inside the candidate region and end the region just below it so the
    // popup doesn't render an empty trailing margin.
    if (text && coords && textLen > 0) {
        int boxTop = (int)ty;
        int boxBottom = (int)(ty + h);
        int lastTextBottom = boxTop;
        for (int i = 0; i < textLen; i++) {
            WCHAR c = text[i];
            if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
                continue;
            }
            Rect r = coords[i];
            if (r.y < boxTop || r.y >= boxBottom) {
                continue;
            }
            int glyphBottom = r.y + r.dy;
            if (glyphBottom > lastTextBottom) {
                lastTextBottom = glyphBottom;
            }
        }
        float trimmedH = (float)lastTextBottom + kAnchorTopMarginPt - ty;
        if (trimmedH > 20.f && trimmedH < h) {
            h = trimmedH;
        }
    }
    return RectF{0.f, ty, mediabox.dx, h};
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
    int textLen = 0;
    Rect* coords = nullptr;
    const WCHAR* text = engine->GetTextForPage(destPage, &textLen, &coords);
    if (destY < 0.f) {
        return LandscapeBox(mediabox, destX, destY, text, coords, textLen);
    }
    if (!text || textLen <= 0 || !coords) {
        return LandscapeBox(mediabox, destX, destY, text, coords, textLen);
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
        return LandscapeBox(mediabox, destX, destY, text, coords, textLen);
    }

    // PDF link destX is unreliable: poorly-authored links carry the source
    // page's body-text X, not the destination-page entry-start X. That lands
    // startIdx mid-line on hanging-indent description-list bibs, dropping
    // the leading "[KOS06]" / "Philippe Kruchten" portion from the popup.
    // Walk to the leftmost glyph on the same line as startIdx so the entry
    // bounds always include the line's left edge.
    {
        int sy = coords[startIdx].y;
        int leftmostX = coords[startIdx].x;
        int leftmostIdx = startIdx;
        for (int i = 0; i < textLen; i++) {
            WCHAR c = text[i];
            if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
                continue;
            }
            Rect r = coords[i];
            if (r.y < sy - 3 || r.y > sy + 3) {
                continue;
            }
            if (r.x < leftmostX) {
                leftmostX = r.x;
                leftmostIdx = i;
            }
        }
        startIdx = leftmostIdx;
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
    // Set when we observe another sibling entry start at firstLineLeftX with
    // no continuation indent in between — strong "this is a description list"
    // signal (e.g. "JVM Java Virtual Machine. 19, 36" / "LLM Large Language
    // Model. 45" abbreviation lists) that survives even when the current
    // entry is a single line.
    bool descListSibling = false;

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
            descListSibling = true;
            endIdx = i;
            break;
        }

        // (b) Indent change: a new line back at the entry's first-line X
        // after a continuation line at a different X. Catches author-year
        // hanging-indent bibliographies where there's no [N] marker — this
        // is the primary signal for the *next* entry's start.
        if (isNewLine && atFirstLineLeftX && pastFirstLine && prevLineLeftX != INT_MAX &&
            (prevLineLeftX < firstLineLeftX - 5 || prevLineLeftX > firstLineLeftX + 5)) {
            descListSibling = true;
            endIdx = i;
            break;
        }

        // (c) Vertical paragraph break (no-indent style fallback). When the
        // glyph that triggered the gap is back at firstLineLeftX, the gap
        // is a blank line between description-list siblings (typical
        // abbreviation lists where each entry is separated by extra
        // vertical space) — treat as a sibling entry boundary.
        if (r.y > prevBottom + lineHeight * 5 / 4) {
            if (atFirstLineLeftX) {
                descListSibling = true;
            }
            endIdx = i;
            break;
        }

        // (d) Single-line-entry case: a new line back at firstLineLeftX before
        // we discovered a continuation indent. The previous "entry" was one
        // line. Common pattern: stacked numbered footnotes "¹url\n²url\n³url"
        // or abbreviation lists ("JVM Java Virtual Machine. 19, 36").
        if (isNewLine && pastFirstLine && atFirstLineLeftX && indentX < 0 && prevLineLeftX != INT_MAX) {
            descListSibling = true;
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
        return LandscapeBox(mediabox, destX, destY, text, coords, textLen);
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
        return LandscapeBox(mediabox, destX, destY, text, coords, textLen);
    }
    // "Figure N.M" / "Table N.M" / "Listing N.M" / "Algorithm N.M" caption
    // anywhere below the detected box: the destination is a figure / table
    // / listing body. Override all other heuristics so the popup uses the
    // landscape view (caption included). Catches code/console listings
    // where each line happens to start with "[TAG]" — those would otherwise
    // be misclassified as description-list bibliography entries.
    {
        auto isCaptionLabelAt = [&](int idx) -> bool {
            if (idx > 0) {
                WCHAR prev = text[idx - 1];
                bool prevAlnum = (prev >= L'a' && prev <= L'z') || (prev >= L'A' && prev <= L'Z') ||
                                 (prev >= L'0' && prev <= L'9');
                if (prevAlnum) {
                    return false;
                }
            }
            auto matchWord = [&](const WCHAR* w, int n) -> bool {
                if (idx + n + 1 >= textLen) {
                    return false;
                }
                for (int j = 0; j < n; j++) {
                    WCHAR c = text[idx + j];
                    if (c >= L'A' && c <= L'Z') {
                        c = (WCHAR)(c + 32);
                    }
                    if (c != w[j]) {
                        return false;
                    }
                }
                int k = idx + n;
                while (k < textLen && (text[k] == L' ' || text[k] == L'\t')) {
                    k++;
                }
                return k < textLen && text[k] >= L'0' && text[k] <= L'9';
            };
            return matchWord(L"figure", 6) || matchWord(L"table", 5) || matchWord(L"listing", 7) ||
                   matchWord(L"algorithm", 9);
        };
        int boxBottomY = (int)(box.y + box.dy);
        for (int i = 0; i < textLen; i++) {
            if (coords[i].y <= boxBottomY) {
                continue;
            }
            if (isCaptionLabelAt(i)) {
                // Let LandscapeBox handle the caption-extension — it has a
                // tighter, line-count-capped walk that doesn't sweep into
                // following body paragraphs.
                return LandscapeBox(mediabox, destX, destY, text, coords, textLen);
            }
        }
    }
    // Description-list bibliography ("[Smith2020]", "[1]", …) — unambiguous,
    // keep the fitted box.
    if (text[startIdx] == L'[') {
        return box;
    }
    // Tabular layout: continuation X far right of firstLineLeftX is a
    // column gap, not a hanging indent. Detection terminated at the first
    // data row; show the landscape view so the user sees the full table.
    if (indentX > 0 && (indentX - firstLineLeftX) > 80) {
        return LandscapeBox(mediabox, destX, destY, text, coords, textLen);
    }
    // Section heading or caption-style label. Body paragraph below the
    // heading has first-line indent, so detection captures heading + body
    // line 1 and `indentX` lands in the same range as a hanging-indent bib.
    // Use the entry's first character / first word to disambiguate: real
    // bibliographies rarely start with a digit or with a label word like
    // "Figure"/"Table"/"Section". Catches "6.2 Foo", "Figure 2.2: …", etc.
    auto matchPrefix = [&](const WCHAR* word, int n) {
        if (startIdx + n >= textLen) {
            return false;
        }
        for (int j = 0; j < n; j++) {
            WCHAR c = text[startIdx + j];
            if (c >= L'A' && c <= L'Z') {
                c = (WCHAR)(c + 32);
            }
            if (c != word[j]) {
                return false;
            }
        }
        return true;
    };
    WCHAR firstC = text[startIdx];
    bool digitStart = (firstC >= L'0' && firstC <= L'9');
    bool labelStart = matchPrefix(L"figure", 6) || matchPrefix(L"table", 5) ||
                      matchPrefix(L"section", 7) || matchPrefix(L"chapter", 7) ||
                      matchPrefix(L"algorithm", 9);
    if (digitStart || labelStart) {
        return LandscapeBox(mediabox, destX, destY, text, coords, textLen);
    }
    // Code-listing detector: a high density of braces / semicolons / parens
    // within the detected box means the destination is most likely a code
    // listing presented as a figure. Bibliography prose almost never has
    // these characters at this density. Show the landscape view so the
    // popup also includes the figure caption below the code.
    {
        int codeChars = 0;
        int totalChars = 0;
        for (int i = startIdx; i < endIdx; i++) {
            WCHAR c = text[i];
            if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
                continue;
            }
            totalChars++;
            if (c == L'{' || c == L'}' || c == L';' || c == L'(' || c == L')') {
                codeChars++;
            }
        }
        if (totalChars > 50 && codeChars * 12 > totalChars) {
            return LandscapeBox(mediabox, destX, destY, text, coords, textLen);
        }
    }
    // Description-list / glossary / footnote-style entry: rule (a) or (d)
    // fired, meaning we saw a *sibling* entry start at firstLineLeftX. That
    // is a strong "this is a list of entries" signal even when the current
    // entry is a single line (abbreviations: "JVM Java Virtual Machine.").
    if (descListSibling) {
        return box;
    }
    // Single-line entry with no continuation indent and no sibling entry
    // detected — caption / heading / in-text cross-ref destination.
    if (box.dy < 30.f && indentX < 0) {
        return LandscapeBox(mediabox, destX, destY, text, coords, textLen);
    }
    // Default: looks like a multi-line author-year bibliography entry,
    // keep the fitted box.
    return box;
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

    // Pick the best alphanumeric run from the source rect as the search
    // needle. Strips surrounding punctuation ("(AKM)" → "AKM"). Tokens
    // flanked by parentheses (the typical "definition" convention for
    // expanding a phrase, e.g. "Architectural Knowledge Management (AKM)")
    // win over equally / longer-but-non-flanked tokens — that keeps the
    // resolver from latching onto a citation key like "KLV06" inside
    // "[KLV06]" when both appear near the link rect.
    int bestStart = -1;
    int bestLen = 0;
    bool bestIsParens = false;
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
            if (curLen >= 2) {
                bool flanked = (curStart > 0 && rawText[curStart - 1] == L'(' && i < rawLen && rawText[i] == L')');
                bool take = false;
                if (flanked && !bestIsParens) {
                    take = true;
                } else if (flanked == bestIsParens && curLen > bestLen) {
                    take = true;
                }
                if (take) {
                    bestStart = curStart;
                    bestLen = curLen;
                    bestIsParens = flanked;
                }
            }
            curStart = -1;
            curLen = 0;
        }
    }
    if (bestLen < 2) {
        return -1.f;
    }

    int destLen = 0;
    Rect* destCoords = nullptr;
    const WCHAR* destText = engine->GetTextForPage(destPage, &destLen, &destCoords);
    if (!destText || destLen <= 0 || !destCoords) {
        return -1.f;
    }
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
    return (bestY >= 0) ? (float)bestY : -1.f;
}

void RefHoverOnTimer(RefHoverState* s, HWND hwndCanvas, EngineBase* engine, float pageZoom) {
    KillTimer(hwndCanvas, kRefHoverTimerID);
    if (!s || !engine || s->pendingDestPage <= 0) {
        return;
    }
    int destPage = s->pendingDestPage;
    float destX = s->pendingDestX;
    float destY = s->pendingDestY;
    // PageDestGetDestPoint returns {0,0,0,0} when the link has no specific
    // anchor (page-level destination) — that's the typical case for body-text
    // abbreviation / glossary links and for some TOC-derived bib refs. In
    // those cases destY == 0 (not < 0), so we have to treat <= 0 as "no
    // anchor" and try to recover a specific Y from the source link's text.
    if (destY <= 0.f) {
        float resolved = ResolveDestYFromSourceText(engine, s->pendingSrcPage, s->pendingSrcRect, destPage);
        if (resolved >= 0.f) {
            destY = resolved;
            if (destX < 0.f) {
                destX = 0.f;
            }
        }
    }

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
        POINT mp = {s->pendingScreenPt.x, s->pendingScreenPt.y};
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
    if (region.dy > 250.f && s->pendingPageScreenRect.dy > 0) {
        Rect pr = s->pendingPageScreenRect;
        int curY = s->pendingScreenPt.y;
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
        if (s->pendingPageScreenRect.dy > 0) {
            int pageBased = s->pendingPageScreenRect.dy * 45 / 100;
            if (pageBased < popupHCap) {
                popupHCap = pageBased;
            }
        }
    }
    float availH = (float)(popupHCap - 2 * kBorder);
    float availW = (float)(popupWCap - 2 * kBorder);
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
    s->displayedDestX = destX;
    s->displayedDestY = destY;
    s->lastRegion = region;

    ShowPopup(s, s->pendingScreenPt);
}
