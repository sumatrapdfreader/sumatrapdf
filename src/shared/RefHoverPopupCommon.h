/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by RefHoverPopupCommon.cpp and each app's RefHoverPopup.cpp ---

IPageDestination* LaunchLinkAtPopupPt(RefHoverState* s, int clientX, int clientY);
bool RefHoverIsInternalLink(IPageElement* el, DisplayModel* dm);

// WHEEL_DELTA, the unit RefHoverWheelScroll counts notches in
constexpr int kWheelDelta = 120;

// implemented by each app
bool RefHoverPopupShown(RefHoverState* s);
Size RefHoverPopupClientSize(RefHoverState* s);
