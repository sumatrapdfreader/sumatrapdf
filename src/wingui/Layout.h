typedef int64_t i64;
typedef int32_t i32;

const i32 Inf = std::numeric_limits<i32>::max();

struct Size
{
  i32 width;
  i32 height;
};

struct Point
{
  i32 x;
  i32 y;
};

// can't call it Rectangle because conflicts with GDI+ Rectangle function
struct Rect
{
  Point min;
  Point max;

  i32 Width() const;
  i32 Height() const;
};

RECT RectToRECT(const Rect);

i32 clamp(i32 v, i32 vmin, i32 vmax);
i32 scale(i32 v, i64 num, i64 den);
i32 guardInf(i32 a, i32 b);

struct Constraints
{
  Size min;
  Size max;

  Size Constrain(const Size) const;
  Size ConstrainAndAttemptToPreserveAspectRatio(const Size) const;
  i32 ConstrainHeight(i32 height) const;
  i32 ConstrainWidth(i32 width) const;
  bool HasBoundedHeight() const;
  bool HasBoundedWidth() const;
  bool HasTightWidth() const;
  bool HasTightHeight() const;
  Constraints Inset(i32 width, i32 height) const;
  bool IsBounded() const;
  bool IsNormalized() const;
  bool IsTight() const;
  bool IsSatisfiedBy(Size) const;
  bool IsZero() const;
  Constraints Loosen() const;
  Constraints LoosenHeight() const;
  Constraints LoosenWidth() const;
  Constraints Tighten(Size) const;
  Constraints TightenHeight(i32 height) const;
  Constraints TightenWidth(i32 width) const;
};

Constraints Expand();
Constraints ExpandHeight(i32 width);
Constraints ExpandWidth(i32 height);
Constraints Loose(const Size size);
Constraints Tight(const Size size);
Constraints TightHeight(i32 height);

// yes, C++ is really that lame and we have to implement
// dynamic typing manually
// identity of an object is an address
// that it's a string is good for debugging
typedef const char* Kind;

struct ILayout
{
  Kind kind;

  virtual ~ILayout(){};
  virtual Size Layout(const Constraints bc) = 0;
  virtual i32 MinIntrinsicHeight(i32) = 0;
  virtual i32 MinIntrinsicWidth(i32) = 0;
  virtual void SetBounds(const Rect) = 0;
};

// defined as i64 but values are i32
typedef i64 Alignment;

constexpr Alignment AlignStart = -32768;
constexpr Alignment AlignCenter = 0;
constexpr Alignment AlignEnd = 0x7fff;

struct Align : public ILayout
{
  Alignment HAlign;   // Horizontal alignment of child widget.
  Alignment VAlign;   // Vertical alignment of child widget.
  float WidthFactor;  // If greater than zero, ratio of container width to child width.
  float HeightFactor; // If greater than zero, ratio of container height to child height.
  ILayout *Child;

  Size childSize;

  Align();
  ~Align() override;
  Size Layout(const Constraints bc) override;
  int MinIntrinsicHeight(int) override;
  int MinIntrinsicWidth(int) override;
  void SetBounds(const Rect) override;
};

bool IsAlign(Kind);
