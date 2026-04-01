/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Overlay scrollbar - a semi-transparent top-level window that acts like
// a standard Windows scrollbar but floats over the owner window.

extern int gThickVisibilityDistance;
extern bool gOverlayScrollbarSuppressThick;

struct OverlayScrollbar {
    enum class Type {
        Vert,
        Horz,
    };

    // Smart: thin/thick transitions based on mouse proximity
    // Thick: always shown thick, no transitions
    enum class Mode {
        Smart,
        Thick,
    };

    enum class State {
        Hidden,         // not active, not shown
        SmartInvisible, // active but auto-hidden (Smart mode)
        SmartThin,      // shown thin (Smart mode)
        SmartThick,     // shown thick (Smart mode)
        AlwaysThick,    // always shown thick (Thick mode)
    };

    HWND hwnd = nullptr;      // the scrollbar top-level window
    HWND hwndOwner = nullptr; // positioned relative to this window; receives scroll messages
    Type type = Type::Vert;
    Mode mode = Mode::Smart;

    // scroll state (mirrors SCROLLINFO)
    int nMin = 0;
    int nMax = 0;
    uint nPage = 0;
    int nPos = 0;
    int nTrackPos = 0;

    // widths in pixels (before DPI scaling)
    int thinWidth = 4;
    int thickWidth = 14;

    // auto-hide timing (milliseconds)
    int showAfterScrollMs = 5000;    // how long to show thin bar after scroll info update
    int hideAfterMouseStopMs = 3000; // hide after mouse stops moving

    // internal state
    State state = State::Hidden;
    bool isDragging = false; // user is dragging the thumb
    int dragStartY = 0;      // mouse Y (or X for horz) when drag started
    int dragStartPos = 0;    // nPos when drag started
    bool mouseOverThumb = false;

    // repeat-scroll state (for held arrow/track clicks)
    UINT repeatScrollCode = 0;    // SB_LINEUP, SB_PAGEDOWN, etc.; 0 = not repeating
    bool repeatIsInitial = false; // true = waiting for initial delay, false = repeating

    // timer IDs
    static constexpr UINT_PTR kTimerAutoHide = 1;
    static constexpr UINT_PTR kTimerRepeatScroll = 2;
};

OverlayScrollbar* OverlayScrollbarCreate(HWND hwndOwner, OverlayScrollbar::Type type,
                                         OverlayScrollbar::Mode mode = OverlayScrollbar::Mode::Smart);
void OverlayScrollbarDestroy(OverlayScrollbar* sb);

// Same API as SetScrollInfo / GetScrollInfo
void OverlayScrollbarSetInfo(OverlayScrollbar* sb, const SCROLLINFO* si, bool redraw);
void OverlayScrollbarGetInfo(OverlayScrollbar* sb, SCROLLINFO* si);

// Call when owner window moves/resizes
void OverlayScrollbarUpdatePos(OverlayScrollbar* sb);

// Show/hide
void OverlayScrollbarShow(OverlayScrollbar* sb, bool show);

// returns true if scrollbar is visible (thin, thick, or always thick)
bool IsOverlayScrollbarVisible(OverlayScrollbar* sb);
