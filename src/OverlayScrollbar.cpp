/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/Dpi.h"
#include "base/Win.h"

#include "OverlayScrollbar.h"
#include "Theme.h"

#define OVERLAY_SCROLLBAR_CLASS L"SUMATRA_OVERLAY_SCROLLBAR"

static bool gScrollbarClassRegistered = false;

bool gOverlayScrollbarSuppressThick = false;
// if true, draw small filled triangles instead of chevrons
static bool gThickArrows = true;

// all live overlay scrollbars, for global mouse tracking
static Vec<OverlayScrollbar*> gAllScrollbars;
static UINT_PTR gMouseTrackTimer = 0;
static Point gLastMousePos = {-1, -1};
static constexpr UINT_PTR kMouseTrackTimerID = 100;
static constexpr int kMouseTrackIntervalMs = 50;

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

static constexpr int kMinThumbSize = 20;
static constexpr u8 kAlphaThin = 180;
static constexpr u8 kAlphaThick = 220;

using State = OverlayScrollbar::State;

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

static int ScaledWidth(OverlayScrollbar* sb, bool thick) {
    return thick ? sb->thickWidth : sb->thinWidth;
}

static bool IsVert(OverlayScrollbar* sb) {
    return sb->type == OverlayScrollbar::Type::Vert;
}

