/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// A VirtCtrl is a "virtual control": a tree of them lives inside a single HWND,
// each taking up a part of it. VirtRoot bridges the tree to the HWND: it
// drives layout, painting and message dispatch (hover / capture / focus).
//
// A VirtCtrl is an ILayout (Layout.h), so it composes with VBox / HBox /
// Padding / Table. Its `bounds` are relative to the parent's content
// origin; painting and hit-testing carry an origin down the tree, which is what
// makes VirtScroll cheap (shift the origin, don't re-layout the subtree).

// needs gui/PlatformFont.h (PlatformFont), gui/Gfx.h (Gfx) and
// gui/GuiColors.h (the color enums) included before it

struct VirtCtrl;
struct VirtRoot;
struct Pixmap;
struct GfxDoubleBuffer;
struct WindowBase;
struct ControlBase;
struct Tooltip;

enum VirtFlags : u32 {
    vwfEnabled = 1 << 0,
    vwfFocusable = 1 << 1,
    vwfFocused = 1 << 2,
    vwfHovered = 1 << 3,
    vwfPressed = 1 << 4,
    // keeps the mouse during a drag; while captured, VirtMouseEvent::pt is
    // in window coords because the mouse can be outside the wnd's bounds
    vwfCapturesMouse = 1 << 5,
    vwfClipChildren = 1 << 6,
    // the wnd paints and hit-tests its children itself (virtualized lists)
    vwfPaintsOwnChildren = 1 << 7,
    // decorative: never a hit-test target (children still are)
    vwfNoHitTest = 1 << 8,
    vwfSkipTabStop = 1 << 9,
};

enum VirtHitFlags : u32 {
    vhfIncludeDisabled = 1 << 0,
};

ILayout* ElementFromPoint(ILayout* root, Point ptWindow, Point* ptLocalOut = nullptr, u32 flags = 0);
ILayout* ElementFromPoint(VirtRoot* root, Point ptWindow, Point* ptLocalOut = nullptr, u32 flags = 0);

// all rects are in HWND client coords
struct VirtPaintCtx {
    Gfx* gfx = nullptr;
    Rect bounds;  // the wnd being painted
    Rect content; // bounds deflated by padding
    Rect clip;    // intersection of the clip rects of all ancestors
};

struct VirtMouseEvent {
    // the wnd currently being offered the event; changes as it bubbles up
    VirtCtrl* target = nullptr;
    // the wnd the mouse actually hit; stays the same while bubbling
    VirtCtrl* hit = nullptr;
    // relative to target's bounds; in window coords while captured
    Point pt;
    Point ptWindow;
    int button = 0; // 0 left, 1 right, 2 middle
    int wheelDelta = 0;
    bool isCtrl = false;
    bool isShift = false;
    bool isAlt = false;
    // true = consume (stop bubbling)
    bool didHandle = false;
};

struct VirtKeyEvent {
    VirtCtrl* target = nullptr;
    int vkey = 0;
    bool isCtrl = false;
    bool isShift = false;
    bool isAlt = false;
    bool didHandle = false;
};

struct VirtSetCursorEvent {
    VirtCtrl* w = nullptr;
    Point ptLocal;
    bool didHandle = false;
};

struct VirtCharEvent {
    VirtCtrl* w = nullptr;
    int c = 0;
    bool didHandle = false;
};

struct VirtFocusEvent {
    VirtCtrl* w = nullptr;
    bool gotFocus = false;
};

struct VirtTooltipEvent {
    VirtCtrl* w = nullptr;
    Point ptLocal;
    TempStr tip; // set by handler
};

using VirtMouseHandler = Func1<VirtMouseEvent*>;
using VirtKeyHandler = Func1<VirtKeyEvent*>;
using VirtSetCursorHandler = Func1<VirtSetCursorEvent*>;
using VirtCharHandler = Func1<VirtCharEvent*>;
using VirtFocusHandler = Func1<VirtFocusEvent*>;
using VirtTooltipHandler = Func1<VirtTooltipEvent*>;
using VirtPaintHandler = Func1<VirtPaintCtx*>;

struct VirtCtrl : ILayout {
    VirtCtrl* parent = nullptr;
    VirtRoot* root = nullptr;
    Vec<VirtCtrl*> children; // owned

