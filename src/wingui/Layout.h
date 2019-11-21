typedef int64_t i64;
typedef int32_t i32;

typedef int32_t Length;

const Length Inf = std::numeric_limits<Length>::max();

extern Length DIP;

struct Size {
    Length Width = 0;
    Length Height = 0;
};

struct Point {
    Length X = 0;
    Length Y = 0;
};

// can't call it Rectangle because conflicts with GDI+ Rectangle function
struct Rect {
    Point Min;
    Point Max;

    Length Width() const;
    Length Height() const;
    Length Dx() const;
    Length Dy() const;
};

RECT RectToRECT(const Rect);

Length clamp(Length v, Length vmin, Length vmax);
Length scale(Length v, i64 num, i64 den);
Length guardInf(Length a, Length b);

struct Constraints {
    Size Min;
    Size Max;

    Size Constrain(const Size) const;
    Size ConstrainAndAttemptToPreserveAspectRatio(const Size) const;
    Length ConstrainHeight(Length height) const;
    Length ConstrainWidth(Length width) const;
    bool HasBoundedHeight() const;
    bool HasBoundedWidth() const;
    bool HasTightWidth() const;
    bool HasTightHeight() const;
    Constraints Inset(Length width, Length height) const;
    bool IsBounded() const;
    bool IsNormalized() const;
    bool IsTight() const;
    bool IsSatisfiedBy(Size) const;
    bool IsZero() const;
    Constraints Loosen() const;
    Constraints LoosenHeight() const;
    Constraints LoosenWidth() const;
    Constraints Tighten(Size) const;
    Constraints TightenHeight(Length height) const;
    Constraints TightenWidth(Length width) const;
};

Constraints Expand();
Constraints ExpandHeight(Length width);
Constraints ExpandWidth(Length height);
Constraints Loose(const Size size);
Constraints Tight(const Size size);
Constraints TightHeight(Length height);

// identity of an object is an address of a unique, global string
// string is good for debugging
// yes, C++ is really that lame and we have to implement
// dynamic typing manually
typedef const char* Kind;

struct ILayout {
    Kind kind = nullptr;

    virtual ~ILayout(){};
    virtual Size Layout(const Constraints bc) = 0;
    virtual Length MinIntrinsicHeight(Length width) = 0;
    virtual Length MinIntrinsicWidth(Length height) = 0;
    virtual void SetBounds(Rect) = 0;
};

bool IsLayoutOfKind(ILayout*, Kind);

Length calculateVGap(ILayout* previous, ILayout* current);
Length calculateHGap(ILayout* previous, ILayout* current);

// padding.go

struct Insets {
    Length Top = 0;
    Length Right = 0;
    Length Bottom = 0;
    Length Left = 0;
};

inline Insets DefaultInsets() {
    const Length padding = 11;
    return Insets{padding, padding, padding, padding};
}

inline Insets UniformInsets(Length l) {
    return Insets{l, l, l, l};
}

struct Padding : public ILayout {
    Insets insets;
    ILayout* child;
    Size childSize;

    ~Padding() override;
    Size Layout(const Constraints bc) override;
    Length MinIntrinsicHeight(Length width) override;
    Length MinIntrinsicWidth(Length height) override;
    void SetBounds(Rect) override;
};

bool IsPadding(Kind);
bool IsPadding(ILayout*);

// expand.go

struct Expand : public ILayout {
    ILayout* child;
    int factor;

    // ILayout
    ~Expand() override;
    Size Layout(const Constraints bc) override;
    Length MinIntrinsicHeight(Length width) override;
    Length MinIntrinsicWidth(Length height) override;
    void SetBounds(Rect) override;
};

bool IsExpand(Kind);
bool IsExpand(ILayout*);

// vbox.go

// TODO: rename MainStart => Start, MainEnd => End, MainCenter => Center
// Homogeneous => Evenly
enum class MainAxisAlign : uint8_t {
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

enum class CrossAxisAlign : uint8_t {
    Stretch,     // Children will be stretched so that the extend across box
    CrossStart,  // Children will be aligned to the left or top of the box
    CrossCenter, // Children will be aligned in the center of the box
    CrossEnd,    // Children will be aligned to the right or bottom of the box
};

struct boxElementInfo {
    Size size;
    int flex;
};

bool IsVBox(Kind);
bool IsVBox(ILayout*);

struct VBox : public ILayout {
    Vec<ILayout*> children;
    MainAxisAlign alignMain;
    CrossAxisAlign alignCross;
    Vec<boxElementInfo> childrenInfo;
    Length totalHeight;
    int totalFlex;

    ~VBox() override;
    Size Layout(const Constraints bc) override;
    Length MinIntrinsicHeight(Length width) override;
    Length MinIntrinsicWidth(Length height) override;
    void SetBounds(Rect bounds) override;

    void setBoundsForChild(size_t i, ILayout* v, Length posX, Length posY, Length posX2, Length posY2);

    // TODO: alternatively, ensure childrenInfo has the same
    // size as children in Layout()
    void addChild(ILayout* child);
};

// hbox.go

bool IsHBox(Kind);
bool IsHBox(ILayout*);

struct HBox : public ILayout {
    MainAxisAlign alignMain;
    CrossAxisAlign alignCross;
    Vec<ILayout*> children;
    Vec<boxElementInfo> childrenInfo;
    Length totalWidth;
    int totalFlex;

    ~HBox() override;
    Size Layout(const Constraints bc) override;
    Length MinIntrinsicHeight(Length width) override;
    Length MinIntrinsicWidth(Length height) override;
    void SetBounds(Rect bounds) override;

    void setBoundsForChild(size_t i, ILayout* v, Length posX, Length posY, Length posX2, Length posY2);
    void addChild(ILayout* child);
};

// align.go

// defined as i64 but values are i32
typedef i64 Alignment;

constexpr Alignment AlignStart = -32768;
constexpr Alignment AlignCenter = 0;
constexpr Alignment AlignEnd = 0x7fff;

struct Align : public ILayout {
    Alignment HAlign;   // Horizontal alignment of child widget.
    Alignment VAlign;   // Vertical alignment of child widget.
    float WidthFactor;  // If greater than zero, ratio of container width to child width.
    float HeightFactor; // If greater than zero, ratio of container height to child height.
    ILayout* Child;

    Size childSize;

    Align();
    ~Align() override;
    Size Layout(const Constraints bc) override;
    Length MinIntrinsicHeight(Length width) override;
    Length MinIntrinsicWidth(Length height) override;
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
