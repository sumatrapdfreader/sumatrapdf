/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// port of https://gitlab.com/stone.code/goey

const int Inf = INT_MAX;

void PositionRB(const Rect& container, Rect& r);
void MoveXY(Rect& r, int x, int y);

int Clamp(int v, int vmin, int vmax);
int Scale(int v, i64 num, i64 den);
int GuardInf(int a, int b);

struct Constraints {
    Size min;
    Size max;

    Size Constrain(Size) const;
    Size ConstrainAndAttemptToPreserveAspectRatio(Size) const;
    int ConstrainHeight(int height) const;
    int ConstrainWidth(int width) const;
    bool HasBoundedHeight() const;
    bool HasBoundedWidth() const;
    bool HasTightWidth() const;
    bool HasTightHeight() const;
    Constraints Inset(int width, int height) const;
    bool IsBounded() const;
    bool IsNormalized() const;
    bool IsTight() const;
    bool IsSatisfiedBy(Size) const;
    bool IsZero() const;
    Constraints Loosen() const;
    Constraints LoosenHeight() const;
    Constraints LoosenWidth() const;
    Constraints Tighten(Size) const;
    Constraints TightenHeight(int height) const;
    Constraints TightenWidth(int width) const;
};

Constraints ExpandInf();
Constraints ExpandHeight(int width);
Constraints ExpandWidth(int height);
Constraints Loose(Size size);
Constraints Tight(Size size);
Constraints TightHeight(int height);

using NeedLayout = Func0;

// works like css visibility property
enum class Visibility {
    Visible,
    // not visible but takes up space for purpose of layout
    Hidden,
    // not visible and doesn't take up space
    Collapse,
};

struct VirtCtrl;
struct ControlBase;

struct ILayout {
    Kind kind = nullptr;
    // allows easy way to hide / show elements
    // without rebuilding the whole layout
    Visibility visibility = Visibility::Visible;
    // for easy debugging, remember last bounds
    Rect lastBounds;

    ILayout() = default;
    virtual ~ILayout() = default;

    Kind GetKind() const { return kind; }
    virtual void SetVisibility(Visibility);
    virtual Visibility GetVisibility();
    virtual int MinIntrinsicHeight(int width) = 0;
    virtual int MinIntrinsicWidth(int height) = 0;
    virtual Size Layout(Constraints bc) = 0;
    virtual void SetBounds(Rect);

    // walking a layout tree: containers return their children, leaves nothing.
    // Lets code find things in a tree it didn't build (see CollectVirtCtrls())
    virtual int LayoutChildCount() { return 0; }
    virtual ILayout* LayoutChildAt(int) { return nullptr; }
    // non-null for the virtual controls, which have no HWND of their own: the
    // window they end up in paints them and sends them their input
    virtual VirtCtrl* AsVirtCtrl() { return nullptr; }
    // non-null for the controls that do have an HWND of their own
    virtual ControlBase* AsControl() { return nullptr; }
};

bool IsCollapsed(ILayout*);

bool IsLayoutOfKind(ILayout*, Kind);

// padding.go

struct Insets {
    int top = 0;
    int right = 0;
    int bottom = 0;
    int left = 0;
};

Insets DefaultInsets();
Insets DpiScaledInsets(int uniform);
Insets DpiScaledInsets(int topBottom, int leftRight);
Insets DpiScaledInsets(int top, int right, int bottom, int left);

struct Padding : ILayout {
    ILayout* child = nullptr;
    Insets insets{};
    Size childSize;

    Padding(ILayout*, const Insets&);
    ~Padding() override;

    Size Layout(Constraints bc) override;
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    void SetBounds(Rect) override;

    int LayoutChildCount() override;
    ILayout* LayoutChildAt(int) override;
};

bool IsPadding(Kind);
bool IsPadding(ILayout*);

// vbox.go