    u32 flags = vwfEnabled;
    // relative to parent's content origin
    Rect bounds;
    Insets padding{};
    int id = 0;
    uintptr_t userData = 0;
    // keyboard mnemonic ("&File" => 'F', 0 = none). Parsed when the text is
    // set, so readers (mnemonic navigation in WindowBase) don't re-parse the
    // text every time
    char mnemonic = 0;
    // for debugging; not owned
    Str name;
    // static tooltip (owned); used when onGetTooltip is empty
    Str tooltip;
    // default cursor; used when onSetCursor is empty
    CursorId cursor = CursorId::None;

    // per-instance color overrides, indexed by the class's kCol* enum. Owned,
    // allocated on demand by SetColor(); kColorUnset in a slot means "use the
    // class default". Null until someone overrides a color, which is the common
    // case: a control normally paints in its class's global colors
    Color* colors = nullptr;
    // the class's global default colors (a gCols* array) and how many entries
    // both arrays have. Set by the class's constructor
    const Color* colorDefaults = nullptr;
    int nColors = 0;

    VirtCtrl();
    ~VirtCtrl() override;

    void SetTooltip(Str);

    Color GetColor(int idx) const;
    void SetColor(int idx, Color);
    void ResetColors();

    // ILayout
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    Size Layout(Constraints bc) override;
    void SetBounds(Rect) override;
    int LayoutChildCount() override;
    ILayout* LayoutChildAt(int) override;
    VirtCtrl* AsVirtCtrl() override;

    virtual Size GetIdealSize();

    virtual void Paint(VirtPaintCtx&);
    virtual void PaintChildren(VirtPaintCtx&);
    void PaintTree(Gfx*, Point origin, Rect clip);
    void PaintStandalone(Gfx*);

    virtual bool HitTest(Point ptLocal);
    // scroll containers return how much their content is scrolled
    virtual Point ScrollOffset();

    // dispatch to on* handlers; return true to consume (stop bubbling)
    bool OnMouseDown(VirtMouseEvent&);
    bool OnMouseUp(VirtMouseEvent&);
    bool OnMouseMove(VirtMouseEvent&);
    bool OnMouseWheel(VirtMouseEvent&);
    bool OnDoubleClick(VirtMouseEvent&);
    bool OnContextMenu(VirtMouseEvent&);
    void OnMouseEnter();
    void OnMouseLeave();
    void OnCaptureLost();
    bool OnKeyDown(VirtKeyEvent&);
    bool OnChar(int c);
    void OnFocusChanged(bool gotFocus);
    bool OnSetCursor(Point ptLocal);
    TempStr GetTooltipTemp(Point ptLocal);

    void AddChild(VirtCtrl*);
    void InsertChild(VirtCtrl*, int idx);
    void RemoveChild(VirtCtrl*, bool del = true);
    void RemoveAllChildren(bool del = true);
    int ChildCount() const;
    VirtCtrl* ChildAt(int idx) const;
    VirtCtrl* FindById(int);

    Point OriginInWindow();
    Point ChildOriginInWindow();
    Rect BoundsInWindow();
    Rect ContentRectInWindow();
    Rect VisibleRectInWindow();

    void Invalidate();
    void Invalidate(Rect rLocal);
    void RequestLayout();

    // same names as ControlBase's, so code that shows / hides / disables a
    // control doesn't care which kind it got
    void SetIsVisible(bool);
    bool IsVisible() const;
    void SetIsEnabled(bool);
    bool IsEnabled() const;

    HWND GetHwnd() const;
    bool IsPaintable() const;
    bool IsHitTestable() const;
    void SetFlag(u32 f, bool on);
    bool HasFlag(u32 f) const;
    void SetRoot(VirtRoot*);

    // input / focus / cursor (wire with MkMethod1 in ctors; paint stays virtual)
    // high-level click: if set and onMouseUp is not, mouse down/up call this
    VirtMouseHandler onClick;
    VirtMouseHandler onMouseDown;
    VirtMouseHandler onMouseUp;
    VirtMouseHandler onMouseMove;
    VirtMouseHandler onMouseWheel;
    VirtMouseHandler onDoubleClick;
    VirtMouseHandler onContextMenu;
    Func0 onMouseEnter;
    Func0 onMouseLeave;
    Func0 onCaptureLost;
    VirtKeyHandler onKeyDown;
    VirtCharHandler onChar;
    VirtFocusHandler onFocusChanged;
    // dynamic cursor (overrides cursor when set)
    VirtSetCursorHandler onSetCursor;
    // dynamic tip (overrides tooltip when set)
    VirtTooltipHandler onGetTooltip;
};

