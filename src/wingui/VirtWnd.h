/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// A VirtWnd is a "virtual control": a tree of them lives inside a single HWND,
// each taking up a part of it. VirtWndRoot bridges the tree to the HWND: it
// drives layout, painting and message dispatch (hover / capture / focus).
//
// A VirtWnd is an ILayout (Layout.h), so it composes with VBox / HBox /
// Padding / VirtWndTable. Its `bounds` are relative to the parent's content
// origin; painting and hit-testing carry an origin down the tree, which is what
// makes VirtWndScroll cheap (shift the origin, don't re-layout the subtree).

// needs wingui/PlatformFont.h (PlatformFont) and wingui/Gfx.h (Gfx)
// included before it

struct VirtWnd;
struct VirtWndRoot;
struct Pixmap;

enum VirtWndFlags : u32 {
    vwfEnabled = 1 << 0,
    vwfFocusable = 1 << 1,
    vwfFocused = 1 << 2,
    vwfHovered = 1 << 3,
    vwfPressed = 1 << 4,
    // keeps the mouse during a drag; while captured, VirtWndMouseEvent::pt is
    // in window coords because the mouse can be outside the wnd's bounds
    vwfCapturesMouse = 1 << 5,
    vwfClipChildren = 1 << 6,
    // the wnd paints and hit-tests its children itself (virtualized lists)
    vwfPaintsOwnChildren = 1 << 7,
    // decorative: never a hit-test target (children still are)
    vwfNoHitTest = 1 << 8,
    vwfSkipTabStop = 1 << 9,
};

// all rects are in HWND client coords
struct VirtWndPaintCtx {
    Gfx* gfx = nullptr;
    Rect bounds;  // the wnd being painted
    Rect content; // bounds deflated by padding
    Rect clip;    // intersection of the clip rects of all ancestors
};

struct VirtWndMouseEvent {
    // the wnd currently being offered the event; changes as it bubbles up
    VirtWnd* target = nullptr;
    // the wnd the mouse actually hit; stays the same while bubbling
    VirtWnd* hit = nullptr;
    // relative to target's bounds; in window coords while captured
    Point pt;
    Point ptWindow;
    int button = 0; // 0 left, 1 right, 2 middle
    int wheelDelta = 0;
    bool isCtrl = false;
    bool isShift = false;
    bool isAlt = false;
};

struct VirtWndKeyEvent {
    VirtWnd* target = nullptr;
    int vkey = 0;
    bool isCtrl = false;
    bool isShift = false;
    bool isAlt = false;
};

using VirtWndMouseHandler = Func1<VirtWndMouseEvent*>;
using VirtWndPaintHandler = Func1<VirtWndPaintCtx*>;

struct VirtWnd : LayoutBase {
    VirtWnd* parent = nullptr;
    VirtWndRoot* root = nullptr;
    Vec<VirtWnd*> children; // owned

    u32 flags = vwfEnabled;
    // relative to parent's content origin
    Rect bounds;
    Insets padding{};
    int id = 0;
    uintptr_t userData = 0;
    // for debugging; not owned
    Str name;

    VirtWnd();
    ~VirtWnd() override;

    // ILayout
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    Size Layout(Constraints bc) override;
    void SetBounds(Rect) override;

    virtual Size GetIdealSize();

    virtual void Paint(VirtWndPaintCtx&);
    virtual void PaintChildren(VirtWndPaintCtx&);
    void PaintTree(Gfx*, Point origin, Rect clip);
    void PaintStandalone(Gfx*);

    virtual bool HitTest(Point ptLocal);
    // scroll containers return how much their content is scrolled
    virtual Point ScrollOffset();
    VirtWnd* WndFromPoint(Point ptWindow, Point* ptLocalOut);

    // return true to consume the event, false to let it bubble to the parent
    virtual bool OnMouseDown(VirtWndMouseEvent&);
    virtual bool OnMouseUp(VirtWndMouseEvent&);
    virtual bool OnMouseMove(VirtWndMouseEvent&);
    virtual bool OnMouseWheel(VirtWndMouseEvent&);
    virtual bool OnDoubleClick(VirtWndMouseEvent&);
    virtual bool OnContextMenu(VirtWndMouseEvent&);
    virtual void OnMouseEnter();
    virtual void OnMouseLeave();
    virtual void OnCaptureLost();
    virtual bool OnKeyDown(VirtWndKeyEvent&);
    virtual bool OnChar(int c);
    virtual void OnFocusChanged(bool gotFocus);
    virtual bool OnSetCursor(Point ptLocal);
    virtual TempStr GetTooltipTemp(Point ptLocal);

    void AddChild(VirtWnd*);
    void InsertChild(VirtWnd*, int idx);
    void RemoveChild(VirtWnd*, bool del = true);
    void RemoveAllChildren(bool del = true);
    int ChildCount() const;
    VirtWnd* ChildAt(int idx) const;
    VirtWnd* FindById(int);

    Point OriginInWindow();
    Point ChildOriginInWindow();
    Rect BoundsInWindow();
    Rect ContentRectInWindow();
    Rect VisibleRectInWindow();

