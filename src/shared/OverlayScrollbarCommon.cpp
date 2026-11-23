/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#if defined(SUMATRA_NG)
#include "gui/GpuiBridge.h"
#else
#include "base/Base.h"
#endif
#include "gui/Dpi.h"
#include "Theme.h"
#include "OverlayScrollbar.h"
#include "OverlayScrollbarCommon.h"

// Derive scrollbar colors from current theme
Color ThemeTrackColor() {
    Color bg = ThemeControlBackgroundColor();
    return bg;
}

Color ThemeThumbColor() {
    Color bg = ThemeControlBackgroundColor();
    return AccentColor(bg, 100);
}

Color ThemeThumbHoverColor() {
    Color bg = ThemeControlBackgroundColor();
    return AccentColor(bg, 140);
}

bool IsThick(OverlayScrollbar* sb) {
    return sb->state == State::SmartThick || sb->state == State::AlwaysThick;
}

bool IsVisible(OverlayScrollbar* sb) {
    return sb->state == State::SmartThin || sb->state == State::SmartThick || sb->state == State::AlwaysThick;
}

// scrollbar is active: shown or auto-hidden but ready to appear
bool IsActive(OverlayScrollbar* sb) {
    return sb->state != State::Hidden;
}

int ScaledWidth(OverlayScrollbar* sb, bool thick) {
    return thick ? sb->thickWidth : sb->thinWidth;
}

bool IsVert(OverlayScrollbar* sb) {
    return sb->type == OverlayScrollbar::Type::Vert;
}

// returns true if scrollbar is visible (thin, thick, or always thick)
bool IsOverlayScrollbarVisible(OverlayScrollbar* sb) {
    return sb && IsVisible(sb);
}