bool IsVirtCtrlOfKind(VirtCtrl*, Kind);

// The virtual controls at the top of a layout tree: nodes whose ancestors are
// all plain layouts. Doesn't descend into a VirtCtrl's own children - it paints
// and hit-tests those itself. A tree of only HWND controls yields none
void CollectVirtCtrls(ILayout* root, Vec<VirtCtrl*>& out);

// One stop in a window's Tab ring: exactly one of the two is set. The ring is
// the layout order, so an Edit and a virtual button sitting in the same VBox
// are reached one after the other
struct TabStop {
    ControlBase* ctrl = nullptr;
    VirtCtrl* vwnd = nullptr;
};

// the tab stops of a layout tree, in layout order: HWND controls that have
// WS_TABSTOP and are visible + enabled, and virtual controls that are
// vwfFocusable and not vwfSkipTabStop
void CollectTabStops(ILayout* root, Vec<TabStop>& out);

//--- hosting virtual controls in a window
//
// A window (or a control) that has a layout tree drives its virtual controls
// through these three: lay out, paint, dispatch input. `*rootInOut` is created
// on demand and stays null while the tree has no virtual controls at all - the
// common case of a window made only of HWND controls costs nothing

// runs the layout, then refreshes the root with the tree's top-level virtual
// controls. Never lays out from a paint: it would move child HWNDs mid-paint
void LayoutTreeToSize(HWND, ILayout* layout, Size, VirtRoot** rootInOut);
// same, for a host that positions the tree itself: only refreshes the root
void RefreshVirtTops(HWND, ILayout* layout, Rect bounds, VirtRoot** rootInOut);
// double-buffered; fills bg first. Does nothing if there's nothing virtual
void PaintVirtTree(VirtRoot*, HDC, Rect clip, Color bg);
// paint + input in one call, for a plain HWND that isn't a WindowBase /
// ControlBase (the side panels are subclassed WC_STATICs). Returns true when
// the message was handled
bool VirtHostOnMessage(HWND, VirtRoot*, UINT, WPARAM, LPARAM, LRESULT&, Color bg);
// the single entry point for input: mouse, cursor, tooltips and keyboard, with
// RTL mouse coordinates unmirrored
bool VirtTreeOnMessage(HWND, VirtRoot*, UINT, WPARAM, LPARAM, LRESULT&);

// The virtual controls of one HWND: the window paints them and hands them its
// input. They are the top-level virtual nodes of the window's layout tree, so
// there can be any number of them - a VBox of an Edit and two VirtTexts leaves
// two - and they are not owned here; the layout tree owns them
struct VirtRoot {
    HWND hwnd = nullptr;
    GfxDoubleBuffer* gfxBuf = nullptr;
    Vec<VirtCtrl*> tops;
    // set only by SetChild(), which owns what it is given
    VirtCtrl* owned = nullptr;
    // part of hwnd occupied by the tree
    Rect bounds;

    VirtCtrl* hovered = nullptr;
    VirtCtrl* captured = nullptr;
    VirtCtrl* focused = nullptr;
    VirtCtrl* pressed = nullptr;
    Tooltip* tooltip = nullptr;
    VirtCtrl* tooltipWnd = nullptr; // control the tip is currently showing for

    bool needsLayout = true;
    // legacy single-tree hosts lay out lazily from Paint(); see SetChild()
    bool layoutInPaint = false;
    bool trackingMouseLeave = false;

    explicit VirtRoot(HWND);
    ~VirtRoot();

    // takes ownership; for a window whose whole content is one virtual tree
    void SetChild(VirtCtrl*);
    // the tops found in a layout tree; not owned
    void SetTops(const Vec<VirtCtrl*>&);
    void SetBounds(Rect);
    void LayoutIfNeeded();
    void RequestLayout();
    void Paint(Gfx*, Rect clip);

