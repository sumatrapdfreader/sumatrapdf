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
};

constexpr int kRefHoverDelayMs = 300;
constexpr UINT_PTR kRefHoverTimerID = 9;

RefHoverState* RefHoverCreate(HWND hwndCanvas);
void RefHoverDestroy(RefHoverState* s);
void RefHoverSchedule(RefHoverState* s, HWND hwndCanvas, Point screenPt, int destPage, float destX, float destY);
void RefHoverHide(RefHoverState* s, HWND hwndCanvas);
void RefHoverOnTimer(RefHoverState* s, HWND hwndCanvas, EngineBase* engine);
