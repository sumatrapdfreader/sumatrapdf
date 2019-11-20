#include "utils/BaseUtil.h"

#include <cstdio>     // std::printf
#include <limits>     // std::limits
#include <cmath>      // std::is_nan
#include <algorithm>  // std::max
#include <cassert>    // assert
#include <functional> // std::function

#include "Layout.h"

Length Rect::Width() const {
    return Max.X - Min.X;
}
Length Rect::Height() const {
    return Max.Y - Min.Y;
}

Length Rect::Dx() const {
    return this->Max.X - this->Min.X;
}

// Dy returns r's height.
Length Rect::Dy() const {
    return this->Max.Y - this->Min.Y;
}

RECT RectToRECT(const Rect r) {
    LONG left = r.Min.X;
    LONG top = r.Min.Y;
    LONG right = r.Max.X;
    LONG bottom = r.Max.Y;
    RECT res{left, top, right, bottom};
    return res;
}

Length clamp(Length v, Length vmin, Length vmax) {
    if (v > vmax) {
        return vmax;
    }
    if (v < vmin) {
        return vmin;
    }
    return v;
}

Length scale(Length v, i64 num, i64 den) {
    i64 res = (i64(v) * num) / den;
    return Length(res);
}

Length guardInf(Length a, Length b) {
    if (a == Inf) {
        return Inf;
    }
    return b;
}

Constraints Expand() {
    Size min{0, 0};
    Size max{Inf, Inf};
    return Constraints{min, max};
}

Constraints ExpandHeight(Length width) {
    Size min{width, 0};
    Size max{width, Inf};
    return Constraints{min, max};
}

Constraints ExpandWidth(Length height) {
    Size min{0, height};
    Size max{Inf, height};
    return Constraints{min, max};
}

Constraints Loose(const Size size) {
    return Constraints{Size{}, size};
}

Constraints Tight(const Size size) {
    return Constraints{size, size};
}

Constraints TightHeight(Length height) {
    Size min{0, height};
    Size max{Inf, height};
    return Constraints{min, max};
}

Size Constraints::Constrain(Size size) const {
    Length w = clamp(size.Width, this->Min.Width, this->Max.Width);
    Length h = clamp(size.Height, this->Min.Height, this->Max.Height);
    return Size{w, h};
}

Size Constraints::ConstrainAndAttemptToPreserveAspectRatio(const Size size) const {
    if (this->IsTight()) {
        return this->Min;
    }

    Length width = size.Width;
    Length height = size.Height;

    if (width > this->Max.Width) {
        width = this->Max.Width;
        height = scale(width, size.Height, size.Width);
    }
    if (height > this->Max.Height) {
        height = this->Max.Height;
        width = scale(height, size.Width, size.Height);
    }

    if (width < this->Min.Width) {
        width = this->Min.Width;
        height = scale(width, size.Height, size.Width);
    }

    if (height < this->Min.Height) {
        height = this->Min.Height;
        width = scale(height, size.Width, size.Height);
    }

    Size c{width, height};
    return this->Constrain(c);
}

Length Constraints::ConstrainHeight(Length height) const {
    return clamp(height, this->Min.Height, this->Max.Height);
}

Length Constraints::ConstrainWidth(Length width) const {
    return clamp(width, this->Min.Width, this->Max.Width);
}

bool Constraints::HasBoundedHeight() const {
    return this->Max.Height < Inf;
}

bool Constraints::HasBoundedWidth() const {
    return this->Max.Width < Inf;
}

bool Constraints::HasTightWidth() const {
    return this->Min.Width >= this->Max.Width;
}

bool Constraints::HasTightHeight() const {
    return this->Min.Height >= this->Max.Height;
}

Constraints Constraints::Inset(Length width, Length height) const {
    Length minw = this->Min.Width;
    Length deflatedMinWidth = guardInf(minw, std::max(0, minw - width));
    Length minh = this->Min.Height;
    Length deflatedMinHeight = guardInf(minh, std::max(0, minh - height));
    Size min{deflatedMinWidth, deflatedMinHeight};
    Length maxw = this->Max.Width;
    Length maxh = this->Max.Height;
    Size max{
        std::max(deflatedMinWidth, guardInf(maxw, maxw - width)),
        std::max(deflatedMinHeight, guardInf(maxh, maxh - height)),
    };
    return Constraints{min, max};
}