    // single entry point from the owning WndProc, returns false if not handled
    bool OnMessage(UINT msg, WPARAM, LPARAM, LRESULT& res);

    void UpdateTooltip(Point ptWindow);
    void HideTooltip();
    void SetFocus(VirtCtrl*);
    bool TabNavigate(bool backwards);
    void SetCapture(VirtCtrl*);
    void ReleaseCapture();
    void ClearHover();
    void ClearPressed();
    void OnWndDestroyed(VirtCtrl*);
    void Invalidate(Rect rWindow);
    void TrackMouseLeaveIfNeeded();
};

//--- containers

// tells the owner which part of the content is visible so it can create
// only the wnds that are needed (list virtualization)
struct VirtScrollRange {
    VirtCtrl* wnd = nullptr;
    int visibleY = 0;
    int visibleDy = 0;
};

struct VirtScroll : VirtCtrl {
    int scrollY = 0;
    int contentDy = 0;
    int lineDy = 16;
    // when set, syncs the HWND's vertical scrollbar
    bool syncScrollbar = false;
    Func1<VirtScrollRange*> onVisibleRangeChanged;

    VirtScroll();
    ~VirtScroll() override;

    Size Layout(Constraints bc) override;
    void SetBounds(Rect) override;
    Point ScrollOffset() override;
    void OnMouseWheel(VirtMouseEvent*);

    void SetContentDy(int);
    int MaxScrollY() const;
    bool ScrollTo(int y);
    bool ScrollBy(int dy);
    bool ScrollPage(int dir);
    void ScrollIntoView(VirtCtrl*);
    void OnVScroll(WPARAM);

  private:
    int lastNotifiedY = -1;
    int lastNotifiedDy = -1;

    void UpdateScrollbar();
    void NotifyVisibleRange();
};

// Scrolls an ILayout child (VBox/HBox/Table) inside a clipped viewport.
struct ScrollBox : VirtCtrl {
    ILayout* child = nullptr; // owned
    int scrollY = 0;
    Size contentSize;
    int lineDy = 16;
    bool syncScrollbar = true;

    explicit ScrollBox(ILayout* child);
    ~ScrollBox() override;

    Size Layout(Constraints bc) override;
    void SetBounds(Rect) override;
    Point ScrollOffset() override;
    void Paint(VirtPaintCtx&) override;
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    Size GetIdealSize() override;
    int LayoutChildCount() override;
    ILayout* LayoutChildAt(int) override;

    int MaxScrollY() const;
    bool ScrollTo(int y);
    bool ScrollBy(int dy);
    bool ScrollPage(int dir);
    void OnMouseWheel(VirtMouseEvent*);
    void OnVScroll(WPARAM);

  private:
    void UpdateScrollbar();
};

// A list of rows: it owns the model, the selection and the scroll position, and
// paints only the rows that are visible. The virtual counterpart of the HWND
// ListBox, minus the win32 listbox's habits (it doesn't steal the keyboard
// focus when clicked, and it repaints as one piece instead of scrolling pixels)
struct VirtListBox : VirtCtrl {
    struct DrawItemEvent {
        VirtListBox* listBox = nullptr;
        Gfx* gfx = nullptr;
        // the whole row, in window coords, without the scrollbar strip
        Rect itemRect;
        int itemIndex = -1;
        bool selected = false;
    };

    using SelectionChangedHandler = Func0;
    using DoubleClickHandler = Func0;
    using DrawItemHandler = Func1<DrawItemEvent*>;

    ListBoxModel* model = nullptr; // owned
    SelectionChangedHandler onSelectionChanged;
    DoubleClickHandler onDoubleClick;
    // when not set, rows are drawn as plain text
    DrawItemHandler onDrawItem;

    PlatformFont* font = nullptr; // not owned, interned
    // colors: kColList* (kColListSel / kColListScrollbar are derived from
    // kColListBg when left unset)

    // how many rows GetIdealSize() asks for; 0 means "as many as there are",
    // capped at 16
    int idealSizeLines = 0;
    // width GetIdealSize() asks for; 0 means a default
    int idealSizeDx = 0;
    // 0 means "derive from the font"
    int itemDy = 0;
    // for scaling before the tree is attached to a window (GetHwnd() is null
    // until then); once it is, the window's own dpi wins
    int dpi = 96;

