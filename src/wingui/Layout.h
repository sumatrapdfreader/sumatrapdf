/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// port of https://gitlab.com/stone.code/goey

const int Inf = std::numeric_limits<int>::max();

RECT RectToRECT(const Rect);

int clamp(int v, int vmin, int vmax);
int scale(int v, i64 num, i64 den);
int guardInf(int a, int b);

struct Constraints {
    Size min{};
    Size max{};

    Size Constrain(const Size) const;
    Size ConstrainAndAttemptToPreserveAspectRatio(const Size) const;
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
Constraints Loose(const Size size);
Constraints Tight(const Size size);
Constraints TightHeight(int height);

typedef std::function<void()> NeedLayout;

// works like css visibility property
enum class Visibility {
    Visible,
    // not visible but takes up space for purpose of layout
    Hidden,
    // not visible and doesn't take up space
    Collapse,
};

struct ILayout {
    Kind kind = nullptr;
    // allows easy way to hide / show elements
    // without rebuilding the whole layout
    bool isVisible = true;
    // for easy debugging, remember last bounds
    Rect lastBounds{};

    ILayout() = default;
    ILayout(Kind k);
    virtual ~ILayout(){};
    virtual Size Layout(const Constraints bc) = 0;
    virtual int MinIntrinsicHeight(int width) = 0;
    virtual int MinIntrinsicWidth(int height) = 0;
    virtual void SetBounds(Rect) = 0;

    void SetIsVisible(bool);
};

bool IsLayoutOfKind(ILayout*, Kind);

int calculateVGap(ILayout* previous, ILayout* current);
int calculateHGap(ILayout* previous, ILayout* current);

// padding.go

struct Insets {
    int top = 0;
    int right = 0;
    int bottom = 0;
    int left = 0;
};

Insets DefaultInsets();
Insets UniformInsets(int l);
Insets DpiScaledInsets(HWND, int top, int right = -1, int bottom = -1, int left = -1);

struct Padding : public ILayout {
    ILayout* child = nullptr;
    Insets insets{};
    Size childSize{};

    Padding(ILayout*, const Insets&);
    ~Padding() override;
    Size Layout(const Constraints bc) override;
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    void SetBounds(Rect) override;
};

bool IsPadding(Kind);
bool IsPadding(ILayout*);

// expand.go

struct Expand : public ILayout {
    ILayout* child = nullptr;
    int factor = 0;

    // ILayout
    Expand(ILayout* c, int f);
    ~Expand() override;
    Size Layout(const Constraints bc) override;
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    void SetBounds(Rect) override;
};

Expand* CreateExpand(ILayout*, int);

bool IsExpand(Kind);
bool IsExpand(ILayout*);

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
    Size size = {};
    int flex = 0;
};

bool IsVBox(Kind);
bool IsVBox(ILayout*);

struct VBox : public ILayout {
    Vec<boxElementInfo> children;
    MainAxisAlign alignMain = MainAxisAlign::MainStart;
    CrossAxisAlign alignCross = CrossAxisAlign::CrossStart;
    int totalHeight = 0;
    int totalFlex = 0;

    VBox();
    ~VBox() override;
    Size Layout(const Constraints bc) override;
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    void SetBounds(Rect bounds) override;

    void SetBoundsForChild(int i, ILayout* v, int posX, int posY, int posX2, int posY2);

    boxElementInfo& AddChild(ILayout* child);
    boxElementInfo& AddChild(ILayout* child, int flex);
    int ChildrenCount();
    int VisibleChildrenCount();
};

// hbox.go

bool IsHBox(Kind);
bool IsHBox(ILayout*);

struct HBox : public ILayout {
    Vec<boxElementInfo> children;
    MainAxisAlign alignMain = MainAxisAlign::MainStart;
    CrossAxisAlign alignCross = CrossAxisAlign::CrossStart;
    int totalWidth = 0;
    int totalFlex = 0;

    ~HBox() override;
    Size Layout(const Constraints bc) override;
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    void SetBounds(Rect bounds) override;

    void SetBoundsForChild(int i, ILayout* v, int posX, int posY, int posX2, int posY2);
    boxElementInfo& AddChild(ILayout* child);
    boxElementInfo& AddChild(ILayout* child, int flex);
    int ChildrenCount();
};

// align.go

// defined as i64 but values are i32
typedef i64 Alignment;

constexpr Alignment AlignStart = -32768;
constexpr Alignment AlignCenter = 0;
constexpr Alignment AlignEnd = 0x7fff;

struct Align : public ILayout {
    Alignment HAlign = AlignStart; // Horizontal alignment of child widget.
    Alignment VAlign = AlignStart; // Vertical alignment of child widget.
    float WidthFactor = 0;         // If greater than zero, ratio of container width to child width.
    float HeightFactor = 0;        // If greater than zero, ratio of container height to child height.
    ILayout* Child = 0;
    Size childSize{};

    Align(ILayout*);
    ~Align() override;
    Size Layout(const Constraints bc) override;
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    void SetBounds(Rect) override;
};

// spacer is to be used to take space
// can be used for flexible
struct Spacer : public ILayout {
    int dx = 0;
    int dy = 0;

    Spacer(int, int);
    ~Spacer() override;
    Size Layout(const Constraints bc) override;
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    void SetBounds(Rect) override;
};

bool IsAlign(Kind);
bool IsAlign(ILayout*);

// declaring here because used in Layout.cpp
// lives in ButtonCtrl.cpp
bool IsButton(Kind);
bool IsButton(ILayout*);

// declaring here because used in Layout.cpp
// lives in ButtonCtrl.cpp
bool IsCheckbox(Kind);
bool IsCheckbox(ILayout*);

bool IsExpand(Kind);
bool IsExpand(ILayout*);

bool IsLabeL(Kind);
bool IsLabel(ILayout*);

extern Kind kindLabel;

void LayoutAndSizeToContent(ILayout* layout, int minDx, int minDy, HWND hwnd);
Rect LayoutToSize(ILayout* layout, const Size size);
Rect Relayout(ILayout* layout);

void dbglayoutf(const char* fmt, ...);
void LogConstraints(Constraints c, const char* suffix);
