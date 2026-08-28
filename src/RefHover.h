/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// The whole RefHover module's declarations: the public API Canvas / MainWindow
// call, the layout detectors the unit tests exercise, and the internals shared
// between the RefHover*.cpp files.

//--- public API

class EngineBase;
struct DocController;
struct DisplayModel;
struct ILinkHandler;
struct IPageDestination;
struct IPageElement;
struct RenderedBitmap;
struct Pixmap;
struct RefLookupCache;

struct RefHoverState {
    HWND hwndPopup = nullptr;
    HWND hwndCanvas = nullptr;
    // kept current by Canvas on mouse-move so popup clicks can open links
    DocController* ctrl = nullptr;
    ILinkHandler* linkHandler = nullptr;
    // currently shown rendered destination strip (owned)
    Pixmap* bmp = nullptr;
    // engine for the currently displayed page, AddRef'd while shown so the
    // popup can hit-test links under the cursor (hand cursor + click-to-open)
    EngineBase* hitEngine = nullptr;

    // cache of plain-text citation lookups (lazy-init on first use)
    RefLookupCache* lookupCache = nullptr;

    // Pending hover request: set by RefHoverSchedule, consumed by
    // RefHoverOnTimer when the hover-delay timer fires.
    struct Pending {
        Point screenPt;
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
        RectF srcRect;
        // Screen rect of the source page (visible portion). Used to clamp
        // the popup so it stays within the document area and doesn't drift
        // into the gray margins outside the page.
        Rect pageScreenRect;
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
        RectF region;
        // Second crop stitched below `region` in the delivered bitmap, for a
        // bracket-style entry that wraps across a 2-column page break (see
        // DetectEntryBox's continuationOut). Empty (dx/dy <= 0) when there's
        // none. Only ever set by the initial DetectEntryBox-driven show —
        // wheel-zoom/scroll re-renders build a fresh request from just
        // displayed.region and never populate this, so the stitched strip is
        // dropped as soon as the user interacts (region-shift math for a
        // composited bitmap isn't supported).
        RectF continuationRegion;
        // initial show: commit displayed.* and show the popup on completion.
        // false for wheel zoom / scroll re-renders, which update displayed.*
        // optimistically and only need the new bitmap.
        bool showPopup = false;
        Point screenPt;
        float destXRaw = -1.f;
        float destYRaw = -1.f;
        // source link location (page coords) that triggered this show; carried
        // through so RefHoverSchedule can tell two occurrences of the same
        // reference apart and reposition the popup to the new one
        int srcPageRaw = -1;
        RectF srcRectRaw;
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
        RectF region;
        // baseZoom matches the document's current page zoom on first show
        // so popup text height is comparable to page text. userZoom is the
        // multiplier driven by the user's mouse-wheel.
        float baseZoom = 1.f;
        float userZoom = 1.f;
        // source link that produced this popup (page coords). Compared in
        // RefHoverSchedule so hovering a different occurrence of the same
        // reference re-positions the popup instead of skipping as a no-op.
        int srcPage = -1;
        RectF srcRect;
    } displayed;
};

constexpr UINT_PTR kRefHoverTimerID = 9;
constexpr UINT_PTR kRefHoverHideTimerID = 10;

RefHoverState* RefHoverCreate(HWND hwndCanvas);
void RefHoverDestroy(RefHoverState* s);
bool RefHoverIsInternalLink(IPageElement* el, DisplayModel* dm);
void RefHoverOnCanvasMouseMove(RefHoverState*& s, HWND hwndCanvas, DocController* ctrl, ILinkHandler* linkHandler,
                               DisplayModel* dm, int x, int y, IPageElement* el, int srcPageNo, int hoverDelayMs);
void RefHoverOnCanvasMouseLeave(RefHoverState* s, HWND hwndCanvas, int hoverDelayMs);
void RefHoverOnCanvasLeftButtonDown(RefHoverState* s, HWND hwndCanvas);
bool RefHoverOnCanvasTimer(RefHoverState* s, HWND hwndCanvas, DisplayModel* dm, UINT_PTR timerId);
void RefHoverSchedule(RefHoverState* s, HWND hwndCanvas, int delayMs, Point screenPt, int destPage, float destX,
                      float destY, float destZoom, int srcPage, RectF srcRect, Rect pageScreenRect);