// TODO: rename MainStart => Start, MainEnd => End, MainCenter => Center
// Homogeneous => Evenly
enum class MainAxisAlign : u8 {
    // Children will be packed together at the top or left of the box
    MainStart,
    // Children will be packed together and centered in the box.
    MainCenter,
    // Children will be packed together at the bottom or right of the box
    MainEnd,
    // Children will be spaced apart
    SpaceAround,
    // Children will be spaced apart, but the first and last children will but the ends of the box.
    SpaceBetween,
    // Children will be allocated equal space.
    Homogeneous,
};

inline bool IsPacked(MainAxisAlign a) {
    return a <= MainAxisAlign::MainEnd;
}

enum class CrossAxisAlign : u8 {
    Stretch,     // Children will be stretched so that the extend across box
    CrossStart,  // Children will be aligned to the left or top of the box
    CrossCenter, // Children will be aligned in the center of the box
    CrossEnd,    // Children will be aligned to the right or bottom of the box
};

struct boxElementInfo {
    ILayout* layout = nullptr;
    Size size;
    int flex = 0;
};

struct VBox : ILayout {
    Vec<boxElementInfo> children;
    MainAxisAlign alignMain = MainAxisAlign::MainStart;
    CrossAxisAlign alignCross = CrossAxisAlign::CrossStart;
    int gap = 0;
    int totalHeight = 0;
    int totalFlex = 0;

    VBox();
    ~VBox() override;

    Size Layout(Constraints bc) override;
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    void SetBounds(Rect bounds) override;

    void SetBoundsForChild(int i, ILayout* v, int posX, int posY, int posX2, int posY2) const;

    int LayoutChildCount() override;
    ILayout* LayoutChildAt(int) override;

    boxElementInfo& AddChild(ILayout* child);
    boxElementInfo& AddChild(ILayout* child, int flex);
    int ChildrenCount() const;
    int NonCollapsedChildrenCount();
};

// hbox.go

struct HBox : ILayout {
    Vec<boxElementInfo> children;
    MainAxisAlign alignMain = MainAxisAlign::MainStart;
    CrossAxisAlign alignCross = CrossAxisAlign::CrossStart;
    int gap = 0;
    // when true, children are placed right-to-left (MainStart packs to the right)
    bool rtl = false;
    int totalWidth = 0;
    int totalFlex = 0;

    HBox();
    ~HBox() override;

    // ILayout
    Size Layout(Constraints bc) override;
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    void SetBounds(Rect bounds) override;

    void SetBoundsForChild(int i, ILayout* v, int posX, int posY, int posX2, int posY2) const;

    int LayoutChildCount() override;
    ILayout* LayoutChildAt(int) override;

    boxElementInfo& AddChild(ILayout* child);
    boxElementInfo& AddChild(ILayout* child, int flex);
    int ChildrenCount() const;
    int NonCollapsedChildrenCount();
};

// align.go

// defined as i64 but values are i32
typedef i64 Alignment;

constexpr Alignment AlignStart = -32768;
constexpr Alignment AlignCenter = 0;
constexpr Alignment AlignEnd = 0x7fff;

struct Align : ILayout {
    Alignment HAlign = AlignStart; // Horizontal alignment of child widget.
    Alignment VAlign = AlignStart; // Vertical alignment of child widget.
    float WidthFactor = 0;         // If greater than zero, ratio of container width to child width.
    float HeightFactor = 0;        // If greater than zero, ratio of container height to child height.
    ILayout* Child = nullptr;
    Size childSize;

    explicit Align(ILayout*);
    ~Align() override;

    Size Layout(Constraints bc) override;
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    void SetBounds(Rect) override;

    int LayoutChildCount() override;
    ILayout* LayoutChildAt(int) override;
};

// spacer can be used for spacing between elements
struct Spacer : ILayout {
    int dx = 0;
    int dy = 0;

    Spacer(int, int);
    ~Spacer() override;

    // ILayout
    Size Layout(Constraints bc) override;
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    void SetBounds(Rect) override;
};

