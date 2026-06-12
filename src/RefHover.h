/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class EngineBase;
struct RenderedBitmap;

struct RefHoverState {
    HWND hwndPopup = nullptr;
    // currently shown rendered destination strip (owned)
    RenderedBitmap* bmp = nullptr;

    // Pending hover request: set by RefHoverSchedule, consumed by
    // RefHoverOnTimer when the hover-delay timer fires.
    struct Pending {
        Point screenPt{};
        int destPage = -1;
        float destX = -1.f;
        float destY = -1.f;
        // /XYZ zoom hint from the link (1.0 = 100%). 0 means "no zoom hint";
        // RefHover then falls back to its auto-fit DetectEntryBox heuristic.
        // When non-zero, the popup renders the destination region centred on
        // (destX, destY) at this zoom — honouring the link author's intent
        // (e.g. "goto top-left at 2x").
        float destZoom = 0.f;
        // Source link location, used to recover a more specific destY when
        // the PDF link is page-level (destY < 0). We extract the source
        // link's text from srcPage at srcRect and search for that text on
        // destPage to find the matching entry's Y. Without this, page-level
        // abbreviation / glossary links render the whole abbreviations page
        // from top.
        int srcPage = -1;
        RectF srcRect{};
        // Screen rect of the source page (visible portion). Used to clamp
        // the popup so it stays within the document area and doesn't drift
        // into the gray margins outside the page.
        Rect pageScreenRect{};
    } pending;

    // Async rendering: renders run on a background thread (a complex page
    // would otherwise stall the UI for the duration of the render) and the
    // bitmap is delivered back on the UI thread via uitask.
    struct RenderRequest {
        bool valid = false;
        // matched against renderGen on completion, stale results are dropped
        int gen = 0;
        EngineBase* engine = nullptr; // AddRef()'ed for the render duration
        int pageNo = -1;
        float zoom = 0.f;
        RectF region{};
        // initial show: commit displayed.* and show the popup on completion.
        // false for wheel zoom / scroll re-renders, which update displayed.*
        // optimistically and only need the new bitmap.
        bool showPopup = false;
        Point screenPt{};
        float destXRaw = -1.f;
        float destYRaw = -1.f;
    };
    // bumped on every new request and on hide, invalidating older results
    int renderGen = 0;
    bool renderInFlight = false;
    // the latest request that arrived while another was in flight; started
    // when that one completes (coalesces wheel-scroll / zoom streams)
    RenderRequest queuedRender;

    // Currently-displayed bitmap context. Compared against incoming hover
    // requests to skip a re-render when the destination hasn't changed, and
    // re-used by the wheel-zoom / wheel-scroll handlers so they can re-render
    // without re-running detection.
    struct Displayed {
        int destPage = -1;
        float destX = -1.f;
        float destY = -1.f;
        // Region of the page rendered into the popup bitmap, kept so the
        // wheel handlers can shift / scale it without re-running detection.
        RectF region{};
        // baseZoom matches the document's current page zoom on first show
        // so popup text height is comparable to page text. userZoom is the
        // multiplier driven by the user's mouse-wheel.
        float baseZoom = 1.f;
        float userZoom = 1.f;
    } displayed;
};

constexpr UINT_PTR kRefHoverTimerID = 9;

RefHoverState* RefHoverCreate(HWND hwndCanvas);
void RefHoverDestroy(RefHoverState* s);
// delayMs: how long the cursor must hover before the popup shows
// (the CitationHoverDelay advanced setting)
void RefHoverSchedule(RefHoverState* s, HWND hwndCanvas, int delayMs, Point screenPt, int destPage, float destX,
                      float destY, float destZoom, int srcPage, RectF srcRect, Rect pageScreenRect);
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
