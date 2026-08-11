/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// A VirtWnd is a "virtual control": a tree of them lives inside a single HWND,
// each taking up a part of it. VirtRoot bridges the tree to the HWND: it
// drives layout, painting and message dispatch (hover / capture / focus).
//
// A VirtWnd is an ILayout (Layout.h), so it composes with VBox / HBox /
// Padding / VirtTable. Its `bounds` are relative to the parent's content
// origin; painting and hit-testing carry an origin down the tree, which is what
// makes VirtScroll cheap (shift the origin, don't re-layout the subtree).

// needs wingui/PlatformFont.h (PlatformFont) and wingui/Gfx.h (Gfx)
// included before it

struct VirtWnd;
struct VirtRoot;
struct Pixmap;
struct WindowBase;
struct ControlBase;

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

// all rects are in HWND client coords
struct VirtPaintCtx {
    Gfx* gfx = nullptr;
    Rect bounds;  // the wnd being painted
    Rect content; // bounds deflated by padding
    Rect clip;    // intersection of the clip rects of all ancestors
};

struct VirtMouseEvent {
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

struct VirtKeyEvent {
    VirtWnd* target = nullptr;
    int vkey = 0;
    bool isCtrl = false;
    bool isShift = false;
    bool isAlt = false;
};

using VirtMouseHandler = Func1<VirtMouseEvent*>;
using VirtPaintHandler = Func1<VirtPaintCtx*>;

struct VirtWnd : LayoutBase {
    VirtWnd* parent = nullptr;
    VirtRoot* root = nullptr;
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
    int LayoutChildCount() override;
    ILayout* LayoutChildAt(int) override;
    VirtWnd* AsVirtWnd() override;

    virtual Size GetIdealSize();

    virtual void Paint(VirtPaintCtx&);
    virtual void PaintChildren(VirtPaintCtx&);
    void PaintTree(Gfx*, Point origin, Rect clip);
    void PaintStandalone(Gfx*);

    virtual bool HitTest(Point ptLocal);
    // scroll containers return how much their content is scrolled
    virtual Point ScrollOffset();
    VirtWnd* WndFromPoint(Point ptWindow, Point* ptLocalOut);

    // return true to consume the event, false to let it bubble to the parent
    virtual bool OnMouseDown(VirtMouseEvent&);
    virtual bool OnMouseUp(VirtMouseEvent&);
    virtual bool OnMouseMove(VirtMouseEvent&);
    virtual bool OnMouseWheel(VirtMouseEvent&);
    virtual bool OnDoubleClick(VirtMouseEvent&);
    virtual bool OnContextMenu(VirtMouseEvent&);
    virtual void OnMouseEnter();
    virtual void OnMouseLeave();
    virtual void OnCaptureLost();
    virtual bool OnKeyDown(VirtKeyEvent&);
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
};

bool IsVirtWndOfKind(VirtWnd*, Kind);

// The virtual controls at the top of a layout tree: nodes whose ancestors are
// all plain layouts. Doesn't descend into a VirtWnd's own children - it paints
// and hit-tests those itself. A tree of only HWND controls yields none
void CollectVirtWnds(ILayout* root, Vec<VirtWnd*>& out);

// One stop in a window's Tab ring: exactly one of the two is set. The ring is
// the layout order, so an Edit and a virtual button sitting in the same VBox
// are reached one after the other
struct TabStop {
    ControlBase* ctrl = nullptr;
    VirtWnd* vwnd = nullptr;
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
void PaintVirtTree(VirtRoot*, HDC, Rect clip, COLORREF bg);
// paint + input in one call, for a plain HWND that isn't a WindowBase /
// ControlBase (the side panels are subclassed WC_STATICs). Returns true when
// the message was handled
bool VirtHostOnMessage(HWND, VirtRoot*, UINT, WPARAM, LPARAM, LRESULT&, COLORREF bg);
// the single entry point for input: mouse, cursor, tooltips and keyboard, with
// RTL mouse coordinates unmirrored
bool VirtTreeOnMessage(HWND, VirtRoot*, UINT, WPARAM, LPARAM, LRESULT&);

// The virtual controls of one HWND: the window paints them and hands them its
// input. They are the top-level virtual nodes of the window's layout tree, so
// there can be any number of them - a VBox of an Edit and two VirtTexts leaves
// two - and they are not owned here; the layout tree owns them
struct VirtRoot {
    HWND hwnd = nullptr;
    Vec<VirtWnd*> tops;
    // set only by SetChild(), which owns what it is given
    VirtWnd* owned = nullptr;
    // part of hwnd occupied by the tree
    Rect bounds;