    void Invalidate();
    void Invalidate(Rect rLocal);
    void RequestLayout();

    HWND GetHwnd() const;
    bool IsPaintable() const;
    bool IsHitTestable() const;
    void SetFlag(u32 f, bool on);
    bool HasFlag(u32 f) const;
    void SetRoot(VirtWndRoot*);
};

bool IsVirtWndOfKind(VirtWnd*, Kind);

struct VirtWndRoot {
    HWND hwnd = nullptr;
    VirtWnd* child = nullptr; // owned
    // part of hwnd occupied by the tree
    Rect bounds;

    VirtWnd* hovered = nullptr;
    VirtWnd* captured = nullptr;
    VirtWnd* focused = nullptr;
    VirtWnd* pressed = nullptr;

    bool needsLayout = true;
    bool trackingMouseLeave = false;

    explicit VirtWndRoot(HWND);
    ~VirtWndRoot();

    void SetChild(VirtWnd*);
    void SetBounds(Rect);
    void LayoutIfNeeded();
    void RequestLayout();
    void Paint(Gfx*, Rect clip);

    // single entry point from the owning WndProc, returns false if not handled
    bool OnMessage(UINT msg, WPARAM, LPARAM, LRESULT& res);

    VirtWnd* WndFromPoint(Point ptWindow, Point* ptLocalOut);
    void SetFocus(VirtWnd*);
    bool TabNavigate(bool backwards);
    void SetCapture(VirtWnd*);
    void ReleaseCapture();
    void ClearHover();
    void ClearPressed();
    void OnWndDestroyed(VirtWnd*);
    void Invalidate(Rect rWindow);
    void TrackMouseLeaveIfNeeded();
};

//--- containers

struct VirtWndBox : VirtWnd {
    bool isVertical = true;
    MainAxisAlign alignMain = MainAxisAlign::MainStart;
    CrossAxisAlign alignCross = CrossAxisAlign::CrossStart;

    explicit VirtWndBox(bool isVertical = true);
    ~VirtWndBox() override;

    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    Size Layout(Constraints bc) override;
    void SetBounds(Rect) override;

    void AddChild(VirtWnd*, int flex = 0);

  private:
    VBox* vbox = nullptr;
    HBox* hbox = nullptr;
    Vec<int> flexes;

    void RebuildBox();
    ILayout* Box();
};

// one cell of a VirtWndTable. alignH / alignV say where the child sits when the
// cell is bigger than the child; CrossAxisAlign::Stretch makes the child fill
// the cell in that direction
struct VirtWndTableCell {
    // owned, as one of the table's VirtWnd children
    VirtWnd* child = nullptr;
    int rowSpan = 1;
    int colSpan = 1;
    CrossAxisAlign alignH = CrossAxisAlign::CrossStart;
    CrossAxisAlign alignV = CrossAxisAlign::CrossStart;
    // covered by a cell that spans into it, so it can't hold a child of its own
    bool covered = false;
    // the child's size, measured by Layout()
    Size childSize;
};

// a grid of rows x cols cells, each holding a VirtWnd. A column is as wide as
// its widest cell and a row as tall as its tallest; a cell can span several
// rows and / or columns
struct VirtWndTable : VirtWnd {
    int rows = 0;
    int cols = 0;
    // space between adjacent columns / rows
    int colGap = 0;
    int rowGap = 0;

    VirtWndTable();
    ~VirtWndTable() override;

    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    Size Layout(Constraints bc) override;
    void SetBounds(Rect) override;

    void SetSize(int rows, int cols);
    VirtWndTableCell& SetCell(int row, int col, VirtWnd* child, int rowSpan = 1, int colSpan = 1);
    VirtWndTableCell* CellAt(int row, int col);
    VirtWnd* GetCell(int row, int col);
    void RemoveAllCells();

    // valid after a Layout() / SetBounds() pass
    int ColWidth(int col);
    int RowHeight(int row);
    Rect CellRect(int row, int col);

  private:
    Vec<VirtWndTableCell> cells; // rows * cols, row-major
    Vec<int> colWidths;
    Vec<int> rowHeights;

    int CellIdx(int row, int col) const;
    void MarkCovered(int row, int col, int rowSpan, int colSpan, bool covered);
    void Measure();
    Size TotalSize();
    Rect ContentRect();
};

// tells the owner which part of the content is visible so it can create
// only the wnds that are needed (list virtualization)
struct VirtWndScrollRange {
    VirtWnd* wnd = nullptr;
    int visibleY = 0;
    int visibleDy = 0;
};

struct VirtWndScroll : VirtWnd {
    int scrollY = 0;
    int contentDy = 0;
    int lineDy = 16;
    // when set, syncs the HWND's vertical scrollbar
    bool syncScrollbar = false;
    Func1<VirtWndScrollRange*> onVisibleRangeChanged;

    VirtWndScroll();
    ~VirtWndScroll() override;

    Size Layout(Constraints bc) override;
    void SetBounds(Rect) override;
    Point ScrollOffset() override;
    bool OnMouseWheel(VirtWndMouseEvent&) override;