// Get the track rect in client coords of the scrollbar window
static Rect GetTrackRect(OverlayScrollbar* sb) {
    Rect rc = HwndClientRect(sb->hwnd);
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

// Calculate thumb rect within the track
static Rect GetThumbRect(OverlayScrollbar* sb) {
    Rect track = GetTrackRect(sb);
    int range = sb->nMax - sb->nMin + 1;
    if (range <= 0 || (int)sb->nPage >= range) {
        return track;
    }

    int trackLen = IsVert(sb) ? track.dy : track.dx;
    int thumbLen = MulDiv(trackLen, (int)sb->nPage, range);
    thumbLen = std::max(thumbLen, DpiScale(kMinThumbSize));

    int scrollableTrack = trackLen - thumbLen;
    int scrollableRange = range - (int)sb->nPage;
    int pos = sb->isDragging ? sb->nTrackPos : sb->nPos;
    int thumbOffset = 0;
    if (scrollableRange > 0) {
        thumbOffset = MulDiv(pos - sb->nMin, scrollableTrack, scrollableRange);
    }
    thumbOffset = setMinMax(thumbOffset, 0, scrollableTrack);

    if (IsVert(sb)) {
        return {track.x, track.y + thumbOffset, track.dx, thumbLen};
    }
    return {track.x + thumbOffset, track.y, thumbLen, track.dy};
}

static Rect GetArrowTopRect(OverlayScrollbar* sb) {
    Rect rc = HwndClientRect(sb->hwnd);
    int arrowSize = IsVert(sb) ? rc.dx : rc.dy;
    if (IsVert(sb)) {
        return {0, 0, rc.dx, arrowSize};
    }
    return {0, 0, arrowSize, rc.dy};
}

static Rect GetArrowBottomRect(OverlayScrollbar* sb) {
    Rect rc = HwndClientRect(sb->hwnd);
    int arrowSize = IsVert(sb) ? rc.dx : rc.dy;
    if (IsVert(sb)) {
        return {0, rc.dy - arrowSize, rc.dx, arrowSize};
    }
    return {rc.dx - arrowSize, 0, arrowSize, rc.dy};
}

static void SendScrollMsg(OverlayScrollbar* sb, UINT scrollMsg, WPARAM wp) {
    SendMessageW(sb->hwndOwner, scrollMsg, wp, 0);
}

static UINT ScrollMsgForType(OverlayScrollbar* sb) {
    return IsVert(sb) ? WM_VSCROLL : WM_HSCROLL;
}

// The band the scrollbar occupies when thick, in screen coordinates. The mouse
// being in it is what turns a smart scrollbar thick, so it is always the thick
// width, whatever width the scrollbar is drawn at right now
static Rect GetScrollbarScreenRect(OverlayScrollbar* sb) {
    Rect ownerRc = HwndWindowRect(sb->hwndOwner);
    int scrollW = ScaledWidth(sb, true);
    if (IsVert(sb)) {
        return {ownerRc.x + ownerRc.dx - scrollW, ownerRc.y, scrollW, ownerRc.dy};
    }
    return {ownerRc.x, ownerRc.y + ownerRc.dy - scrollW, ownerRc.dx, scrollW};
}

// Check if hwnd is the same as or an ancestor of child
static bool IsOrIsParentOf(HWND hwnd, HWND child) {
    while (child) {
        if (child == hwnd) {
            return true;
        }
        child = GetParent(child);
    }
    return false;
}

// Update the layered window with the current appearance
static void PaintScrollbar(OverlayScrollbar* sb) {
    if (!sb->hwnd || !HwndIsVisible(sb->hwnd)) {
        return;
    }

    Rect wrc = HwndWindowRect(sb->hwnd);
    int w = wrc.dx;
    int h = wrc.dy;
    if (w <= 0 || h <= 0) {
        return;
    }

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) {
        ReleaseDC(nullptr, hdcScreen);
        return;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hbmp = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbmp || !bits) {
        if (hbmp) {
            DeleteObject(hbmp);
        }
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        return;
    }
    HBITMAP hbmpOld = (HBITMAP)SelectObject(hdcMem, hbmp);

    memset(bits, 0, (size_t)w * h * 4);

    bool thick = IsThick(sb);
    u8 alpha = kAlphaThin;
    if (thick) {
        // non-default themes define exact colors, so draw thick scrollbar fully opaque
        alpha = IsCurrentThemeDefault() ? kAlphaThick : 255;
    }

    auto fillRect = [&](Rect r, Color color) {
        DWORD pixel = PremultiplyPixel(color, alpha);
        DWORD* pixels = (DWORD*)bits;
        int x0 = std::max(r.x, 0);
        int y0 = std::max(r.y, 0);
        int x1 = std::min(r.x + r.dx, w);
        int y1 = std::min(r.y + r.dy, h);
        for (int y = y0; y < y1; y++) {
            for (int x = x0; x < x1; x++) {
                pixels[(y * w) + x] = pixel;
            }
        }
    };

    if (IsThick(sb)) {
        fillRect(Rect(0, 0, w, h), ThemeTrackColor());
    }

    Rect thumbRc = GetThumbRect(sb);
    Color thumbCol = sb->mouseOverThumb ? ThemeThumbHoverColor() : ThemeThumbColor();

    if (!IsThick(sb)) {
        int thinW = ScaledWidth(sb, false);
        if (IsVert(sb)) {
            thumbRc.x = (w - thinW) / 2;
            thumbRc.dx = thinW;
        } else {
            thumbRc.y = (h - thinW) / 2;
            thumbRc.dy = thinW;
        }
    }
    fillRect(thumbRc, thumbCol);

    if (IsThick(sb)) {
        Gdiplus::Graphics gfx(hdcMem);
        gfx.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

        Color arrowCol = ThemeThumbHoverColor();
        u8 ar = (u8)MulDiv(GetRValue(arrowCol), alpha, 255);
        u8 ag = (u8)MulDiv(GetGValue(arrowCol), alpha, 255);
        u8 ab = (u8)MulDiv(GetBValue(arrowCol), alpha, 255);
        Gdiplus::Color gdipArrowCol(alpha, ar, ag, ab);

        Rect arrowTop = GetArrowTopRect(sb);
        Rect arrowBot = GetArrowBottomRect(sb);

        if (gThickArrows) {
            // filled triangles (like Windows Terminal)
            Gdiplus::SolidBrush br(gdipArrowCol);
            if (IsVert(sb)) {
                float sz = (float)arrowTop.dx / 3.0f;
                // up triangle
                float cx = (float)(arrowTop.x + (arrowTop.dx / 2));
                float cy = (float)(arrowTop.y + (arrowTop.dy / 2));
                Gdiplus::PointF upPts[3] = {
                    {cx, cy - (sz * 0.7f)},
                    {cx - sz, cy + (sz * 0.7f)},
                    {cx + sz, cy + (sz * 0.7f)},
                };
                gfx.FillPolygon(&br, upPts, 3);
                // down triangle
                cx = (float)(arrowBot.x + (arrowBot.dx / 2));
                cy = (float)(arrowBot.y + (arrowBot.dy / 2));
                Gdiplus::PointF downPts[3] = {
                    {cx - sz, cy - (sz * 0.7f)},
                    {cx + sz, cy - (sz * 0.7f)},
                    {cx, cy + (sz * 0.7f)},
                };
                gfx.FillPolygon(&br, downPts, 3);
            } else {
                float sz = (float)arrowTop.dy / 3.0f;
                // left triangle
                float cx = (float)(arrowTop.x + (arrowTop.dx / 2));
                float cy = (float)(arrowTop.y + (arrowTop.dy / 2));
                Gdiplus::PointF leftPts[3] = {
                    {cx - (sz * 0.7f), cy},
                    {cx + (sz * 0.7f), cy - sz},
                    {cx + (sz * 0.7f), cy + sz},
                };
                gfx.FillPolygon(&br, leftPts, 3);
                // right triangle
                cx = (float)(arrowBot.x + (arrowBot.dx / 2));
                cy = (float)(arrowBot.y + (arrowBot.dy / 2));
                Gdiplus::PointF rightPts[3] = {
                    {cx - (sz * 0.7f), cy - sz},
                    {cx - (sz * 0.7f), cy + sz},
                    {cx + (sz * 0.7f), cy},
                };
                gfx.FillPolygon(&br, rightPts, 3);
            }
        } else {
            // chevron lines
            Gdiplus::Pen pen(gdipArrowCol, 1.5f);
            pen.SetStartCap(Gdiplus::LineCapRound);
            pen.SetEndCap(Gdiplus::LineCapRound);
            pen.SetLineJoin(Gdiplus::LineJoinRound);
            int inset = IsVert(sb) ? arrowTop.dx / 5 : arrowTop.dy / 5;

            if (IsVert(sb)) {
                int sz = arrowTop.dx / 5;
                float cx = (float)(arrowTop.x + (arrowTop.dx / 2));
                float cy = (float)(arrowTop.y + (arrowTop.dy / 2)) + ((float)inset / 2);
                Gdiplus::PointF upPts[3] = {
                    {cx - (float)sz, cy + ((float)sz / 2.0f)},
                    {cx, cy - ((float)sz / 2.0f)},
                    {cx + (float)sz, cy + ((float)sz / 2.0f)},
                };
                gfx.DrawLines(&pen, upPts, 3);

                cx = (float)(arrowBot.x + (arrowBot.dx / 2));
                cy = (float)(arrowBot.y + (arrowBot.dy / 2)) - ((float)inset / 2);
                Gdiplus::PointF downPts[3] = {
                    {cx - (float)sz, cy - ((float)sz / 2.0f)},
                    {cx, cy + ((float)sz / 2.0f)},
                    {cx + (float)sz, cy - ((float)sz / 2.0f)},
                };
                gfx.DrawLines(&pen, downPts, 3);
            } else {
                int sz = arrowTop.dy / 5;
                float cx = (float)(arrowTop.x + (arrowTop.dx / 2)) + ((float)inset / 2);
                float cy = (float)(arrowTop.y + (arrowTop.dy / 2));
                Gdiplus::PointF leftPts[3] = {
                    {cx + ((float)sz / 2.0f), cy - (float)sz},
                    {cx - ((float)sz / 2.0f), cy},
                    {cx + ((float)sz / 2.0f), cy + (float)sz},
                };
                gfx.DrawLines(&pen, leftPts, 3);

                cx = (float)(arrowBot.x + (arrowBot.dx / 2)) - ((float)inset / 2);
                cy = (float)(arrowBot.y + (arrowBot.dy / 2));
                Gdiplus::PointF rightPts[3] = {
                    {cx - ((float)sz / 2.0f), cy - (float)sz},
                    {cx + ((float)sz / 2.0f), cy},
                    {cx - ((float)sz / 2.0f), cy + (float)sz},
                };
                gfx.DrawLines(&pen, rightPts, 3);
            }
        }
    }

    POINT ptSrc = {0, 0};
    SIZE szWnd = {w, h};
    POINT ptDst = {wrc.x, wrc.y};
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(sb->hwnd, hdcScreen, &ptDst, &szWnd, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, hbmpOld);
    DeleteObject(hbmp);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
}