// Get the track rect in client coords of the scrollbar
Rect GetTrackRect(OverlayScrollbar* sb) {
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

// win32's MulDiv (a * b / c, rounded) for a positive c; portable
int MulDivI(int a, int b, int c) {
    if (c <= 0) {
        return -1;
    }
    i64 n = (i64)a * (i64)b;
    n = n >= 0 ? n + (c / 2) : n - (c / 2);
    return (int)(n / c);
}

int ThumbLen(OverlayScrollbar* sb, int trackLen) {
    int range = std::max(sb->nMax - sb->nMin + 1, 1);
#if defined(SUMATRA_NG)
    // same inputs as the MulDiv path; gpui's minimum is ours, not its 48dip default
    if (trackLen <= 0) {
        return DpiScale(kMinThumbSize);
    }
    float thumb =
        gp::ScrollbarThumbSize((float)trackLen, (float)sb->nPage, (float)range, (float)DpiScale(kMinThumbSize));
    return (int)(thumb + 0.5f);
#else
    int thumbLen = MulDivI(trackLen, (int)sb->nPage, range);
    return std::max(thumbLen, DpiScale(kMinThumbSize));
#endif
}

// Calculate thumb rect within the track. The offset is clamped to 0 when the
// track is shorter than the minimum thumb.
Rect GetThumbRect(OverlayScrollbar* sb) {
    Rect track = GetTrackRect(sb);
    int range = sb->nMax - sb->nMin + 1;
    int nPage = (int)sb->nPage;
    if (range <= 0 || nPage >= range) {
        return track;
    }

    int trackLen = IsVert(sb) ? track.dy : track.dx;
    int thumbLen = ThumbLen(sb, trackLen);

    int scrollableTrack = trackLen - thumbLen;
    int pos = sb->isDragging ? sb->nTrackPos : sb->nPos;
    int thumbOffset = 0;
#if defined(SUMATRA_NG)
    float off = gp::ScrollbarThumbPos((float)trackLen, (float)thumbLen, (float)(pos - sb->nMin), (float)nPage,
                                      (float)std::max(range, 1));
    thumbOffset = (int)(off + 0.5f);
#else
    int scrollableRange = range - nPage;
    if (scrollableRange > 0) {
        thumbOffset = MulDivI(pos - sb->nMin, scrollableTrack, scrollableRange);
    }
#endif
    thumbOffset = limitValue(thumbOffset, 0, std::max(0, scrollableTrack));

    if (IsVert(sb)) {
        return {track.x, track.y + thumbOffset, track.dx, thumbLen};
    }
    return {track.x + thumbOffset, track.y, thumbLen, track.dy};
}

// the position that puts the thumb's center at `clickPos` (client coords)
int PosForThumbCenteredAt(OverlayScrollbar* sb, int clickPos) {
    Rect track = GetTrackRect(sb);
    int range = sb->nMax - sb->nMin + 1;
    int trackLen = IsVert(sb) ? track.dy : track.dx;
    int thumbLen = ThumbLen(sb, trackLen);
    int scrollableRange = range - (int)sb->nPage;
#if defined(SUMATRA_NG)
    int origin = IsVert(sb) ? track.y : track.x;
    float off = gp::ScrollbarOffsetForTrackPress((float)clickPos, (float)origin, (float)trackLen, (float)thumbLen,
                                                 (float)sb->nPage, (float)std::max(range, 1));
    int newPos = sb->nMin + (int)(off + 0.5f);
    if (scrollableRange > 0) {
        newPos = limitValue(newPos, sb->nMin, sb->nMin + scrollableRange);
    } else {
        newPos = sb->nMin;
    }
    return newPos;
#else
    int scrollableTrack = trackLen - thumbLen;
    int clickInTrack = clickPos - (IsVert(sb) ? track.y : track.x);
    int thumbOffset = clickInTrack - (thumbLen / 2);
    thumbOffset = limitValue(thumbOffset, 0, std::max(0, scrollableTrack));
    int newPos = sb->nMin;
    if (scrollableTrack > 0 && scrollableRange > 0) {
        newPos = sb->nMin + MulDivI(thumbOffset, scrollableRange, scrollableTrack);
    }
    return newPos;
#endif
}

Rect GetArrowTopRect(OverlayScrollbar* sb) {
    Rect rc = ClientRect(sb);
    int arrowSize = IsVert(sb) ? rc.dx : rc.dy;
    if (IsVert(sb)) {
        return {0, 0, rc.dx, arrowSize};
    }
    return {0, 0, arrowSize, rc.dy};
}

Rect GetArrowBottomRect(OverlayScrollbar* sb) {
    Rect rc = ClientRect(sb);
    int arrowSize = IsVert(sb) ? rc.dx : rc.dy;
    if (IsVert(sb)) {
        return {0, rc.dy - arrowSize, rc.dx, arrowSize};
    }
    return {rc.dx - arrowSize, 0, arrowSize, rc.dy};
}

void ShowScrollbarWindow(OverlayScrollbar* sb, bool thick) {
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

void HideScrollbarWindow(OverlayScrollbar* sb, HideReason why) {
    // Don't hide while user is dragging the thumb
    if (sb->isDragging) {
        return;
    }
    if (IsAlwaysThickMode(sb)) {
        return; // never hide in Thick mode
    }
    // the find markers are what the user looks at: stay, thin
    if (why == HideReason::Idle && len(sb->findMarkers) > 0) {
        if (sb->state != State::SmartThin) {
            SetState(sb, State::SmartThin);
        }
        return;
    }
    SetState(sb, State::SmartInvisible);
}

// Where the find matches are along a vertical bar: fractions of its range,
// sorted, and the current one (-1: none), in their colors. Returns true if they
// changed. A smart bar shows while there are markers.
bool SetFindMarkers(OverlayScrollbar* sb, const Vec<float>& markers, float current, Color col, Color currentCol) {
    if (!IsVert(sb)) {
        return false;
    }
    bool same = sb->findMarkerCurrent == current && len(sb->findMarkers) == len(markers) &&
                sb->findMarkerColor == col && sb->findMarkerCurrentColor == currentCol;
    for (int i = 0; same && i < len(markers); i++) {
        same = sb->findMarkers[i] == markers[i];
    }
    if (same) {
        return false;
    }
    bool had = len(sb->findMarkers) > 0;
    sb->findMarkers = markers;
    sb->findMarkerCurrent = current;
    sb->findMarkerColor = col;
    sb->findMarkerCurrentColor = currentCol;
    bool has = len(markers) > 0;
    if (has && sb->state == State::SmartInvisible) {
        ShowScrollbarWindow(sb, false);
    } else if (had && !has && sb->state == State::SmartThin) {
        // nothing keeps it up anymore: the usual auto-hide
        RestartSmartThinAutoHide(sb);
    }
    return true;
}

// the find markers as ticks across the bar (as wide as the thumb), one per
// pixel row, in client coords. current->dy is 0 when there is no current match.
void GetFindMarkerRects(OverlayScrollbar* sb, Vec<Rect>& others, Rect* current) {
    VecReset(others);
    *current = {};
    if (!IsVert(sb) || (len(sb->findMarkers) == 0 && sb->findMarkerCurrent < 0)) {
        return;
    }
    Rect rc = ClientRect(sb);
    Rect track = GetTrackRect(sb);
    int x = track.x;
    int dx = track.dx;
    if (!IsThick(sb)) {
        // the thin bar is drawn centered in the window
        dx = std::min(ScaledWidth(sb, false), rc.dx);
        x = (rc.dx - dx) / 2;
    }
    int h = std::max(DpiScale(2), 2);
    int span = track.dy - h;
    if (span <= 0) {
        return;
    }
    int lastY = INT_MIN;
    for (float f : sb->findMarkers) {
        int y = track.y + (int)(f * (float)span);
        if (y == lastY) {
            continue;
        }
        lastY = y;
        VecAppend(others, Rect{x, y, dx, h});
    }
    if (sb->findMarkerCurrent >= 0) {
        *current = {x, track.y + (int)(sb->findMarkerCurrent * (float)span), dx, h};
    }
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

// -dbg-control TestLayout: "findMarkers n=3 ticks=3 current=0.420 visible=1
// at=0.100,0.420,0.900" (at: the first few, fractions of the range)
TempStr OverlayScrollbarFindMarkersReportTemp(OverlayScrollbar* sb) {
    constexpr int kMaxReported = 8;
    if (!sb) {
        return str::DupTemp(StrL("findMarkers n=0 ticks=0 current=-1 visible=0 at=\n"));
    }
    Vec<Rect> ticks;
    Rect current;
    GetFindMarkerRects(sb, ticks, &current);
    str::Builder at;
    for (int i = 0; i < len(sb->findMarkers) && i < kMaxReported; i++) {
        at.Append(fmt(i > 0 ? ",%.3f" : "%.3f", (double)sb->findMarkers[i]));
    }
    return fmt("findMarkers n=%d ticks=%d current=%.3f visible=%d at=%s\n", len(sb->findMarkers), len(ticks),
               (double)sb->findMarkerCurrent, IsOverlayScrollbarVisible(sb) ? 1 : 0, ToStrTemp(at));
}

bool gOverlayScrollbarSuppressThick = false;
