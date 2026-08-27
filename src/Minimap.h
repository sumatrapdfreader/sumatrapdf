#pragma once
struct MainWindow;
struct DisplayModel;
struct RenderedBitmap;

struct CachedThumb {
    int pageNo = -1;
    RenderedBitmap* bitmap = nullptr;
    DWORD lastUsed = 0;
};

struct Minimap {
    MainWindow* win = nullptr;
    HWND hwnd = nullptr;
    bool isVisible = false;
    
    // Scroll state
    struct OverlayScrollbar* overlayScrollV = nullptr;
    int scrollY = 0;
    int maxScrollY = 0;
    int thumbW = 0;
    Vec<int> pageHeights; // Pre-calculated thumbnail heights
    
    // Smooth Scrolling state
    double scrollAnimTargetY = 0;
    double scrollAnimCurrentY = 0;
    bool scrollAnimActive = false;
    bool isThumbTracking = false;
    bool scrollAnimHiResTimer = false;
    LARGE_INTEGER scrollAnimLastTime{};
    
    // Cache
    Vec<CachedThumb> cache;
    int pageNo = -1;
    float zoom = 0.20f;
    
    // Hover Preview State
    int hoverPageNo = -1;
    bool isMouseTracking = false;
    HWND hwndPreview = nullptr;
    RenderedBitmap* previewBitmap = nullptr;
    int previewPageNo = -1;

    // Tracking state for cache invalidation and scroll sync
    struct DisplayModel* lastDm = nullptr;
    int lastDarkModeEpoch = 0;
    int lastPdfScrollY = -1;

    // Asynchronous rendering
    Vec<int> pendingRequests;

    ~Minimap();
};

Minimap* MinimapCreate(MainWindow* win);
void MinimapDestroy(Minimap* minimap);
void MinimapToggleVisible(MainWindow* win);
void MinimapUpdate(Minimap* minimap);
void MinimapUpdateScrollProportional(Minimap* minimap);
void MinimapOnPaint(Minimap* minimap, HWND hwnd);
void MinimapRecalculateScroll(Minimap* minimap);
void MinimapScrollTo(Minimap* minimap, int y);
void MinimapClearCache(Minimap* minimap);