// Make the layered window fully transparent without hiding it.
// ShowWindow(SW_HIDE) can steal activation from other windows (e.g. command palette).
static void MakeLayeredWindowTransparent(HWND hwnd) {
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 0;
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(hwnd, nullptr, nullptr, nullptr, nullptr, nullptr, 0, &blend, ULW_ALPHA);
}

static void SetState(OverlayScrollbar* sb, State newState) {
    if (sb->state == newState) {
        return;
    }
    bool wasVisible = IsVisible(sb);
    sb->state = newState;
    bool nowVisible = IsVisible(sb);

    OverlayScrollbarUpdatePos(sb);
    if (nowVisible) {
        if (!wasVisible) {
            ShowWindow(sb->hwnd, SW_SHOWNOACTIVATE);
        }
        PaintScrollbar(sb);
    } else {
        // Make fully transparent instead of ShowWindow(SW_HIDE) because
        // SW_HIDE can trigger Z-order changes that hide other popups
        sb->mouseOverThumb = false;
        MakeLayeredWindowTransparent(sb->hwnd);
    }

    KillTimer(sb->hwnd, OverlayScrollbar::kTimerAutoHide);
    if (newState == State::SmartThin) {
        SetTimer(sb->hwnd, OverlayScrollbar::kTimerAutoHide, sb->showAfterScrollMs, nullptr);
    }
}