    int scrollY = 0;
    // Shift/Ctrl click, Shift+arrows and Ctrl+A; off by default so other
    // lists stay single-select
    bool multiSelect = false;

    VirtListBox();
    ~VirtListBox() override;

    Size GetIdealSize() override;
    void SetBounds(Rect) override;
    void Paint(VirtPaintCtx&) override;
    void OnMouseDown(VirtMouseEvent*);
    void OnMouseUp(VirtMouseEvent*);
    void OnMouseMove(VirtMouseEvent*);
    void OnMouseWheel(VirtMouseEvent*);
    void OnDoubleClick(VirtMouseEvent*);
    void OnKeyDown(VirtKeyEvent*);
    void OnCaptureLost();

    // for efficiency you can re-use the model: get it, change the data, call
    // SetModel() again
    void SetModel(ListBoxModel*);
    int ItemsCount();
    int GetItemHeight();
    int GetCurrentSelection();
    // -1 clears the selection; doesn't call onSelectionChanged
    bool SetCurrentSelection(int);
    bool IsSelected(int idx);
    int SelectedCount();
    void GetSelectedIndices(Vec<int>& out);
    void SelectAll();
    void SelectRange(int from, int to);
    void ToggleSelected(int idx);
    int ItemFromPoint(Point ptLocal);
    // the row's rectangle in window coords; empty when the row isn't visible
    Rect ItemRect(int idx);
    void EnsureVisible(int idx);
    int ViewportDy();
    // ViewportDy() rounded down to whole rows
    int UsableDy();
    int MaxScrollY();
    bool ScrollTo(int y);
    bool ScrollBy(int dy);

  private:
    int selIdx = -1;
    int anchorIdx = -1;
    Vec<u8> selected;
    // EnsureVisible() called before the first layout; applied by SetBounds()
    int pendingVisibleIdx = -1;
    bool draggingThumb = false;
    // where the thumb drag started, in window coords, and the scroll position
    // it started from
    int dragStartY = 0;
    int dragStartScrollY = 0;

    void EnsureSelectedSize();
    void ApplyClick(int idx, bool ctrl, bool shift);
    void ApplyNav(int idx, bool ctrl, bool shift);
    int GetDpi();
    int ScrollbarDx();
    Rect ContentRectLocal();
    Rect ItemsRectLocal();
    Rect ScrollbarRectLocal();
    Rect ThumbRectLocal();
    bool SelectAndNotify(int idx);
};

enum class SplitterType {
    Horiz,
    Vert,
};

// The drag handle between two panes, e.g. the sidebar and the document. While
// dragging it captures the mouse; a non-live splitter shows a dotted popup
// where the split would land and only moves the panes on release.
struct VirtSplitter : VirtCtrl {
    struct MoveEvent {
        VirtSplitter* w = nullptr;
        bool finishedDragging = false;
        // SetCursor asks whether this position is allowed (IDC_NO) without
        // moving the panes. A live onMove that relayouts here shimmers.
        bool queryOnly = false;
        // the owner sets this to false to forbid resizing to here
        bool resizeAllowed = true;
    };

    using MoveHandler = Func1<MoveEvent*>;

    SplitterType type = SplitterType::Horiz;
    // false: the panes only move when the drag ends
    bool isLive = true;
    // colors: kColSplitterBg
    // how thick the bar is; the other axis is stretched by the layout.
    // 0 keeps whatever bounds it was given
    int thickness = 0;
    MoveHandler onMove;

    VirtSplitter();
    ~VirtSplitter() override;

    Size GetIdealSize() override;

    void Paint(VirtPaintCtx&) override;
    void OnMouseDown(VirtMouseEvent*);
    void OnMouseUp(VirtMouseEvent*);
    void OnMouseMove(VirtMouseEvent*);
    void OnMouseEnter();
    void OnMouseLeave();
    void OnCaptureLost();
    void OnSetCursor(VirtSetCursorEvent*);

  private:
    // the dotted popup of a non-live drag; above the child windows, which is
    // why it is a window of its own rather than something we paint
    HWND overlayHwnd = nullptr;
    HBITMAP bmp = nullptr;
    HBRUSH brush = nullptr;
    bool isDragging = false;
    Point lastDragPos{-1, -1};