    VirtWnd* hovered = nullptr;
    VirtWnd* captured = nullptr;
    VirtWnd* focused = nullptr;
    VirtWnd* pressed = nullptr;

    bool needsLayout = true;
    // legacy single-tree hosts lay out lazily from Paint(); see SetChild()
    bool layoutInPaint = false;
    bool trackingMouseLeave = false;

    explicit VirtRoot(HWND);
    ~VirtRoot();

    // takes ownership; for a window whose whole content is one virtual tree
    void SetChild(VirtWnd*);
    // the tops found in a layout tree; not owned
    void SetTops(const Vec<VirtWnd*>&);
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

// one cell of a VirtTable. alignH / alignV say where the child sits when the
// cell is bigger than the child; CrossAxisAlign::Stretch makes the child fill
// the cell in that direction
struct VirtTableCell {
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
struct VirtTable : VirtWnd {
    int rows = 0;
    int cols = 0;
    // space between adjacent columns / rows
    int colGap = 0;
    int rowGap = 0;

    VirtTable();
    ~VirtTable() override;

    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    Size Layout(Constraints bc) override;
    void SetBounds(Rect) override;

    void SetSize(int rows, int cols);
    VirtTableCell& SetCell(int row, int col, VirtWnd* child, int rowSpan = 1, int colSpan = 1);
    VirtTableCell* CellAt(int row, int col);
    VirtWnd* GetCell(int row, int col);
    void RemoveAllCells();

    // valid after a Layout() / SetBounds() pass
    int ColWidth(int col);
    int RowHeight(int row);
    Rect CellRect(int row, int col);

  private:
    Vec<VirtTableCell> cells; // rows * cols, row-major
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
struct VirtScrollRange {
    VirtWnd* wnd = nullptr;
    int visibleY = 0;
    int visibleDy = 0;
};

struct VirtScroll : VirtWnd {
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
    bool OnMouseWheel(VirtMouseEvent&) override;

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

// A list of rows: it owns the model, the selection and the scroll position, and
// paints only the rows that are visible. The virtual counterpart of the HWND
// ListBox, minus the win32 listbox's habits (it doesn't steal the keyboard
// focus when clicked, and it repaints as one piece instead of scrolling pixels)
struct VirtListBox : VirtWnd {
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
    COLORREF textColor = kColorUnset;
    COLORREF bgColor = kColorUnset;
    // background of the selected row; derived from bgColor when unset
    COLORREF selectionColor = kColorUnset;
    COLORREF scrollbarColor = kColorUnset;

    // how many rows GetIdealSize() asks for; 0 means "as many as there are",
    // capped at 16
    int idealSizeLines = 0;
    // width GetIdealSize() asks for; 0 means a default
    int idealSizeDx = 0;
    // 0 means "derive from the font"
    int itemDy = 0;
    // for scaling before the tree is attached to a window (GetHwnd() is null
    // until then)
    HWND hwndForDpi = nullptr;

    int scrollY = 0;

    VirtListBox();
    ~VirtListBox() override;

    Size GetIdealSize() override;
    void SetBounds(Rect) override;
    void Paint(VirtPaintCtx&) override;
    bool OnMouseDown(VirtMouseEvent&) override;
    bool OnMouseUp(VirtMouseEvent&) override;
    bool OnMouseMove(VirtMouseEvent&) override;
    bool OnMouseWheel(VirtMouseEvent&) override;
    bool OnDoubleClick(VirtMouseEvent&) override;
    bool OnKeyDown(VirtKeyEvent&) override;
    void OnCaptureLost() override;