// one child of an Overlay: sits or stretches inside the shared box
struct OverlayChild {
    ILayout* child = nullptr;
    Size size;
    CrossAxisAlign alignH = CrossAxisAlign::Stretch;
    CrossAxisAlign alignV = CrossAxisAlign::Stretch;
};

// children share one box (a z-stack). Sized to the largest child.
struct Overlay : ILayout {
    Vec<OverlayChild> children;

    Overlay();
    ~Overlay() override;

    Size Layout(Constraints bc) override;
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    void SetBounds(Rect) override;

    int LayoutChildCount() override;
    ILayout* LayoutChildAt(int) override;

    OverlayChild& AddChild(ILayout* child);
    OverlayChild& AddChild(ILayout* child, CrossAxisAlign alignH, CrossAxisAlign alignV);
    int ChildrenCount() const;
};

// HBox that wraps onto the next row when children do not fit the available width.
// Flex is applied per row. rtl places each row right-to-left.
struct Wrap : ILayout {
    Vec<boxElementInfo> children;
    CrossAxisAlign alignCross = CrossAxisAlign::CrossStart;
    bool rtl = false;
    int colGap = 0;
    int rowGap = 0;

    Wrap();
    ~Wrap() override;

    Size Layout(Constraints bc) override;
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    void SetBounds(Rect) override;

    int LayoutChildCount() override;
    ILayout* LayoutChildAt(int) override;

    boxElementInfo& AddChild(ILayout* child);
    boxElementInfo& AddChild(ILayout* child, int flex);
    int ChildrenCount() const;

  private:
    struct Row {
        int start = 0;
        int count = 0;
        int width = 0;
        int height = 0;
    };
    Vec<Row> rows;

    void PackRows(int maxWidth);
};

//--- Table: a grid of cells, each holding an ILayout child

// one cell of a Table. alignH / alignV say where the child sits when the
// cell is bigger than the child; CrossAxisAlign::Stretch makes the child fill
// the cell in that direction
struct TableCell {
    // owned by the Table (deleted with the table / when replaced)
    ILayout* child = nullptr;
    int rowSpan = 1;
    int colSpan = 1;
    CrossAxisAlign alignH = CrossAxisAlign::CrossStart;
    CrossAxisAlign alignV = CrossAxisAlign::CrossStart;
    // covered by a cell that spans into it, so it can't hold a child of its own
    bool covered = false;
    // the child's size, measured by Layout()
    Size childSize;
};

// a grid of rows x cols cells. A column is as wide as its widest cell and a row
// as tall as its tallest; a cell can span several rows and / or columns
struct Table : ILayout {
    int rows = 0;
    int cols = 0;
    // space between adjacent columns / rows
    int colGap = 0;
    int rowGap = 0;
    Insets padding{};

    Table();
    ~Table() override;

    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    Size Layout(Constraints bc) override;
    void SetBounds(Rect) override;

    int LayoutChildCount() override;
    ILayout* LayoutChildAt(int) override;

    void SetSize(int rows, int cols);
    TableCell& SetCell(int row, int col, ILayout* child, int rowSpan = 1, int colSpan = 1);
    TableCell* CellAt(int row, int col);
    ILayout* GetCell(int row, int col);
    void RemoveAllCells();

    // valid after a Layout() / SetBounds() pass
    int ColWidth(int col);
    int RowHeight(int row);
    Rect CellRect(int row, int col);

  private:
    Vec<TableCell> cells; // rows * cols, row-major
    Vec<int> colWidths;
    Vec<int> rowHeights;

    int CellIdx(int row, int col) const;
    void MarkCovered(int row, int col, int rowSpan, int colSpan, bool covered);
    void Measure();
    Size TotalSize();
    Rect ContentRect();
};

Size LayoutToSize(ILayout* layout, Size size);

void dbglayout(Str s);
void LogConstraints(Constraints c, Str suffix);

#if defined(DEBUG)
void Layout_UnitTests();
#endif

#if OS_WIN
#include "gui/Layout_win.h"
#endif