    void UpdateOverlay();
    void HideOverlay();
};

struct VirtCustom : VirtCtrl {
    Size idealSize;
    VirtPaintHandler onPaint;

    VirtCustom();
    ~VirtCustom() override;

    Size GetIdealSize() override;
    void Paint(VirtPaintCtx&) override;
};

//--- controls

enum class VirtTextAlign {
    Left,
    Center,
    Right
};

// VirtText itself can't be an aggregate - it inherits VirtCtrl, which has
// virtual functions and a constructor - so designated initializers go through
// this, like the CreateArgs of the HWND controls:
//   auto* t = NewVirtText({.s = path, .font = font, .ellipsis = true});
struct VirtTextArgs {
    Str s;
    PlatformFont* font = nullptr; // not owned, interned
    Color textColor = kColorUnset;
    VirtTextAlign align = VirtTextAlign::Left;
    bool withUnderline = false;
    bool isRtl = false;
    bool ellipsis = false;
    // "…" in the middle instead of at the end, for paths
    bool pathEllipsis = false;
    // "&F" → F underlined, "&&" → "&" (win32 STATIC / BUTTON)
    bool prefix = false;
    // nudges the underline off the text baseline box
    int underlineOffsetY = 0;
    Insets padding{};
};

struct VirtText : VirtCtrl {
    Str s;
    PlatformFont* font = nullptr; // not owned, interned
    // colors: kColText
    bool withUnderline = false;
    bool isRtl = false;
    bool ellipsis = false;
    bool pathEllipsis = false;
    // "&F" → F underlined, "&&" → "&" (win32 STATIC / BUTTON)
    bool prefix = false;
    // nudges the underline off the text baseline box
    int underlineOffsetY = 0;
    VirtTextAlign align = VirtTextAlign::Left;

    Size sz = {0, 0};

    VirtText(Str s, PlatformFont* font = nullptr);
    ~VirtText() override;

    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    Size Layout(Constraints bc) override;
    Size GetIdealSize() override;

    Size MinIntrinsicSize(int width, int height);
    Size GetIdealSize(bool onlyIfEmpty);
    void SetText(Str);

    void Paint(VirtPaintCtx&) override;
    // the drawing, in a color the caller picks (a disabled button's text)
    void PaintText(VirtPaintCtx&, Color);
};

VirtText* NewVirtText(const VirtTextArgs&);

char MnemonicCharInStr(Str s);

// Checked downcasts: null unless the layout really is that control. Code that
// walks a layout tree it didn't build (the toolbar, whose items are a mix of
// icon buttons, text buttons, labels and separators) must go through these
// instead of casting on faith.
VirtText* AsVirtText(ILayout*);

struct VirtLink : VirtText {
    Str target; // owned
    bool underlineOnHover = false;

    VirtLink(Str s, PlatformFont* font = nullptr);
    ~VirtLink() override;

    void SetTarget(Str);

    void Paint(VirtPaintCtx&) override;
    void OnMouseEnter();
    void OnMouseLeave();
};

struct VirtButton : VirtText {
    // colors: kColBtn*, defaulting to gColsBtnDefault when isDefault
    Insets textPadding{.top = 4, .right = 8, .bottom = 4, .left = 8};

    VirtButton(Str s, PlatformFont* font = nullptr);
    ~VirtButton() override;

    // Enter clicks a default button when the focus is not on another one, and
    // it is painted a shade stronger (gColsBtnDefault)
    bool IsDefault() const;
    void SetIsDefault(bool);

    Size GetIdealSize() override;
    Color TextColor(Color bg) const;
    void Paint(VirtPaintCtx&) override;
    void OnMouseEnter();
    void OnMouseLeave();
    void OnKeyDown(VirtKeyEvent*);
    bool Click();

  private:
    bool isDefault = false;
};

VirtButton* AsVirtButton(ILayout*);

