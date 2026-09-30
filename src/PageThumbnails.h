/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct PageThumbnailsCache;
struct ThumbnailRowsModel;

enum class ThumbnailsHost {
    // the command palette's "&" mode: the page under the mouse is selected,
    // Enter or double click goes to it
    Palette,
    // the sidebar's Thumbnails view: a click goes to a page, Ctrl / Shift click
    // select several, dragging moves them, dropping PDF files inserts them
    Sidebar,
};

// A grid of page thumbnails, rendered in the background
struct PageThumbnailsCtrl : VirtListBox {
    ThumbnailsHost host = ThumbnailsHost::Palette;
    MainWindow* win = nullptr;
    WindowTab* tab = nullptr;
    // tab's document when SetTab() was called; only compared, never used
    DisplayModel* dm = nullptr;
    ThumbnailRowsModel* rowsModel = nullptr;
    PageThumbnailsCache* cache = nullptr;
    int pageCount = 0;
    // palette: the keyboard / mouse selection; sidebar: the current page
    int selectedPage = 1;
    int cols = 1;
    int thumbDx = 0;
    int thumbDy = 0;
    int gap = 0;
    int rowGap = 0;
    bool active = false;
    // palette: after going to a page on Enter or double click
    Func0 onPageOpened;

    // sidebar: pages selected for moving, indexed by pageNo - 1
    Vec<u8> marked;
    int anchorPage = 0;
    // sidebar: the page the left button went down on, until it goes up
    int pressedPage = 0;
    Point pressPt;
    bool dragging = false;
    // while dragging pages or files: they'd go in front of this page
    // (pageCount + 1: at the end); 0 when not over the grid
    int dropBefore = 0;

    PageThumbnailsCtrl(MainWindow*, PlatformFont*, int dpi, ThumbnailsHost);
    ~PageThumbnailsCtrl() override;

    void SetTab(WindowTab*);
    void SetBounds(Rect) override;
    void Paint(VirtPaintCtx&) override;
    void DrawRow(DrawItemEvent*);
    void OnThumbMouseDown(VirtMouseEvent*);
    void OnThumbMouseMove(VirtMouseEvent*);
    void OnThumbMouseUp(VirtMouseEvent*);
    void OnThumbMouseWheel(VirtMouseEvent*);
    void OnThumbDoubleClick(VirtMouseEvent*);
    void OnThumbKeyDown(VirtKeyEvent*);
    void OnThumbCaptureLost();
    void Activate();
    void Deactivate();
    void HandleKey(int vkey);
    void StartRendering();
    void Refresh();
    int RenderedCount() const;
    void SelectPage(int);
    void SetCurrentPage(int);
    int DropPosition(Point ptLocal);
    void SetDropPosition(int);
    void MarkedPages(Vec<int>& out);
    Rect PageRect(int pageNo);

  protected:
    int PageAtPoint(Point);
    void OpenSelectedPage();
    void ResetCache();
    void MarkOnly(int pageNo);
    void ClickPage(int pageNo, bool ctrl, bool shift);
    void DropMarkedPages();
    void EndPress();
};

void RegisterThumbnailsDropTarget(PageThumbnailsCtrl*, HWND);
void RevokeThumbnailsDropTarget(HWND);