    // for efficiency you can re-use the model: get it, change the data, call
    // SetModel() again
    void SetModel(ListBoxModel*);
    int ItemsCount();
    int GetItemHeight();
    int GetCurrentSelection();
    // -1 clears the selection; doesn't call onSelectionChanged
    bool SetCurrentSelection(int);
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
    // EnsureVisible() called before the first layout; applied by SetBounds()
    int pendingVisibleIdx = -1;
    bool draggingThumb = false;
    // where the thumb drag started, in window coords, and the scroll position
    // it started from
    int dragStartY = 0;
    int dragStartScrollY = 0;

    HWND HwndForDpi();
    int ScrollbarDx();
    Rect ContentRectLocal();
    Rect ItemsRectLocal();
    Rect ScrollbarRectLocal();
    Rect ThumbRectLocal();
    bool SelectAndNotify(int idx);
};

struct VirtCustom : VirtWnd {
    Size idealSize;
    VirtPaintHandler onPaint;
    VirtMouseHandler onClick;

    VirtCustom();
    ~VirtCustom() override;

    Size GetIdealSize() override;
    void Paint(VirtPaintCtx&) override;
    bool OnMouseUp(VirtMouseEvent&) override;
};

//--- controls

enum class VirtTextAlign {
    Left,
    Center,
    Right
};

// VirtText itself can't be an aggregate - it inherits VirtWnd, which has
// virtual functions and a constructor - so designated initializers go through
// this, like the CreateArgs of the HWND controls:
//   auto* t = NewVirtText({.s = path, .font = font, .ellipsis = true});
struct VirtTextArgs {
    Str s;
    PlatformFont* font = nullptr; // not owned, interned
    COLORREF textColor = kColorUnset;
    VirtTextAlign align = VirtTextAlign::Left;
    bool withUnderline = false;
    bool isRtl = false;
    bool ellipsis = false;
    // nudges the underline off the text baseline box
    int underlineOffsetY = 0;
    Insets padding{};
};

struct VirtText : VirtWnd {
    Str s;
    PlatformFont* font = nullptr; // not owned, interned
    bool withUnderline = false;
    bool isRtl = false;
    bool ellipsis = false;
    // nudges the underline off the text baseline box
    int underlineOffsetY = 0;
    VirtTextAlign align = VirtTextAlign::Left;
    COLORREF textColor = kColorUnset;

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
};

VirtText* NewVirtText(const VirtTextArgs&);

struct VirtLink : VirtText {
    Str target;  // owned
    Str tooltip; // owned
    VirtMouseHandler onClick;
    bool underlineOnHover = false;

    VirtLink(Str s, PlatformFont* font = nullptr);
    ~VirtLink() override;

    void SetTarget(Str);
    void SetTooltip(Str);

    void Paint(VirtPaintCtx&) override;
    void OnMouseEnter() override;
    void OnMouseLeave() override;
    bool OnMouseDown(VirtMouseEvent&) override;
    bool OnMouseUp(VirtMouseEvent&) override;
    bool OnSetCursor(Point ptLocal) override;
    TempStr GetTooltipTemp(Point ptLocal) override;
};

struct VirtButton : VirtText {
    COLORREF bgColor = kColorUnset;
    COLORREF bgColorHover = kColorUnset;
    COLORREF borderColor = kColorUnset;
    // when the button is disabled (vwfEnabled cleared)
    COLORREF textColorDisabled = kColorUnset;
    Insets textPadding{4, 8, 4, 8};
    VirtMouseHandler onClick;

    VirtButton(Str s, PlatformFont* font = nullptr);
    ~VirtButton() override;

    Size GetIdealSize() override;
    void Paint(VirtPaintCtx&) override;
    void OnMouseEnter() override;
    void OnMouseLeave() override;
    bool OnMouseDown(VirtMouseEvent&) override;
    bool OnMouseUp(VirtMouseEvent&) override;
    bool OnKeyDown(VirtKeyEvent&) override;
    bool OnSetCursor(Point ptLocal) override;
};

struct VirtIconButton : VirtWnd {
    // not owned; on Windows usually from IconPixmapFromImageList()
    Pixmap* pixmap = nullptr;
    bool isSelected = false;
    Str tooltip; // owned
    VirtMouseHandler onClick;