    void SetContentDy(int);
    int MaxScrollY() const;
    bool ScrollTo(int y);
    bool ScrollBy(int dy);
    bool ScrollPage(int dir);
    void ScrollIntoView(VirtWnd*);
    void OnVScroll(WPARAM);

  private:
    int lastNotifiedY = -1;
    int lastNotifiedDy = -1;

    void UpdateScrollbar();
    void NotifyVisibleRange();
};

struct VirtWndCustom : VirtWnd {
    Size idealSize;
    VirtWndPaintHandler onPaint;
    VirtWndMouseHandler onClick;

    VirtWndCustom();
    ~VirtWndCustom() override;

    Size GetIdealSize() override;
    void Paint(VirtWndPaintCtx&) override;
    bool OnMouseUp(VirtWndMouseEvent&) override;
};

//--- controls

enum class VirtWndTextAlign {
    Left,
    Center,
    Right
};

struct VirtWndText : VirtWnd {
    Str s;
    PlatformFont* font = nullptr; // not owned, interned
    bool withUnderline = false;
    bool isRtl = false;
    bool ellipsis = false;
    // nudges the underline off the text baseline box
    int underlineOffsetY = 0;
    VirtWndTextAlign align = VirtWndTextAlign::Left;
    COLORREF textColor = kColorUnset;

    Size sz = {0, 0};

    VirtWndText(Str s, PlatformFont* font = nullptr);
    ~VirtWndText() override;

    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    Size Layout(Constraints bc) override;
    Size GetIdealSize() override;

    Size MinIntrinsicSize(int width, int height);
    Size GetIdealSize(bool onlyIfEmpty);
    void SetText(Str);

    void Paint(VirtWndPaintCtx&) override;
};

struct VirtWndLink : VirtWndText {
    Str target;  // owned
    Str tooltip; // owned
    VirtWndMouseHandler onClick;
    bool underlineOnHover = false;

    VirtWndLink(Str s, PlatformFont* font = nullptr);
    ~VirtWndLink() override;

    void SetTarget(Str);
    void SetTooltip(Str);

    void Paint(VirtWndPaintCtx&) override;
    void OnMouseEnter() override;
    void OnMouseLeave() override;
    bool OnMouseDown(VirtWndMouseEvent&) override;
    bool OnMouseUp(VirtWndMouseEvent&) override;
    bool OnSetCursor(Point ptLocal) override;
    TempStr GetTooltipTemp(Point ptLocal) override;
};

struct VirtWndButton : VirtWndText {
    COLORREF bgColor = kColorUnset;
    COLORREF bgColorHover = kColorUnset;
    Insets textPadding{4, 8, 4, 8};
    VirtWndMouseHandler onClick;

    VirtWndButton(Str s, PlatformFont* font = nullptr);
    ~VirtWndButton() override;

    Size GetIdealSize() override;
    void Paint(VirtWndPaintCtx&) override;
    void OnMouseEnter() override;
    void OnMouseLeave() override;
    bool OnMouseDown(VirtWndMouseEvent&) override;
    bool OnMouseUp(VirtWndMouseEvent&) override;
    bool OnSetCursor(Point ptLocal) override;
};

struct VirtWndIconButton : VirtWnd {
    // not owned; on Windows usually from IconPixmapFromImageList()
    Pixmap* pixmap = nullptr;
    bool isSelected = false;
    Str tooltip; // owned
    VirtWndMouseHandler onClick;

    VirtWndIconButton();
    ~VirtWndIconButton() override;

    void SetTooltip(Str);

    Size GetIdealSize() override;
    void Paint(VirtWndPaintCtx&) override;
    void OnMouseEnter() override;
    void OnMouseLeave() override;
    bool OnMouseDown(VirtWndMouseEvent&) override;
    bool OnMouseUp(VirtWndMouseEvent&) override;
    bool OnSetCursor(Point ptLocal) override;
    TempStr GetTooltipTemp(Point ptLocal) override;
};

struct VirtWndImage : VirtWnd {
    Pixmap* pixmap = nullptr; // not owned
    // scale the image down to fit, keeping the aspect ratio
    bool fitToBounds = true;

    VirtWndImage();
    ~VirtWndImage() override;

    Size GetIdealSize() override;
    void Paint(VirtWndPaintCtx&) override;
};

struct VirtWndFill : VirtWnd {
    COLORREF color = kColorUnset;
    Size idealSize;

    VirtWndFill();
    ~VirtWndFill() override;

    Size GetIdealSize() override;
    void Paint(VirtWndPaintCtx&) override;
};

struct VirtWndLine : VirtWnd {
    COLORREF color = kColorUnset;
    bool isVertical = false;
    int thickness = 1;

    VirtWndLine();
    ~VirtWndLine() override;

    Size GetIdealSize() override;
    void Paint(VirtWndPaintCtx&) override;
};

struct VirtWndSpacer : VirtWnd {
    Size idealSize;

    VirtWndSpacer(int dx, int dy);
    ~VirtWndSpacer() override;

    Size GetIdealSize() override;
};

Rect FitSizeInRect(Size src, Rect dst);