struct VirtIconButton : VirtCtrl {
    // not owned; in SumatraPDF it comes from GetCachedPixmapForSvg()
    Pixmap* pixmap = nullptr;
    // drawn when !IsEnabled(); if null, pixmap is used
    Pixmap* pixmapDisabled = nullptr;
    // a toggle button (match case, ...) draws bgColorSelected while on
    bool isSelected = false;
    // split button: a chevron on the right; click there fires onDropdown
    bool hasDropdown = false;
    // which half of a split button is hovered (action vs dropdown)
    bool hoverOnDropdown = false;
    // colors: kColIconBtn*
    VirtMouseHandler onDropdown;

    VirtIconButton();
    ~VirtIconButton() override = default;

    int DropdownDx() const;
    Size GetIdealSize() override;
    void Paint(VirtPaintCtx&) override;
    void OnMouseEnter();
    void OnMouseLeave();
    void OnMouseMove(VirtMouseEvent*);
};

VirtIconButton* AsVirtIconButton(ILayout*);

// The ✕ that closes or removes something, styled like the tab close button: a
// gray ✕ that turns white on a red circle when hovered. `withCircle` also fills
// the circle when not hovered, which is what keeps it readable on top of
// arbitrary content (a thumbnail).
// colors: kColClose*
struct VirtCloseButton : VirtCtrl {
    bool withCircle = false;
    Size idealSize;

    VirtCloseButton();
    ~VirtCloseButton() override = default;

    Size GetIdealSize() override;
    void Paint(VirtPaintCtx&) override;
    void OnMouseEnter();
    void OnMouseLeave();
};

// The header of a side panel: a label on the left, the ✕ that closes the panel
// on the right. `box` is what goes into the parent layout and owns the other
// two, so the caller only keeps what it needs to update
struct LabelWithClose {
    HBox* box = nullptr;
    VirtText* label = nullptr;
    VirtCloseButton* closeBtn = nullptr;
};

VirtCloseButton* AsVirtCloseButton(ILayout*);
LabelWithClose NewLabelWithClose(HWND hwndForDpi, PlatformFont*, const VirtMouseHandler& onClose);
void ApplyLabelWithCloseDpi(VirtText*, VirtCloseButton*, int dpi);

struct VirtImage : VirtCtrl {
    Pixmap* pixmap = nullptr; // not owned
    // scale the image down to fit, keeping the aspect ratio
    bool fitToBounds = true;

    VirtImage();
    ~VirtImage() override;

    Size GetIdealSize() override;
    void Paint(VirtPaintCtx&) override;
};

// colors: kColFillBg
struct VirtFill : VirtCtrl {
    Size idealSize;

    VirtFill();
    ~VirtFill() override;

    Size GetIdealSize() override;
    void Paint(VirtPaintCtx&) override;
};

// colors: kColLineFg
struct VirtLine : VirtCtrl {
    bool isVertical = false;
    int thickness = 1;

    VirtLine();
    ~VirtLine() override;

    Size GetIdealSize() override;
    void Paint(VirtPaintCtx&) override;
};

VirtLine* AsVirtLine(ILayout*);

// Discrete horizontal slider: a bar with a circle thumb. value is minVal..maxVal.
struct VirtSlider : VirtCtrl {
    int minVal = 0;
    int maxVal = 0;
    int value = 0;
    int idealDx = 0;
    Func0 onValueChanged;
    Func0 onValueCommitted;

    VirtSlider();
    ~VirtSlider() override;

    void SetValue(int v, bool notify);
    bool IsAdjusting() const;
    int ValueFromLocalX(int xLocal);
    Size GetIdealSize() override;
    void Paint(VirtPaintCtx&) override;
    void OnMouseDown(VirtMouseEvent*);
    void OnMouseMove(VirtMouseEvent*);
    void OnMouseUp(VirtMouseEvent*);
    void OnMouseWheel(VirtMouseEvent*);
    void OnMouseEnter();
    void OnMouseLeave();
    void OnCaptureLost();

  private:
    bool adjusting = false;
    int committed = 0;
    int ThumbRadius() const;
    Rect TrackRectLocal() const;
    void ApplyFromEvent(const VirtMouseEvent&, bool commit);
};

VirtSlider* AsVirtSlider(ILayout*);

struct VirtSpacer : VirtCtrl {
    Size idealSize;

    VirtSpacer(int dx, int dy);
    ~VirtSpacer() override;

    Size GetIdealSize() override;
};

Rect FitSizeInRect(Size src, Rect dst);