bool Constraints::IsBounded() const {
    return this->HasBoundedWidth() && this->HasBoundedHeight();
}

bool Constraints::IsNormalized() const {
    return this->Min.Width >= 0.0 && this->Min.Width <= this->Max.Width && this->Min.Height >= 0.0 &&
           this->Min.Height <= this->Max.Height;
}

bool Constraints::IsSatisfiedBy(Size size) const {
    return this->Min.Width <= size.Width && size.Width <= this->Max.Width && this->Min.Height <= size.Height &&
           size.Height <= this->Max.Height && size.Width != Inf && size.Height != Inf;
}

bool Constraints::IsTight() const {
    return this->HasTightWidth() && this->HasTightHeight();
}

bool Constraints::IsZero() const {
    return this->Min.Width == 0 && this->Min.Height == 0 && this->Max.Width == 0 && this->Max.Height == 0;
}

Constraints Constraints::Loosen() const {
    return Constraints{Size{}, this->Max};
}

Constraints Constraints::LoosenHeight() const {
    return Constraints{Size{this->Min.Width, 0}, this->Max};
}

Constraints Constraints::LoosenWidth() const {
    return Constraints{Size{0, this->Min.Height}, this->Max};
}

// TODO: goey modifies in-place
Constraints Constraints::Tighten(Size size) const {
    Length minw = clamp(size.Width, this->Min.Width, this->Max.Width);
    Length maxw = this->Min.Width;
    Length minh = clamp(size.Height, this->Min.Height, this->Max.Height);
    Length maxh = this->Min.Height;
    return Constraints{
        Size{minw, minh},
        Size{maxw, maxh},
    };
}

// TODO: goey modifies in-place
Constraints Constraints::TightenHeight(Length height) const {
    Length minh = clamp(height, this->Min.Height, this->Max.Height);
    Length maxh = this->Min.Height;
    return Constraints{
        Size{this->Min.Width, minh},
        Size{this->Max.Height, maxh},
    };
}

// TODO: goey modifies in-place
Constraints Constraints::TightenWidth(Length width) const {
    Length minw = clamp(width, this->Min.Width, this->Max.Width);
    Length maxw = this->Min.Width;
    return Constraints{
        Size{minw, this->Min.Height},
        Size{maxw, this->Max.Height},
    };
}

// padding.go

bool IsLayoutOfKind(ILayout* l, Kind kind) {
    if (l == nullptr) {
        return false;
    }
    return l->kind == kind;
}

Kind paddingKind = "padding";
bool IsPadding(Kind kind) {
    return kind == paddingKind;
}
bool IsPadding(ILayout* l) {
    return IsLayoutOfKind(l, paddingKind);
}

Padding::~Padding() {
}

Size Padding::Layout(const Constraints bc) {
    auto hinset = this->insets.Left + this->insets.Right;
    auto vinset = this->insets.Top + this->insets.Bottom;

    auto innerConstraints = bc.Inset(hinset, vinset);
    this->childSize = this->child->Layout(innerConstraints);
    return Size{
        this->childSize.Width + hinset,
        this->childSize.Height + vinset,
    };
}

Length Padding::MinIntrinsicHeight(Length width) {
    auto vinset = this->insets.Top + this->insets.Bottom;
    return this->child->MinIntrinsicHeight(width) + vinset;
}

Length Padding::MinIntrinsicWidth(Length height) {
    auto hinset = this->insets.Left + this->insets.Right;
    return this->child->MinIntrinsicWidth(height) + hinset;
}

void Padding::SetBounds(Rect bounds) {
    bounds.Min.X += this->insets.Left;
    bounds.Min.Y += this->insets.Top;
    bounds.Max.X -= this->insets.Right;
    bounds.Max.Y -= this->insets.Bottom;
    this->child->SetBounds(bounds);
}

Length DIP = 1;

// layout.go
extern bool IsButton(Kind);