static void ShowScrollbarWindow(OverlayScrollbar* sb, bool thick) {
    // Don't revert to thin while user is dragging the thumb
    if (sb->isDragging && !thick) {
        return;
    }
    if (sb->mode == OverlayScrollbar::Mode::Thick) {
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
    if (sb->mode == OverlayScrollbar::Mode::Thick) {
        return; // never hide in Thick mode
    }
    SetState(sb, State::SmartInvisible);
}

// Restart the thin-bar auto-hide countdown (showAfterScrollMs). SetState only
// arms the timer on a state transition, so continuous scroll while already
// SmartThin would otherwise let the earlier mouse-stop / first-reveal timer
// fire and hide the bar mid-scroll.
static void RestartSmartThinAutoHide(OverlayScrollbar* sb) {
    if (!sb->hwnd || sb->state != State::SmartThin) {
        return;
    }
    KillTimer(sb->hwnd, OverlayScrollbar::kTimerAutoHide);
    SetTimer(sb->hwnd, OverlayScrollbar::kTimerAutoHide, sb->showAfterScrollMs, nullptr);
}

// ---- Global mouse tracking ----

static void CALLBACK MouseTrackTimerProc(HWND /*hwnd*/, UINT /*msg*/, UINT_PTR /*idEvent*/, DWORD /*time*/) {
    // e.g. splitter drag uses SetCapture(); don't react to cursor proximity then
    if (GetCapture()) {
        return;
    }

    Point pt = GetCursorPosition();

    bool mouseMoved = (pt.x != gLastMousePos.x || pt.y != gLastMousePos.y);
    gLastMousePos = pt;

    HWND hwndForeground = GetForegroundWindow();

    for (auto* sb : gAllScrollbars) {
        if (!sb->hwnd || !sb->hwndOwner) {
            continue;
        }

        if (!IsActive(sb)) {
            continue;
        }

        // Only process scrollbars whose owner is in the active window hierarchy
        bool ownerActive = IsOrIsParentOf(hwndForeground, sb->hwndOwner);
        if (!ownerActive) {
            // If we were showing, hide
            if (IsVisible(sb)) {
                if (!sb->isDragging) {
                    HideScrollbarWindow(sb);
                }
            }
            continue;
        }

        // Check if mouse is over the owner window's client area
        Rect ownerRc = HwndWindowRect(sb->hwndOwner);
        bool overOwner = ownerRc.Contains(pt);

        // Is the mouse over the band the thick scrollbar occupies? Being merely
        // near it isn't enough: the thick bar used to pop out while the mouse
        // was still over the page, which is distracting while reading
        Rect sbRect = GetScrollbarScreenRect(sb);
        bool overScrollbar = sbRect.Contains(pt);

        if (sb->isDragging) {
            // Don't change state while dragging
            continue;
        }

        if (gOverlayScrollbarSuppressThick) {
            continue;
        }

        if (overScrollbar) {
            // Mouse is over the scrollbar area - show thick
            if (!IsThick(sb)) {
                ShowScrollbarWindow(sb, true);
            }
            // Update thumb hover state
            Point clientPt = HwndScreenToClient(sb->hwnd, pt);
            Rect thumbRc = GetThumbRect(sb);
            bool wasOver = sb->mouseOverThumb;
            sb->mouseOverThumb = thumbRc.Contains(clientPt);
            if (wasOver != sb->mouseOverThumb) {
                PaintScrollbar(sb);
            }
        } else if (overOwner && mouseMoved) {
            // Mouse is over owner and moving, but not over the scrollbar - show thin
            // IsThick() means transitioning from thick to thin
            if (IsThick(sb) || sb->state != State::SmartThin) {
                ShowScrollbarWindow(sb, false);
            }
            // Reset the auto-hide timer since mouse is moving
            KillTimer(sb->hwnd, OverlayScrollbar::kTimerAutoHide);
            SetTimer(sb->hwnd, OverlayScrollbar::kTimerAutoHide, sb->hideAfterMouseStopMs, nullptr);
        } else if (IsThick(sb) && !overOwner) {
            // Mouse left the owner area while thick - transition to hidden
            HideScrollbarWindow(sb);
        }
        // If mouse is over owner but not moving, the existing auto-hide timer handles it
    }
}

static void StartMouseTracking() {
    if (gMouseTrackTimer) {
        return;
    }
    gLastMousePos = {-1, -1};
    gMouseTrackTimer = SetTimer(nullptr, kMouseTrackTimerID, kMouseTrackIntervalMs, MouseTrackTimerProc);
}

static void StopMouseTracking() {
    if (gMouseTrackTimer) {
        KillTimer(nullptr, gMouseTrackTimer);
        gMouseTrackTimer = 0;
    }
}

// ---- WndProc for scrollbar window (handles clicks, drag, wheel) ----

static LRESULT CALLBACK WndProcOverlayScrollbar(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    OverlayScrollbar* sb = (OverlayScrollbar*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (!sb) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    switch (msg) {
        case WM_TIMER:
            if (wp == OverlayScrollbar::kTimerAutoHide) {
                if (!sb->isDragging) {
                    HideScrollbarWindow(sb);
                }
                return 0;
            }
            if (wp == OverlayScrollbar::kTimerRepeatScroll) {
                if (sb->repeatScrollCode == 0) {
                    KillTimer(hwnd, OverlayScrollbar::kTimerRepeatScroll);
                    return 0;
                }
                SendScrollMsg(sb, ScrollMsgForType(sb), MAKEWPARAM(sb->repeatScrollCode, 0));
                if (sb->repeatIsInitial) {
                    // switch from initial delay to repeat rate
                    sb->repeatIsInitial = false;
                    UINT repeatMs = 0;
                    SystemParametersInfoW(SPI_GETKEYBOARDSPEED, 0, &repeatMs, 0);
                    // SPI_GETKEYBOARDSPEED returns 0-31, map to ~33-500ms (same as OS key repeat)
                    repeatMs = 400 - (repeatMs * 12);
                    SetTimer(hwnd, OverlayScrollbar::kTimerRepeatScroll, repeatMs, nullptr);
                }
                return 0;
            }
            break;

        case WM_MOUSEMOVE: {
            int mx = GET_X_LPARAM(lp);
            int my = GET_Y_LPARAM(lp);

            if (sb->isDragging) {
                int ptInTrack = IsVert(sb) ? my : mx;
                Rect track = GetTrackRect(sb);
                int range = sb->nMax - sb->nMin + 1;
                int thumbLen = MulDiv(IsVert(sb) ? track.dy : track.dx, (int)sb->nPage, range);
                int minThumb = DpiScale(kMinThumbSize);
                thumbLen = std::max(thumbLen, minThumb);
                int trackLen = IsVert(sb) ? track.dy : track.dx;
                int scrollableTrack = trackLen - thumbLen;
                int scrollableRange = range - (int)sb->nPage;

                int dragDelta = ptInTrack - sb->dragStartY;
                int newPos = sb->dragStartPos;
                if (scrollableTrack > 0 && scrollableRange > 0) {
                    newPos = sb->dragStartPos + MulDiv(dragDelta, scrollableRange, scrollableTrack);
                }
                newPos = setMinMax(newPos, sb->nMin, sb->nMax - (int)sb->nPage + 1);
                sb->nTrackPos = newPos;
                PaintScrollbar(sb);
                SendScrollMsg(sb, ScrollMsgForType(sb), MAKEWPARAM(SB_THUMBTRACK, newPos));
                return 0;
            }

            // Thumb hover is handled by the global tracker, but also handle here
            // for responsiveness when already thick
            if (IsThick(sb)) {
                Rect thumbRc = GetThumbRect(sb);
                bool wasOver = sb->mouseOverThumb;
                sb->mouseOverThumb = thumbRc.Contains(Point(mx, my));
                if (wasOver != sb->mouseOverThumb) {
                    PaintScrollbar(sb);
                }
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int mx = GET_X_LPARAM(lp);
            int my = GET_Y_LPARAM(lp);
            SetCapture(hwnd);

            if (IsThick(sb)) {
                Rect arrowTop = GetArrowTopRect(sb);
                Rect arrowBot = GetArrowBottomRect(sb);
                Point pt(mx, my);

                if (arrowTop.Contains(pt)) {
                    // SB_LINEUP == SB_LINELEFT, but spell out which axis we mean
                    UINT code = IsVert(sb) ? SB_LINEUP : SB_LINELEFT; // NOLINT(bugprone-branch-clone)
                    SendScrollMsg(sb, ScrollMsgForType(sb), MAKEWPARAM(code, 0));
                    sb->repeatScrollCode = code;
                    sb->repeatIsInitial = true;
                    UINT delayMs = 0;
                    SystemParametersInfoW(SPI_GETKEYBOARDDELAY, 0, &delayMs, 0);
                    delayMs = 250 + (delayMs * 250); // 0-3 maps to 250-1000ms
                    SetTimer(hwnd, OverlayScrollbar::kTimerRepeatScroll, delayMs, nullptr);
                    return 0;
                }
                if (arrowBot.Contains(pt)) {
                    // SB_LINEDOWN == SB_LINERIGHT, but spell out which axis we mean
                    UINT code = IsVert(sb) ? SB_LINEDOWN : SB_LINERIGHT; // NOLINT(bugprone-branch-clone)
                    SendScrollMsg(sb, ScrollMsgForType(sb), MAKEWPARAM(code, 0));
                    sb->repeatScrollCode = code;
                    sb->repeatIsInitial = true;
                    UINT delayMs = 0;
                    SystemParametersInfoW(SPI_GETKEYBOARDDELAY, 0, &delayMs, 0);
                    delayMs = 250 + (delayMs * 250);
                    SetTimer(hwnd, OverlayScrollbar::kTimerRepeatScroll, delayMs, nullptr);
                    return 0;
                }
            }

            // Shift+click: jump thumb center to click position
            if (GetKeyState(VK_SHIFT) & 0x8000) {
                Rect track = GetTrackRect(sb);
                int range = sb->nMax - sb->nMin + 1;
                int trackLen = IsVert(sb) ? track.dy : track.dx;
                int thumbLen = MulDiv(trackLen, (int)sb->nPage, range);
                int minThumb = DpiScale(kMinThumbSize);
                thumbLen = std::max(thumbLen, minThumb);
                int scrollableTrack = trackLen - thumbLen;
                int scrollableRange = range - (int)sb->nPage;
                int clickInTrack = (IsVert(sb) ? my : mx) - (IsVert(sb) ? track.y : track.x);
                int thumbOffset = clickInTrack - (thumbLen / 2);
                thumbOffset = setMinMax(thumbOffset, 0, scrollableTrack);
                int newPos = sb->nMin;
                if (scrollableTrack > 0 && scrollableRange > 0) {
                    newPos = sb->nMin + MulDiv(thumbOffset, scrollableRange, scrollableTrack);
                }
                sb->nTrackPos = newPos;
                PaintScrollbar(sb);
                SendScrollMsg(sb, ScrollMsgForType(sb), MAKEWPARAM(SB_THUMBTRACK, newPos));
                ReleaseCapture();
                return 0;
            }

            Rect thumbRc = GetThumbRect(sb);
            Point pt(mx, my);
            if (thumbRc.Contains(pt)) {
                sb->isDragging = true;
                sb->dragStartY = IsVert(sb) ? my : mx;
                sb->dragStartPos = sb->nPos;
                sb->nTrackPos = sb->nPos;
                return 0;
            }

            Rect track = GetTrackRect(sb);
            if (track.Contains(pt)) {
                int clickPos = IsVert(sb) ? my : mx;
                int thumbMid = IsVert(sb) ? (thumbRc.y + (thumbRc.dy / 2)) : (thumbRc.x + (thumbRc.dx / 2));
                UINT code;
                if (clickPos < thumbMid) {
                    // SB_PAGEUP == SB_PAGELEFT (same for DOWN/RIGHT); spell out the axis
                    code = IsVert(sb) ? SB_PAGEUP : SB_PAGELEFT; // NOLINT(bugprone-branch-clone)
                } else {
                    code = IsVert(sb) ? SB_PAGEDOWN : SB_PAGERIGHT; // NOLINT(bugprone-branch-clone)
                }
                SendScrollMsg(sb, ScrollMsgForType(sb), MAKEWPARAM(code, 0));
                sb->repeatScrollCode = code;
                sb->repeatIsInitial = true;
                UINT delayMs = 0;
                SystemParametersInfoW(SPI_GETKEYBOARDDELAY, 0, &delayMs, 0);
                delayMs = 250 + (delayMs * 250);
                SetTimer(hwnd, OverlayScrollbar::kTimerRepeatScroll, delayMs, nullptr);
                return 0;
            }
            ReleaseCapture();
            return 0;
        }

        case WM_LBUTTONUP:
            if (sb->repeatScrollCode != 0) {
                sb->repeatScrollCode = 0;
                KillTimer(hwnd, OverlayScrollbar::kTimerRepeatScroll);
            }
            if (sb->isDragging) {
                sb->isDragging = false;
                sb->nPos = sb->nTrackPos;
                PaintScrollbar(sb);
                SendScrollMsg(sb, ScrollMsgForType(sb), MAKEWPARAM(SB_THUMBPOSITION, sb->nPos));
            }
            ReleaseCapture();
            return 0;

        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            SendMessageW(sb->hwndOwner, msg, wp, lp);
            return 0;

        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;

        case WM_NCHITTEST: {
            // pass through rightmost 2px of vertical scrollbar for frame resize
            if (IsVert(sb)) {
                int x = GET_X_LPARAM(lp);
                Rect rc = HwndWindowRect(hwnd);
                if ((rc.x + rc.dx - x) <= 2) {
                    return HTTRANSPARENT;
                }
            }
            LRESULT def = DefWindowProcW(hwnd, msg, wp, lp);
            if (def == HTNOWHERE) {
                return HTCLIENT;
            }
            return def;
        }
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void RegisterScrollbarClass() {
    if (gScrollbarClassRegistered) {
        return;
    }
    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(wcex);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProcOverlayScrollbar;
    wcex.hInstance = GetModuleHandleW(nullptr);
    wcex.hCursor = GetCachedCursor(IDC_ARROW);
    wcex.lpszClassName = OVERLAY_SCROLLBAR_CLASS;
    RegisterClassExW(&wcex);
    gScrollbarClassRegistered = true;
}

OverlayScrollbar* OverlayScrollbarCreate(HWND hwndOwner, OverlayScrollbar::Type type, OverlayScrollbar::Mode mode) {
    RegisterScrollbarClass();

    auto* sb = new OverlayScrollbar();
    sb->hwndOwner = hwndOwner;
    sb->type = type;
    sb->mode = mode;
    sb->thinWidth = DpiScale(4);
    sb->thickWidth = DpiScale(16);
    int sysWidth = DpiGetSystemMetrics(IsVert(sb) ? SM_CXVSCROLL : SM_CYHSCROLL);
    if (sysWidth > 0) {
        sb->thickWidth = sysWidth;
    }
    DWORD exStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
    DWORD style = WS_POPUP;

    // use the top-level ancestor as owner so the scrollbar stays above its
    // own window but doesn't cover other application windows
    HWND hwndTopLevel = GetAncestor(hwndOwner, GA_ROOT);
    sb->hwnd = CreateWindowExW(exStyle, OVERLAY_SCROLLBAR_CLASS, nullptr, style, 0, 0, 1, 1, hwndTopLevel, nullptr,
                               GetModuleHandleW(nullptr), nullptr);
    SetWindowLongPtrW(sb->hwnd, GWLP_USERDATA, (LONG_PTR)sb);

    // Register for global mouse tracking
    gAllScrollbars.Append(sb);
    StartMouseTracking();

    return sb;
}

void OverlayScrollbarDestroy(OverlayScrollbar* sb) {
    if (!sb) {
        return;
    }

    // Unregister from global mouse tracking
    gAllScrollbars.Remove(sb);
    if (len(gAllScrollbars) == 0) {
        StopMouseTracking();
    }

    if (sb->hwnd) {
        KillTimer(sb->hwnd, OverlayScrollbar::kTimerAutoHide);
        DestroyWindow(sb->hwnd);
    }
    delete sb;
}

// Same API as SetScrollInfo / GetScrollInfo
void OverlayScrollbarSetInfo(OverlayScrollbar* sb, const SCROLLINFO* si, bool redraw) {
    if (!sb) {
        return;
    }
    bool changed = false;
    if (si->fMask & SIF_RANGE) {
        if (sb->nMin != si->nMin || sb->nMax != si->nMax) {
            changed = true;
        }
        sb->nMin = si->nMin;
        sb->nMax = si->nMax;
    }
    if (si->fMask & SIF_PAGE) {
        if (sb->nPage != si->nPage) {
            changed = true;
        }
        sb->nPage = si->nPage;
    }
    if (si->fMask & SIF_POS) {
        if (sb->nPos != si->nPos) {
            changed = true;
        }
        sb->nPos = si->nPos;
    }

    if (redraw && changed) {
        // Scroll moved: re-reveal the thin smart bar if auto-hidden, repaint the
        // thumb, and keep the auto-hide timer alive while scrolling continues
        // (wheel / keyboard / smooth-scroll ticks — not only mouse motion).
        if (IsVisible(sb)) {
            PaintScrollbar(sb);
            if (sb->state == State::SmartThin) {
                RestartSmartThinAutoHide(sb);
            }
        } else {
            ShowScrollbarWindow(sb, false);
        }
    }
}

// Show the thin smart overlay after scroll activity (mouse wheel, keys, etc.).
// Unlike mouse-move tracking, this does not require cursor motion (#5859).
void OverlayScrollbarNotifyScroll(OverlayScrollbar* sb) {
    if (!sb || !IsActive(sb) || sb->isDragging) {
        return;
    }
    if (sb->mode == OverlayScrollbar::Mode::Thick) {
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

void OverlayScrollbarGetInfo(OverlayScrollbar* sb, SCROLLINFO* si) {
    if (!sb) {
        return;
    }
    if (si->fMask & SIF_RANGE) {
        si->nMin = sb->nMin;
        si->nMax = sb->nMax;
    }
    if (si->fMask & SIF_PAGE) {
        si->nPage = sb->nPage;
    }
    if (si->fMask & SIF_POS) {
        si->nPos = sb->nPos;
    }
    if (si->fMask & SIF_TRACKPOS) {
        si->nTrackPos = sb->nTrackPos;
    }
}

// Call when owner window moves/resizes
void OverlayScrollbarUpdatePos(OverlayScrollbar* sb) {
    if (!sb || !sb->hwnd || !sb->hwndOwner) {
        return;
    }

    Rect ownerRc = HwndWindowRect(sb->hwndOwner);

    int scrollW = ScaledWidth(sb, IsThick(sb));
    int x, y, w, h;

    // Check if the sibling scrollbar (other orientation, same owner) is thick
    bool siblingThick = false;
    for (auto* other : gAllScrollbars) {
        if (other != sb && other->hwndOwner == sb->hwndOwner && IsThick(other)) {
            siblingThick = true;
            break;
        }
    }
    int siblingInset = 0;
    if (IsThick(sb) && siblingThick) {
        siblingInset = scrollW;
    }

    if (IsVert(sb)) {
        x = ownerRc.x + ownerRc.dx - scrollW;
        y = ownerRc.y;
        w = scrollW;
        h = ownerRc.dy - siblingInset;
    } else {
        x = ownerRc.x;
        y = ownerRc.y + ownerRc.dy - scrollW;
        w = ownerRc.dx - siblingInset;
        h = scrollW;
    }

    LONG_PTR exStyle = GetWindowLongPtrW(sb->hwnd, GWL_EXSTYLE);
    if (IsVisible(sb)) {
        exStyle &= ~WS_EX_TRANSPARENT;
    } else {
        exStyle |= WS_EX_TRANSPARENT;
    }
    SetWindowLongPtrW(sb->hwnd, GWL_EXSTYLE, exStyle);

    UINT swpFlags = SWP_NOACTIVATE;
    // re-show the window if the state says it should be visible
    // (RelayoutFrame hides overlay scrollbar windows with SW_HIDE
    // to prevent them from appearing at stale positions)
    if (IsVisible(sb) && !HwndIsVisible(sb->hwnd)) {
        swpFlags |= SWP_SHOWWINDOW;
    }
    // When not visible, don't change Z-order — HWND_TOP on an owned popup
    // brings the owner (frame) to the top too, which can hide other popups
    // like the command palette
    if (!IsVisible(sb)) {
        swpFlags |= SWP_NOZORDER;
    }
    SetWindowPos(sb->hwnd, IsVisible(sb) ? HWND_TOP : nullptr, x, y, w, h, swpFlags);
}

// Hide the scrollbar window without stealing activation from other windows.
// Uses SWP_HIDEWINDOW | SWP_NOACTIVATE instead of ShowWindow(SW_HIDE).
void OverlayScrollbarHide(OverlayScrollbar* sb) {
    if (!sb || !sb->hwnd) {
        return;
    }
    SetWindowPos(sb->hwnd, nullptr, 0, 0, 0, 0,
                 SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
}

// Show/hide
void OverlayScrollbarShow(OverlayScrollbar* sb, bool show) {
    if (!sb) {
        return;
    }
    if (!show) {
        if (!IsActive(sb)) {
            return;
        }
        SetState(sb, State::Hidden);
        return;
    }

    // Already painted thin/thick — nothing to do. SmartInvisible still has an
    // IsWindowVisible layered HWND (fully transparent), so do not treat that as
    // shown: keyboard scroll / UpdateScrollbars must re-reveal it (#5850).
    if (IsVisible(sb) && HwndIsVisible(sb->hwnd)) {
        return;
    }
    if (!IsActive(sb) || sb->state == State::SmartInvisible) {
        ShowScrollbarWindow(sb, false);
        return;
    }
    // re-show if window was temporarily hidden (e.g. during relayout)
    OverlayScrollbarUpdatePos(sb);
    ShowWindow(sb->hwnd, SW_SHOWNOACTIVATE);
    PaintScrollbar(sb);
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
    if (mode == OverlayScrollbar::Mode::Thick) {
        SetState(sb, State::AlwaysThick);
    } else {
        // Smart mode: start as thin, will auto-hide
        SetState(sb, State::SmartThin);
    }
}

// returns true if scrollbar is visible (thin, thick, or always thick)
bool IsOverlayScrollbarVisible(OverlayScrollbar* sb) {
    return sb && IsVisible(sb);
}
