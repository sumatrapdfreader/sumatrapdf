/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class EngineBase;
struct RenderedBitmap;

struct RefHoverState {
    HWND hwndPopup = nullptr;
    // currently shown rendered destination strip (owned)
    RenderedBitmap* bmp = nullptr;
    int displayedDestPage = -1;
    // Destination point used for the currently-displayed bitmap. Compared
    // against incoming requests so adjacent links that target the same page
    // but different positions (e.g. a section-7 link and a bib ref both
    // landing on page 41) re-render rather than reuse the stale popup.
    float displayedDestX = -1.f;
    float displayedDestY = -1.f;

    // pending request: set by RefHoverSchedule, consumed by RefHoverOnTimer
    Point pendingScreenPt{};
    int pendingDestPage = -1;
    float pendingDestX = -1.f;
    float pendingDestY = -1.f;
    // Source link location, used to recover a more specific destY when the
    // PDF link is page-level (destY < 0). We extract the source link's text
    // from srcPage at srcRect and search for that text on destPage to find
    // the matching entry's Y. Without this, page-level abbreviation /
    // glossary links render the whole abbreviations page from top.
    int pendingSrcPage = -1;
    RectF pendingSrcRect{};
    // Screen rect of the source page (visible portion). Used to clamp the
    // popup so it stays within the document area and doesn't drift into
    // the gray margins outside the page.
    Rect pendingPageScreenRect{};

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
void RefHoverSchedule(RefHoverState* s, HWND hwndCanvas, Point screenPt, int destPage, float destX, float destY,
                      int srcPage, RectF srcRect, Rect pageScreenRect);
void RefHoverHide(RefHoverState* s, HWND hwndCanvas);
// pageZoom is the destination page's current display zoom (px-per-pt) —
// used as the initial render zoom so popup text height matches the page.
void RefHoverOnTimer(RefHoverState* s, HWND hwndCanvas, EngineBase* engine, float pageZoom);
// Re-render the popup at adjusted zoom in response to a mouse-wheel event.
// Popup window keeps its initial size; only the rendered content scales.
// Positive delta zooms in, negative zooms out. Returns true if the zoom
// changed and a re-render happened.
bool RefHoverWheelZoom(RefHoverState* s, EngineBase* engine, int wheelDelta);
// Scroll the popup's rendered region by a wheel notch. Positive delta scrolls
// toward earlier content (up); negative scrolls toward later content (down).
// Rolls over to the previous / next page when the viewport hits a page edge
// (continuous scrolling). Popup window keeps its initial size; only the
// rendered region's Y (and possibly page number) changes.
bool RefHoverWheelScroll(RefHoverState* s, EngineBase* engine, int wheelDelta);