Length calculateHGap(ILayout* previous, ILayout* current) {
    // The vertical gap between most controls is 11 relative pixels.  However,
    // there are different rules for between a label and its associated control,
    // or between related controls.  These relationship do not appear in the
    // model provided by this package, so these relationships need to be
    // inferred from the order and type of controls.
    //
    // https://msdn.microsoft.com/en-us/library/windows/desktop/dn742486.aspx#sizingandspacing
    if (IsButton(previous->kind)) {
        if (IsButton(current->kind)) {
            // Any pair of successive buttons will be assumed to be in a
            // related group.
            return 7 * DIP;
        }
    }

    // The spacing between unrelated controls.
    return 11 * DIP;
}

Length calculateVGap(ILayout* previous, ILayout* current) {
    // The vertical gap between most controls is 11 relative pixels.  However,
    // there are different rules for between a label and its associated control,
    // or between related controls.  These relationship do not appear in the
    // model provided by this package, so these relationships need to be
    // inferred from the order and type of controls.
    //
    // https://msdn.microsoft.com/en-us/library/windows/desktop/dn742486.aspx#sizingandspacing

    // Unwrap and Expand widgets.
    if (IsExpand(previous)) {
        // previous = expand.child
    }
    if (IsExpand(current)) {
        // current = expand.child
    }

    // Apply layout rules.
    if (IsLabel(previous)) {
        // Any label immediately preceding any other control will be assumed to
        // be 'associated'.
        return 5 * DIP;
    }
    if (IsCheckbox(previous)) {
        if (IsCheckbox(current)) {
            // Any pair of successive checkboxes will be assumed to be in a
            // related group.
            return 7 * DIP;
        }
    }

    // The spacing between unrelated controls.  This is also the default space
    // between paragraphs of text.
    return 11 * DIP;
}

// vbox.go

Kind vboxKind = "vbox";
bool IsVBox(Kind kind) {
    return kind == vboxKind;
}

VBox::~VBox() {
}

Size VBox::Layout(const Constraints bc) {
    auto n = this->children.size();
    if (n == 0) {
        this->totalHeight = 0;
        return bc.Constrain(Size{});
    }
    // Determine the constraints for layout of child elements.
    auto cbc = bc;

    if (this->alignMain == MainAxisAlign::Homogeneous) {
        auto count = (i64)this->children.size();
        auto gap = calculateVGap(nullptr, nullptr);
        cbc.TightenHeight(scale(cbc.Max.Height, 1, count) - scale(gap, count - 1, count));
    } else {
        cbc.Min.Height = 0;
        cbc.Max.Height = Inf;
    }

    if (this->alignCross == CrossAxisAlign::Stretch) {
        if (cbc.HasBoundedWidth()) {
            cbc = cbc.TightenWidth(cbc.Max.Width);
        } else {
            cbc = cbc.TightenWidth(this->MinIntrinsicWidth(Inf));
        }
    } else {
        cbc = cbc.LoosenWidth();
    }
    auto height = Length(0);
    auto width = Length(0);
    ILayout* previous = nullptr;

    for (size_t i = 0; i < n; i++) {
        auto v = this->children.at(i);
        // Determine what gap needs to be inserted between the elements.
        if (i > 0) {
            if (IsPacked(this->alignMain)) {
                height += calculateVGap(previous, v);
            } else {
                height += calculateVGap(nullptr, nullptr);
            }
        }
        previous = v;

        // Perform layout of the element.  Track impact on width and height.
        auto size = v->Layout(cbc);
        this->childrenInfo[i].size = size; // TODO: does that work?
        height += size.Height;
        width = std::max(width, size.Width);
    }
    this->totalHeight = height;

    // Need to adjust width to any widgets that have flex
    if (this->totalFlex > 0) {
        auto extraHeight = Length(0);
        if (bc.HasBoundedHeight() && bc.Max.Height > this->totalHeight) {
            extraHeight = bc.Max.Height - this->totalHeight;
        } else if (bc.Min.Height > this->totalHeight) {
            extraHeight = bc.Min.Height - this->totalHeight;
        }

        if (extraHeight > 0) {
            // size_t n = this->childrenInfo.size();
            for (size_t i = 0; i < n; i++) {
                auto& v = this->childrenInfo.at(i);
                if (v.flex > 0) {
                    auto oldHeight = v.size.Height;
                    auto fbc = cbc.TightenHeight(v.size.Height + scale(extraHeight, v.flex, this->totalFlex));
                    auto size = this->children[i]->Layout(fbc);
                    this->childrenInfo[i].size = size;
                    this->totalHeight += size.Height - oldHeight;
                }
            }
        }
    }
    if (this->alignCross == CrossAxisAlign::Stretch) {
        return bc.Constrain(Size{cbc.Min.Width, height});
    }

    return bc.Constrain(Size{width, height});
}

