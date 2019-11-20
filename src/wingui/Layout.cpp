#include "utils/BaseUtil.h"

#include <cstdio>     // std::printf
#include <limits>     // std::limits
#include <cmath>      // std::is_nan
#include <algorithm>  // std::max
#include <cassert>    // assert
#include <functional> // std::function

#include "Layout.h"

i32 Rect::Width() const {
  return max.x - min.x;
}
i32 Rect::Height() const {
  return max.y - min.y;
}

RECT RectToRECT(const Rect r) {
  LONG left = r.min.x;
  LONG top = r.min.y;
  LONG right = r.max.x;
  LONG bottom = r.max.y;
  RECT res{left, top, right, bottom};
  return res;
}

i32 clamp(i32 v, i32 vmin, i32 vmax)
{
  if (v > vmax)
  {
    return vmax;
  }
  if (v < vmin)
  {
    return vmin;
  }
  return v;
}

i32 scale(i32 v, i64 num, i64 den)
{
  i64 res = (i64(v) * num) / den;
  return i32(res);
}

i32 guardInf(i32 a, i32 b)
{
  if (a == Inf)
  {
    return Inf;
  }
  return b;
}

Constraints Expand()
{
  Size min{0, 0};
  Size max{Inf, Inf};
  return Constraints{min, max};
}

Constraints ExpandHeight(i32 width)
{
  Size min{width, 0};
  Size max{width, Inf};
  return Constraints{min, max};
}

Constraints ExpandWidth(i32 height)
{
  Size min{0, height};
  Size max{Inf, height};
  return Constraints{min, max};
}

Constraints Loose(const Size size)
{
  return Constraints{Size{}, size};
}

Constraints Tight(const Size size)
{
  return Constraints{size, size};
}

Constraints TightHeight(i32 height)
{
  Size min{0, height};
  Size max{Inf, height};
  return Constraints{min, max};
}

Size Constraints::Constrain(Size size) const
{
  i32 w = clamp(size.width, this->min.width, this->max.width);
  i32 h = clamp(size.height, this->min.height, this->max.height);
  return Size{w, h};
}

Size Constraints::ConstrainAndAttemptToPreserveAspectRatio(const Size size) const
{
  if (this->IsTight())
  {
    return this->min;
  }

  i32 width = size.width;
  i32 height = size.height;

  if (width > this->max.width)
  {
    width = this->max.width;
    height = scale(width, size.height, size.width);
  }
  if (height > this->max.height)
  {
    height = this->max.height;
    width = scale(height, size.width, size.height);
  }

  if (width < this->min.width)
  {
    width = this->min.width;
    height = scale(width, size.height, size.width);
  }

  if (height < this->min.height)
  {
    height = this->min.height;
    width = scale(height, size.width, size.height);
  }

  Size c{width, height};
  return this->Constrain(c);
}

i32 Constraints::ConstrainHeight(i32 height) const
{
  return clamp(height, this->min.height, this->max.height);
}

i32 Constraints::ConstrainWidth(i32 width) const
{
  return clamp(width, this->min.width, this->max.width);
}

bool Constraints::HasBoundedHeight() const
{
  return this->max.height < Inf;
}

bool Constraints::HasBoundedWidth() const
{
  return this->max.width < Inf;
}

bool Constraints::HasTightWidth() const
{
  return this->min.width >= this->max.width;
}

bool Constraints::HasTightHeight() const
{
  return this->min.height >= this->max.height;
}

Constraints Constraints::Inset(i32 width, i32 height) const
{
  i32 minw = this->min.width;
  i32 deflatedMinWidth = guardInf(minw, std::max(0, minw - width));
  i32 minh = this->min.height;
  i32 deflatedMinHeight = guardInf(minh, std::max(0, minh - height));
  Size min{deflatedMinWidth, deflatedMinHeight};
  i32 maxw = this->max.width;
  i32 maxh = this->max.height;
  Size max{
      std::max(deflatedMinWidth, guardInf(maxw, maxw - width)),
      std::max(deflatedMinHeight, guardInf(maxh, maxh - height)),
  };
  return Constraints{min, max};
}