//--- VirtRichText: a small markup with links, keyboard shortcuts and bold runs.
// Shared by SumatraPDF's home page and notifications, and by other apps in the
// family. What it needs from the app is a CommandsContext and a way to open a
// url

struct TipLink;

// What the markup needs to know about the app's commands. The app implements it
// and installs it as gCommandsContext, which is what keeps this code free of
// SumatraPDF's command table.
struct CommandsContext {
    virtual ~CommandsContext() = default;

    // Keyboard shortcut for a command name like "CmdOpenFile", used by
    // (Key/CmdOpenFile). Returns {} when there is no such command (the markup is
    // then left as literal text), and the command name itself when the command
    // exists but has no binding.
    virtual TempStr GetCommandShortcutTemp(Str cmdName) = 0;

    // Runs a link target that names a command. `cmd` may carry arguments, e.g.
    // "CmdFixDefaultApp .pdf".
    virtual void ExecuteCommand(HWND hwnd, Str cmd) = 0;
};

extern CommandsContext* gCommandsContext;

// how the app opens a url link; without it, url links do nothing
extern void (*gTipOpenUrl)(Str url);

// a word in the text; can be part of a link. Node of the intrusive list rooted
// at VirtRichText::words
struct TipWord {
    TipWord* next = nullptr;
    Str text; // owned
    int dx = 0;
    int dy = 0;
    // relative to the control's content origin, set by LayoutText()
    int x = 0;
    int y = 0;
    bool isLink = false;
    bool isBold = false;
    // (Kbd/...) — drawn as a key-cap like the keyboard-shortcuts help sheet
    bool isKbd = false;
    // no inter-word space before this word: it abutted the previous token in the
    // source with no whitespace, e.g. the ':' in "**foo**:" (issue: bold ran into
    // following punctuation with a stray space)
    bool noSpaceBefore = false;
    TipLink* link = nullptr; // the link this word belongs to, if any
};

// node of the intrusive list rooted at VirtRichText::links
struct TipLink {
    TipLink* next = nullptr;
    Str cmd; // owned, resolved target (url or "Cmd...")
    TipWord* firstWord = nullptr;
    TipWord* lastWord = nullptr; // inclusive
};

// A run of text with links, keyboard shortcuts and bold runs, as a virtual
// control: it wraps itself to the width it is given, paints itself, and handles
// clicks on its links. `words` and `links` are root nodes of intrusive lists;
// the content starts at words.next / links.next.
struct VirtRichText : VirtCtrl {
    TipWord words;
    TipLink links;
    // where to append next, so parsing doesn't walk the list for every word
    TipWord* lastWord = nullptr;
    TipLink* lastLink = nullptr;

    // size of the laid-out text, computed by LayoutText()
    int totalDx = 0;
    int totalDy = 0;
    // the width the words were last laid out for
    int layoutDx = -1;

    PlatformFont* font = nullptr; // not owned
    // colors: kColRich*
    // link commands are sent to this window
    HWND hwndForCmds = nullptr;
    // onClick (from VirtCtrl): fired by a click that didn't land on a link, so
    // the whole run can be clickable (command palette "# File History", etc.)

    VirtRichText();
    ~VirtRichText() override;

    void Reset();
    void AddPlainText(Str);
    void AddPlainLink(Str text, Str cmd);
    void LayoutText(int areaWidth);
    bool HasRichContent();
    TempStr PlainTextTemp();
    TipLink* LinkAt(Point ptLocal);

    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    Size Layout(Constraints bc) override;
    Size GetIdealSize() override;
    void SetBounds(Rect) override;
    void Paint(VirtPaintCtx&) override;
    void OnMouseDown(VirtMouseEvent*);
    void OnMouseUp(VirtMouseEvent*);
    void OnSetCursor(VirtSetCursorEvent*);
    void OnGetTooltip(VirtTooltipEvent*); // link under cursor
};

VirtRichText* ParseTip(Str s);
// same, but into a VirtRichText the caller made (e.g. a subclass of it)
void ParseTipInto(VirtRichText*, Str s);
int TipWordCount(VirtRichText*);
int TipLinkCount(VirtRichText*);
void ExecuteTipLink(HWND hwnd, Str cmd);