    VirtIconButton();
    ~VirtIconButton() override;

    void SetTooltip(Str);

    Size GetIdealSize() override;
    void Paint(VirtPaintCtx&) override;
    void OnMouseEnter() override;
    void OnMouseLeave() override;
    bool OnMouseDown(VirtMouseEvent&) override;
    bool OnMouseUp(VirtMouseEvent&) override;
    bool OnSetCursor(Point ptLocal) override;
    TempStr GetTooltipTemp(Point ptLocal) override;
};

// The ✕ that closes or removes something, styled like the tab close button: a
// gray ✕ that turns white on a red circle when hovered. `withCircle` also fills
// the circle when not hovered, which is what keeps it readable on top of
// arbitrary content (a thumbnail).
// Colors left at kColorUnset use the tab close button's.
struct VirtCloseButton : VirtWnd {
    bool withCircle = false;
    COLORREF xColor = kColorUnset;
    COLORREF xColorHover = kColorUnset;
    COLORREF circleColor = kColorUnset;
    COLORREF circleColorHover = kColorUnset;
    Size idealSize;
    Str tooltip; // owned
    VirtMouseHandler onClick;

    VirtCloseButton();
    ~VirtCloseButton() override;

    void SetTooltip(Str);

    Size GetIdealSize() override;
    void Paint(VirtPaintCtx&) override;
    void OnMouseEnter() override;
    void OnMouseLeave() override;
    bool OnMouseDown(VirtMouseEvent&) override;
    bool OnMouseUp(VirtMouseEvent&) override;
    bool OnSetCursor(Point ptLocal) override;
    TempStr GetTooltipTemp(Point ptLocal) override;
};

// The header of a side panel: a label on the left, the ✕ that closes the panel
// on the right. `box` is what goes into the parent layout and owns the other
// two, so the caller only keeps what it needs to update
struct LabelWithClose {
    HBox* box = nullptr;
    VirtText* label = nullptr;
    VirtCloseButton* closeBtn = nullptr;
};

LabelWithClose NewLabelWithClose(HWND hwndForDpi, PlatformFont*, const VirtMouseHandler& onClose);

struct VirtImage : VirtWnd {
    Pixmap* pixmap = nullptr; // not owned
    // scale the image down to fit, keeping the aspect ratio
    bool fitToBounds = true;

    VirtImage();
    ~VirtImage() override;

    Size GetIdealSize() override;
    void Paint(VirtPaintCtx&) override;
};

struct VirtFill : VirtWnd {
    COLORREF color = kColorUnset;
    Size idealSize;

    VirtFill();
    ~VirtFill() override;

    Size GetIdealSize() override;
    void Paint(VirtPaintCtx&) override;
};

struct VirtLine : VirtWnd {
    COLORREF color = kColorUnset;
    bool isVertical = false;
    int thickness = 1;

    VirtLine();
    ~VirtLine() override;

    Size GetIdealSize() override;
    void Paint(VirtPaintCtx&) override;
};

struct VirtSpacer : VirtWnd {
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
struct VirtRichText : VirtWnd {
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
    COLORREF textColor = kColorUnset;
    COLORREF linkColor = kColorUnset;
    // the color the text is painted on; used for the key-cap fill and border
    COLORREF bgColor = kColorUnset;
    // link commands are sent to this window
    HWND hwndForCmds = nullptr;
    // fired by a click that didn't land on a link, so the whole run can be
    // clickable (the command palette's "# File History" and friends)
    VirtMouseHandler onClick;

    VirtRichText();
    ~VirtRichText() override;

    void Reset();
    void AddPlainText(Str);
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
    bool OnMouseDown(VirtMouseEvent&) override;
    bool OnMouseUp(VirtMouseEvent&) override;
    bool OnSetCursor(Point ptLocal) override;
    TempStr GetTooltipTemp(Point ptLocal) override;
};

VirtRichText* ParseTip(Str s);
// same, but into a VirtRichText the caller made (e.g. a subclass of it)
void ParseTipInto(VirtRichText*, Str s);
int TipWordCount(VirtRichText*);
int TipLinkCount(VirtRichText*);
void ExecuteTipLink(HWND hwnd, Str cmd);