bool Constraints::IsBounded() const
{
  return this->HasBoundedWidth() && this->HasBoundedHeight();
}

bool Constraints::IsNormalized() const
{
  return this->min.width >= 0.0 &&
         this->min.width <= this->max.width &&
         this->min.height >= 0.0 &&
         this->min.height <= this->max.height;
}

bool Constraints::IsSatisfiedBy(Size size) const
{
  return this->min.width <= size.width &&
         size.width <= this->max.width &&
         this->min.height <= size.height &&
         size.height <= this->max.height &&
         size.width != Inf &&
         size.height != Inf;
}

bool Constraints::IsTight() const
{
  return this->HasTightWidth() && this->HasTightHeight();
}

bool Constraints::IsZero() const
{
  return this->min.width == 0 && this->min.height == 0 && this->max.width == 0 && this->max.height == 0;
}

Constraints Constraints::Loosen() const
{
  return Constraints{Size{}, this->max};
}

Constraints Constraints::LoosenHeight() const
{
  return Constraints{Size{this->min.width, 0}, this->max};
}

Constraints Constraints::LoosenWidth() const
{
  return Constraints{Size{0, this->min.height}, this->max};
}

// TODO: goey modifies in-place
Constraints Constraints::Tighten(Size size) const
{
  i32 minw = clamp(size.width, this->min.width, this->max.width);
  i32 maxw = this->min.width;
  i32 minh = clamp(size.height, this->min.height, this->max.height);
  i32 maxh = this->min.height;
  return Constraints{
      Size{minw, minh},
      Size{maxw, maxh},
  };
}
 
// TODO: goey modifies in-place
Constraints Constraints::TightenHeight(i32 height) const
{
  i32 minh = clamp(height, this->min.height, this->max.height);
  i32 maxh = this->min.height;
  return Constraints{
      Size{this->min.width, minh},
      Size{this->max.height, maxh},
  };
}

// TODO: goey modifies in-place
Constraints Constraints::TightenWidth(i32 width) const
{
  i32 minw = clamp(width, this->min.width, this->max.width);
  i32 maxw = this->min.width;
  return Constraints{
      Size{minw, this->min.height},
      Size{maxw, this->max.height},
  };
}

const char *alignKind = "align";

bool IsAlign(Kind kind)
{
  return kind == alignKind;
}

Align::Align()
{
  kind = alignKind;
}

Align::~Align() {}

Size Align::Layout(const Constraints bc)
{
  Size size = this->Child->Layout(bc.Loosen());
  this->childSize = size;
  auto f = this->WidthFactor;
  if (f > 0)
  {
    size.width = i32(float(size.width) * f);
  }
  f = this->HeightFactor;
  if (f > 0)
  {
    size.height = i32(float(size.height) * f);
  }
  return bc.Constrain(size);
}

i32 Align::MinIntrinsicHeight(i32 width)
{
  i32 height = this->Child->MinIntrinsicHeight(width);
  auto f = this->HeightFactor;
  if (f > 0)
  {
    return i32(float(height) * f);
  }
  return height;
}

i32 Align::MinIntrinsicWidth(i32 height)
{
  i32 width = this->Child->MinIntrinsicHeight(height);
  auto f = this->WidthFactor;
  if (f > 0)
  {
    return i32(float(width) * f);
  }
  return width;
}

void Align::SetBounds(const Rect bounds) {
  i32 bminx = bounds.min.x;
  i32 bmaxx = bounds.max.x;
  i32 cw = this->childSize.width;
  i64 twm = AlignStart - AlignEnd;
  i64 tw = AlignEnd - AlignStart;
  i32 x = scale(bminx, this->HAlign - AlignEnd, twm) +
          scale(bmaxx - cw, this->HAlign - AlignStart, tw);
  i32 ch = this->childSize.height;
  i32 bminy = bounds.min.y;
  i32 bmaxy = bounds.max.y;
  i32 y = scale(bminy, this->VAlign - AlignEnd, twm) +
          scale(bmaxy - ch, this->VAlign - AlignStart, tw);
  Rect b{
      Point{x, y},
      Point{x + cw, y + ch}};
  this->Child->SetBounds(b);
}
