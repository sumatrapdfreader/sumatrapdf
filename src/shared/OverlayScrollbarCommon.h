/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by OverlayScrollbarCommon.cpp and each app's OverlayScrollbar.cpp ---

Color ThemeTrackColor();
Color ThemeThumbColor();
Color ThemeThumbHoverColor();
using State = OverlayScrollbar::State;
bool IsThick(OverlayScrollbar* sb);
bool IsVisible(OverlayScrollbar* sb);
bool IsActive(OverlayScrollbar* sb);
int ScaledWidth(OverlayScrollbar* sb, bool thick);
bool IsVert(OverlayScrollbar* sb);

Rect GetTrackRect(OverlayScrollbar* sb);
int MulDivI(int a, int b, int c);
int ThumbLen(OverlayScrollbar* sb, int trackLen);
Rect GetThumbRect(OverlayScrollbar* sb);
int PosForThumbCenteredAt(OverlayScrollbar* sb, int clickPos);
Rect GetArrowTopRect(OverlayScrollbar* sb);
Rect GetArrowBottomRect(OverlayScrollbar* sb);
void ShowScrollbarWindow(OverlayScrollbar* sb, bool thick);
// Idle: the auto-hide timeout or the mouse leaving, which find markers outlast.
// Away: the owner isn't active or is going away.
enum class HideReason {
    Idle,
    Away
};
void HideScrollbarWindow(OverlayScrollbar* sb, HideReason why);
bool SetFindMarkers(OverlayScrollbar* sb, const Vec<float>& markers, float current, Color col, Color currentCol);
void GetFindMarkerRects(OverlayScrollbar* sb, Vec<Rect>& others, Rect* current);

// implemented by each app
bool IsAlwaysThickMode(OverlayScrollbar* sb);
Rect ClientRect(OverlayScrollbar* sb);
void SetState(OverlayScrollbar* sb, State newState);
void RestartSmartThinAutoHide(OverlayScrollbar* sb);

static constexpr int kMinThumbSize = 20;

static constexpr u8 kAlphaThin = 180;

static constexpr u8 kAlphaThick = 220;