void RefHoverHide(RefHoverState* s, HWND hwndCanvas);
void RefHoverScheduleHide(RefHoverState* s, HWND hwndCanvas, int delayMs);
void RefHoverOnHideTimer(RefHoverState* s, HWND hwndCanvas);
void RefHoverHandlePopupClick(RefHoverState* s, IPageDestination* dest);
void RefHoverOnTimer(RefHoverState* s, HWND hwndCanvas, EngineBase* engine, float pageZoom);
bool RefHoverWheelZoom(RefHoverState* s, EngineBase* engine, int wheelDelta);
bool RefHoverWheelScroll(RefHoverState* s, EngineBase* engine, int wheelDelta);

//--- layout detection (RefHoverDetect.cpp)

// Flatten per-glyph ink boxes to uniform top-aligned line rows. mupdf reports
// tight per-glyph boxes whose tops vary within a line; the detectors below key
// off coords[i].y as a line coordinate, so callers must pass coords through
// this first (grouping by baseline = y+dy). `out` needs textLen rects and must
// not alias `coords`. Synthetic top-aligned input is left effectively
// unchanged (each line already has a single top).
void NormalizeGlyphLines(const Rect* coords, Rect* out, int glyphCount);

int StripWatermarkGlyphs(WStr text, const Rect* coords, WCHAR* outText, Rect* outCoords);

RectF LandscapeBox(RectF mediabox, float destX, float destY, WStr text, const Rect* coords);

RectF DetectEquationBox(WStr text, const Rect* coords, RectF mediabox, float destX, float destY);

RectF DetectEntryBox(WStr text, const Rect* coords, RectF mediabox, float destX, float destY,
                     RectF* continuationOut = nullptr);

//--- plain-text citation lookup (RefHoverText.cpp)

bool RefHoverTryPlainText(RefHoverState* s, EngineBase* engine, int srcPage, Point pagePos, int& destPageOut,
                          float& destXOut, float& destYOut, RectF& srcRectOut);

void RefHoverFreeLookupCache(RefHoverState* s);

float RefHoverResolveDestYFromSourceText(EngineBase* engine, int srcPage, RectF srcRect, int destPage);

//--- citation pattern matching (RefHoverTextDetect.cpp)

// Detect a "(Surname et al., 2020)" / "Surname (2020)" pattern at pagePos (page
// coordinates). On success returns true and fills *surnameOut with a
// freshly-allocated UTF-8 surname (caller frees) and *yearOut.
// srcRectOut (optional): on success, set to a stable per-occurrence source
// key — the matched citation's glyph span on the page (surname through year).
// Lets callers tell two occurrences of the same citation apart, including
// two markers on the same text line (different x/dx → reposition).
bool DetectCitationInPageText(WStr text, const Rect* coords, int textLen, Point pagePos, Str* surnameOut, int* yearOut,
                              Rect* srcRectOut = nullptr);

bool FindSurnameInPageText(WStr text, const Rect* coords, int textLen, WStr surnameW, int year, float* xOut,
                           float* yOut);

bool DetectNumericCitationInPageText(WStr text, const Rect* coords, int textLen, Point pagePos, int* numOut,
                                     Rect* srcRectOut = nullptr);

bool FindNumericReferenceInPageText(WStr text, const Rect* coords, int textLen, int num, float* xOut, float* yOut);

//--- shared between the RefHover*.cpp files, not for use outside them

constexpr const WCHAR* kRefHoverClass = L"SumatraPDFRefHover";

constexpr float kRefHoverRenderZoom = 1.5f;
constexpr int kRefHoverMaxPopupWidth = 1200;
constexpr int kRefHoverMaxPopupHeight = 600;
constexpr int kRefHoverBorder = 4;
constexpr int kRefHoverCursorPad = 30;
constexpr int kRefHoverScrollStepPx = 60;
constexpr float kRefHoverMinUserZoom = 0.4f;
constexpr float kRefHoverMaxUserZoom = 3.0f;
constexpr float kRefHoverUserZoomStep = 1.15f;

constexpr int kRefHoverMaxLiveStates = 32;

bool RefHoverIsLaunchLink(IPageDestination* dest);

bool RefHoverIsLiveState(RefHoverState* s);
void RefHoverRegisterLiveState(RefHoverState* s);
void RefHoverUnregisterLiveState(RefHoverState* s);
void RefHoverDropQueuedRender(RefHoverState* s);
TempWStr RefHoverPageTextToWStrTemp(Str text);

bool RefHoverPopupCreate(RefHoverState* s, HWND hwndCanvas);

void RefHoverShowPopup(RefHoverState* s, Point screenPt);
void RefHoverRequestRender(RefHoverState* s, EngineBase* engine, RefHoverState::RenderRequest req);
bool RefHoverRerenderDisplayedRegion(RefHoverState* s, EngineBase* engine, int page, RectF region);
