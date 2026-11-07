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
    int range = sb->nMax - sb->nMin + 1;
    int thumbLen = MulDivI(trackLen, (int)sb->nPage, std::max(range, 1));
    return std::max(thumbLen, DpiScale(kMinThumbSize));
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
    int scrollableRange = range - nPage;
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

// the position that puts the thumb's center at `clickPos` (client coords)
int PosForThumbCenteredAt(OverlayScrollbar* sb, int clickPos) {
    Rect track = GetTrackRect(sb);
    int range = sb->nMax - sb->nMin + 1;
    int trackLen = IsVert(sb) ? track.dy : track.dx;
    int thumbLen = ThumbLen(sb, trackLen);
    int scrollableTrack = trackLen - thumbLen;
    int scrollableRange = range - (int)sb->nPage;
    int clickInTrack = clickPos - (IsVert(sb) ? track.y : track.x);
    int thumbOffset = clickInTrack - (thumbLen / 2);
    thumbOffset = limitValue(thumbOffset, 0, std::max(0, scrollableTrack));
    int newPos = sb->nMin;
    if (scrollableTrack > 0 && scrollableRange > 0) {
        newPos = sb->nMin + MulDivI(thumbOffset, scrollableRange, scrollableTrack);
    }
    return newPos;
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

void HideScrollbarWindow(OverlayScrollbar* sb) {
    // Don't hide while user is dragging the thumb
    if (sb->isDragging) {
        return;
    }
    if (IsAlwaysThickMode(sb)) {
        return; // never hide in Thick mode
    }
    SetState(sb, State::SmartInvisible);
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

bool gOverlayScrollbarSuppressThick = false;
