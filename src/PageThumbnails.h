/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct PageThumbnailsCache;
struct ThumbnailRowsModel;
struct Location;
struct Pixmap;

enum class ThumbnailsHost {
    // the command palette's "&" mode: the page under the mouse is selected,
    // Enter or double click goes to it
    Palette,
    // the sidebar's Thumbnails view: a click goes to a page
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

    PageThumbnailsCtrl(MainWindow*, PlatformFont*, int dpi, ThumbnailsHost);
    ~PageThumbnailsCtrl() override;

    void SetTab(WindowTab*);
    void SetBounds(Rect) override;
    void DrawRow(DrawItemEvent*);
    void OnThumbMouseDown(VirtMouseEvent*);
    void OnThumbMouseMove(VirtMouseEvent*);
    void OnThumbMouseWheel(VirtMouseEvent*);
    void OnThumbDoubleClick(VirtMouseEvent*);
    void OnThumbKeyDown(VirtKeyEvent*);
    void Activate();
    void Deactivate();
    void HandleKey(int vkey);
    void StartRendering();
    void Refresh();
    void RefreshPage(int pageNo);
    int RenderedCount() const;
    void SelectPage(int);
    void SetCurrentPage(int);
    Rect PageRect(int pageNo);

  protected:
    int PageAtPoint(Point);
    void OpenSelectedPage();
    void ResetCache();
};

Pixmap* RenderPageThumbnail(EngineBase*, int pageNo, Location, int rotation, int thumbDx, int thumbDy);