Length VBox::MinIntrinsicWidth(Length height) {
    auto n = this->children.size();
    if (n == 0) {
        return 0;
    }
    if (this->alignMain == MainAxisAlign::Homogeneous) {
        height = guardInf(height, scale(height, 1, i64(n)));
        auto size = this->children[0]->MinIntrinsicWidth(height);
        for (size_t i = 1; i < n; i++) {
            auto v = this->children[i];
            size = std::max(size, v->MinIntrinsicWidth(height));
        }
        return size;
    }
    auto size = this->children[0]->MinIntrinsicWidth(Inf);
    for (size_t i = 1; i < n; i++) {
        auto v = this->children[i];
        size = std::max(size, v->MinIntrinsicWidth(Inf));
    }
    return size;
}

Length VBox::MinIntrinsicHeight(Length width) {
    auto n = this->children.size();
    if (n == 0) {
        return 0;
    }
    auto size = this->children[0]->MinIntrinsicHeight(width);
    if (IsPacked(this->alignMain)) {
        auto previous = this->children[0];
        for (size_t i = 1; i < n; i++) {
            auto v = this->children[i];
            // Add the preferred gap between this pair of widgets
            size += calculateVGap(previous, v);
            previous = v;
            // Find minimum size for this widget, and update
            size += v->MinIntrinsicHeight(width);
        }
        return size;
    }

    if (this->alignMain == MainAxisAlign::Homogeneous) {
        for (size_t i = 1; i < n; i++) {
            auto v = this->children[i];
            size = std::max(size, v->MinIntrinsicHeight(width));
        }

        // Add a minimum gap between the controls.
        auto vgap = calculateVGap(nullptr, nullptr);
        size = scale(size, i64(n), 1) + scale(vgap, i64(n) - 1, 1);
        return size;
    }

    for (size_t i = 1; i < n; i++) {
        auto v = this->children[i];
        size += v->MinIntrinsicHeight(width);
    }

    // Add a minimum gap between the controls.
    auto vgap = calculateVGap(nullptr, nullptr);
    if (this->alignMain == MainAxisAlign::SpaceBetween) {
        size += scale(vgap, i64(n) - 1, 1);
    } else {
        size += scale(vgap, i64(n) + 1, 1);
    }

    return size;
}

void VBox::SetBounds(Rect bounds) {
    auto n = this->children.size();
    if (n == 0) {
        return;
    }

    if (this->alignMain == MainAxisAlign::Homogeneous) {
        auto gap = calculateVGap(nullptr, nullptr);
        auto dy = bounds.Dy() + gap;
        auto count = i64(n);

        for (size_t i = 0; i < n; i++) {
            auto v = children[i];
            auto y1 = bounds.Min.Y + scale(dy, i, count);
            auto y2 = bounds.Min.Y + scale(dy, i + 1, count) - gap;
            setBoundsForChild((int)i, v, bounds.Min.X, y1, bounds.Max.X, y2);
        }
        return;
    }

    // Adjust the bounds so that the minimum Y handles vertical alignment
    // of the controls.  We also calculate 'extraGap' which will adjust
    // spacing of the controls for non-packed alignments.
    auto extraGap = Length(0);
    if (totalFlex == 0) {
        switch (alignMain) {
            case MainAxisAlign::MainStart:
                // Do nothing
                break;
            case MainAxisAlign::MainCenter:
                bounds.Min.Y += (bounds.Dy() - totalHeight) / 2;
                break;
            case MainAxisAlign::MainEnd:
                bounds.Min.Y = bounds.Max.Y - totalHeight;
                break;
            case MainAxisAlign::SpaceAround:
            {
                Length l = (bounds.Dy() - totalHeight);
                extraGap = scale(l, 1, i64(n) + 1);
                bounds.Min.Y += extraGap;
                extraGap += calculateVGap(nullptr, nullptr);
                break;
            }
            case MainAxisAlign::SpaceBetween:
                if (n > 1) {
                    Length l = (bounds.Dy() - totalHeight);
                    extraGap = scale(l, 1, i64(n) - 1);
                    extraGap += calculateVGap(nullptr, nullptr);
                } else {
                    // There are no controls between which to put the extra space.
                    // The following essentially convert SpaceBetween to SpaceAround
                    bounds.Min.Y += (bounds.Dy() - totalHeight) / 2;
                }
                break;
        }
    }

    // Position all of the child controls.
    auto posY = bounds.Min.Y;
    ILayout* previous = nullptr;
    for (size_t i = 0; i < n; i++) {
        auto v = children[i];
        if (IsPacked(alignMain)) {
            if (i > 0) {
                posY += calculateVGap(previous, v);
            }
            previous = v;
        }

        auto dy = childrenInfo[i].size.Height;
        setBoundsForChild((int)i, v, bounds.Min.X, posY, bounds.Max.X, posY + dy);
        posY += dy + extraGap;
    }
}

