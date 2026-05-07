/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class EngineBase;
struct RenderedBitmap;

struct RefHoverState {
    HWND hwndPopup = nullptr;
    // currently shown rendered destination strip (owned)
    RenderedBitmap* bmp = nullptr;
    int displayedDestPage = -1;

    // pending request: set by RefHoverSchedule, consumed by RefHoverOnTimer
    Point pendingScreenPt{};
    int pendingDestPage = -1;
    float pendingDestX = -1.f;
    float pendingDestY = -1.f;

    // re-render context, kept so mouse-wheel can zoom the popup without
    // re-running detection. Reset on every new destination.
    RectF lastRegion{};
    // baseZoom matches the document's current page zoom on first show so
    // popup text height is comparable to page text. userZoom is the
    // multiplier driven by the user's mouse-wheel.
    float baseZoom = 1.f;
    float userZoom = 1.f;
};

constexpr int kRefHoverDelayMs = 300;
constexpr UINT_PTR kRefHoverTimerID = 9;

RefHoverState* RefHoverCreate(HWND hwndCanvas);
void RefHoverDestroy(RefHoverState* s);
void RefHoverSchedule(RefHoverState* s, HWND hwndCanvas, Point screenPt, int destPage, float destX, float destY);
void RefHoverHide(RefHoverState* s, HWND hwndCanvas);
// pageZoom is the destination page's current display zoom (px-per-pt) —
// used as the initial render zoom so popup text height matches the page.
void RefHoverOnTimer(RefHoverState* s, HWND hwndCanvas, EngineBase* engine, float pageZoom);
// Re-render the popup at adjusted zoom in response to a mouse-wheel event.
// Popup window keeps its initial size; only the rendered content scales.
// Positive delta zooms in, negative zooms out. Returns true if the zoom
// changed and a re-render happened.
bool RefHoverWheelZoom(RefHoverState* s, EngineBase* engine, int wheelDelta);
