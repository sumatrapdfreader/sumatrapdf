/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// port of https://gitlab.com/stone.code/goey

const int Inf = std::numeric_limits<int>::max();

// can't call it Rectangle because conflicts with GDI+ Rectangle function
struct Rect {
    int x = 0;
    int y = 0;
    int dx = 0;
    int dy = 0;

    Rect() = default;
    Rect(PointI min, PointI max);

    int Width() const;
    int Height() const;
    int Dx() const;
    int Dy() const;
    int Right() const;
    int Bottom() const;
    bool empty() const;
};

RECT RectToRECT(const Rect);

int clamp(int v, int vmin, int vmax);
int scale(int v, i64 num, i64 den);
int guardInf(int a, int b);

struct Constraints {
    SizeI min{};
    SizeI max{};

    SizeI Constrain(const SizeI) const;
    SizeI ConstrainAndAttemptToPreserveAspectRatio(const SizeI) const;
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
    bool IsSatisfiedBy(SizeI) const;
    bool IsZero() const;
    Constraints Loosen() const;
    Constraints LoosenHeight() const;
    Constraints LoosenWidth() const;
    Constraints Tighten(SizeI) const;
    Constraints TightenHeight(int height) const;
    Constraints TightenWidth(int width) const;
};

Constraints ExpandInf();
Constraints ExpandHeight(int width);
Constraints ExpandWidth(int height);
Constraints Loose(const SizeI size);
Constraints Tight(const SizeI size);
Constraints TightHeight(int height);

typedef std::function<void()> NeedLayout;

struct LayoutManager {
    bool needLayout = false;

    LayoutManager() = default;
    virtual ~LayoutManager() = 0;

    virtual void NeedLayout();
};

struct ILayout {
    Kind kind = nullptr;
    LayoutManager* layoutManager = nullptr;
    // allows easy way to hide / show elements
    // without rebuilding the whole layout
    bool isVisible = true;
    // for easy debugging, remember last bounds
    Rect lastBounds{};

    ILayout() = default;
    ILayout(Kind k);
    virtual ~ILayout(){};
    virtual SizeI Layout(const Constraints bc) = 0;
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

inline Insets DefaultInsets() {
    const int padding = 8;
    return Insets{padding, padding, padding, padding};
}

inline Insets UniformInsets(int l) {
    return Insets{l, l, l, l};
}

struct Padding : public ILayout {
    Insets insets{};
    ILayout* child = nullptr;
    SizeI childSize{};

    ~Padding() override;
    SizeI Layout(const Constraints bc) override;
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
    SizeI Layout(const Constraints bc) override;
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
    SizeI size = {};
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

    ~VBox() override;
    SizeI Layout(const Constraints bc) override;
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    void SetBounds(Rect bounds) override;

    void setBoundsForChild(size_t i, ILayout* v, int posX, int posY, int posX2, int posY2);

    boxElementInfo& addChild(ILayout* child);
    boxElementInfo& addChild(ILayout* child, int flex);
    size_t childrenCount(); // only visible children
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
    SizeI Layout(const Constraints bc) override;
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    void SetBounds(Rect bounds) override;

    void setBoundsForChild(size_t i, ILayout* v, int posX, int posY, int posX2, int posY2);
    boxElementInfo& addChild(ILayout* child);
    boxElementInfo& addChild(ILayout* child, int flex);
    size_t childrenCount();
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
    SizeI childSize{};

    Align(ILayout*);
    ~Align() override;
    SizeI Layout(const Constraints bc) override;
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