void VBox::setBoundsForChild(int i, ILayout* v, Length posX, Length posY, Length posX2, Length posY2) {
    auto dx = childrenInfo[i].size.Width;
    switch (alignCross) {
        case CrossAxisAlign::CrossStart:
            v->SetBounds(Rect{
                Point{posX, posY},
                Point{posX + dx, posY2},
            });
            break;
        case CrossAxisAlign::CrossCenter:
            v->SetBounds(Rect{
                Point{posX + (posX2 - posX - dx) / 2, posY},
                Point{posX + (posX2 - posX + dx) / 2, posY2},
            });
            break;
        case CrossAxisAlign::CrossEnd:
            v->SetBounds(Rect{
                Point{posX2 - dx, posY},
                Point{posX2, posY2},
            });
            break;
        case CrossAxisAlign::Stretch:
            v->SetBounds(Rect{
                Point{posX, posY},
                Point{posX2, posY2},
            });
            break;
    }
}

void VBox::addChild(ILayout* child) {
    children.Append(child);
    boxElementInfo el;
    childrenInfo.Append(el);
}

// align.go

Kind alignKind = "align";

bool IsAlign(Kind kind) {
    return kind == alignKind;
}

Align::Align() {
    kind = alignKind;
}

Align::~Align() {
}

Size Align::Layout(const Constraints bc) {
    Size size = this->Child->Layout(bc.Loosen());
    this->childSize = size;
    auto f = this->WidthFactor;
    if (f > 0) {
        size.Width = i32(float(size.Width) * f);
    }
    f = this->HeightFactor;
    if (f > 0) {
        size.Height = i32(float(size.Height) * f);
    }
    return bc.Constrain(size);
}

Length Align::MinIntrinsicHeight(Length width) {
    Length height = this->Child->MinIntrinsicHeight(width);
    auto f = this->HeightFactor;
    if (f > 0) {
        return Length(float(height) * f);
    }
    return height;
}

Length Align::MinIntrinsicWidth(Length height) {
    Length width = this->Child->MinIntrinsicHeight(height);
    auto f = this->WidthFactor;
    if (f > 0) {
        return Length(float(width) * f);
    }
    return width;
}

void Align::SetBounds(Rect bounds) {
    Length bminx = bounds.Min.X;
    Length bmaxx = bounds.Max.X;
    Length cw = this->childSize.Width;
    i64 twm = AlignStart - AlignEnd;
    i64 tw = AlignEnd - AlignStart;
    Length x = scale(bminx, this->HAlign - AlignEnd, twm) + scale(bmaxx - cw, this->HAlign - AlignStart, tw);
    Length ch = this->childSize.Height;
    Length bminy = bounds.Min.Y;
    Length bmaxy = bounds.Max.Y;
    Length y = scale(bminy, this->VAlign - AlignEnd, twm) + scale(bmaxy - ch, this->VAlign - AlignStart, tw);
    Rect b{Point{x, y}, Point{x + cw, y + ch}};
    this->Child->SetBounds(b);
}

// TODO: implement me
bool IsExpand(Kind) {
    return false;
}
bool IsExpand(ILayout*) {
    return false;
}

bool IsLabeL(Kind) {
    return false;
}
bool IsLabel(ILayout*) {
    return false;
}

bool IsCheckbox(Kind) {
    return false;
}
bool IsCheckbox(ILayout*) {
    return false;
}
