/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/Dpi.h"

#include "Layout.h"

bool Size::empty() const {
    return Width == 0 || Height == 0;
}

bool Point::empty() const {
    return X == 0 || Y == 0;
}

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

bool Rect::empty() const {
    return Dx() == 0 || Dy() == 0;
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

Constraints ExpandInf() {
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

Constraints Constraints::Tighten(Size size) const {
    Constraints bc = *this;
    bc.Min.Width = clamp(size.Width, bc.Min.Width, bc.Max.Width);
    bc.Max.Width = bc.Min.Width;
    bc.Min.Height = clamp(size.Height, bc.Min.Height, bc.Max.Height);
    bc.Max.Height = bc.Min.Height;
    return bc;
}

Constraints Constraints::TightenHeight(Length height) const {
    Constraints bc = *this;
    bc.Min.Height = clamp(height, bc.Min.Height, bc.Max.Height);
    bc.Max.Height = bc.Min.Height;
    return bc;
}

Constraints Constraints::TightenWidth(Length width) const {
    Constraints bc = *this;

    bc.Min.Width = clamp(width, bc.Min.Width, bc.Max.Width);
    bc.Max.Width = bc.Min.Width;
    return bc;
}

void LayoutManager::NeedLayout() {
    needLayout = true;
}

void ILayout::SetIsVisible(bool newIsVisible) {
    isVisible = newIsVisible;
    if (layoutManager) {
        layoutManager->NeedLayout();
    }
}

bool IsLayoutOfKind(ILayout* l, Kind kind) {
    if (l == nullptr) {
        return false;
    }
    return l->kind == kind;
}

// padding.go

Kind paddingKind = "padding";
bool IsPadding(Kind kind) {
    return kind == paddingKind;
}
bool IsPadding(ILayout* l) {
    return IsLayoutOfKind(l, paddingKind);
}

Padding::~Padding() {
    delete child;
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

// layout.go
ILayout::ILayout(Kind k) {
    kind = k;
}

Length calculateHGap(ILayout* previous, ILayout* current) {
    // The vertical gap between most controls is 11 relative pixels.  However,
    // there are different rules for between a label and its associated control,
    // or between related controls.  These relationship do not appear in the
    // model provided by this package, so these relationships need to be
    // inferred from the order and type of controls.
    //
    // https://msdn.microsoft.com/en-us/library/windows/desktop/dn742486.aspx#sizingandspacing
    if (IsButton(previous)) {
        if (IsButton(current)) {
            // Any pair of successive buttons will be assumed to be in a
            // related group.
            return DpiScale(8);
        }
    }

    // The spacing between unrelated controls.
    return DpiScale(11);
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
        Expand* expand = (Expand*)previous;
        previous = expand->child;
    }
    if (IsExpand(current)) {
        Expand* expand = (Expand*)current;
        current = expand->child;
    }

    // Apply layout rules.
    if (IsLabel(previous)) {
        // Any label immediately preceding any other control will be assumed to
        // be 'associated'.
        return DpiScale(2);
    }

    if (IsCheckbox(previous)) {
        if (IsCheckbox(current)) {
            // Any pair of successive checkboxes will be assumed to be in a
            // related group.
            // return DpiScale(2);
            return 0;
        }
    }

    // The spacing between unrelated controls.  This is also the default space
    // between paragraphs of text.
    return DpiScale(8);
}

// vbox.go

Kind kindVBox = "vbox";
bool IsVBox(Kind kind) {
    return kind == kindVBox;
}

VBox::~VBox() {
    for (auto& c : children) {
        delete c.layout;
    }
}

size_t VBox::childrenCount() {
    size_t n = 0;
    for (auto& c : children) {
        if (c.layout->isVisible) {
            n++;
        }
    }
    return n;
}

int updateFlex(Vec<boxElementInfo>& children, MainAxisAlign alignMain) {
    if (alignMain == MainAxisAlign::Homogeneous) {
        return 0;
    }
    int totalFlex = 0;
    for (auto& i : children) {
        if (i.layout->isVisible) {
            totalFlex += i.flex;
        }
    }
    return totalFlex;
}

Size VBox::Layout(const Constraints bc) {
    auto n = childrenCount();
    if (n == 0) {
        totalHeight = 0;
        return bc.Constrain(Size{});
    }
    totalFlex = updateFlex(this->children, this->alignMain);

    // Determine the constraints for layout of child elements.
    auto cbc = bc;

    if (this->alignMain == MainAxisAlign::Homogeneous) {
        auto count = (i64)this->childrenCount();
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
        auto& v = this->children.at(i);
        // Determine what gap needs to be inserted between the elements.
        if (i > 0) {
            if (IsPacked(this->alignMain)) {
                height += calculateVGap(previous, v.layout);
            } else {
                height += calculateVGap(nullptr, nullptr);
            }
        }
        previous = v.layout;

        // Perform layout of the element.  Track impact on width and height.
        auto size = v.layout->Layout(cbc);
        v.size = size; // TODO: does that work?
        height += size.Height;
        width = std::max(width, size.Width);
    }
    totalHeight = height;

    // Need to adjust width to any widgets that have flex
    if (totalFlex > 0) {
        auto extraHeight = Length(0);
        if (bc.HasBoundedHeight() && bc.Max.Height > this->totalHeight) {
            extraHeight = bc.Max.Height - this->totalHeight;
        } else if (bc.Min.Height > this->totalHeight) {
            extraHeight = bc.Min.Height - this->totalHeight;
        }

        if (extraHeight > 0) {
            for (auto& v : children) {
                if (v.flex > 0) {
                    auto oldHeight = v.size.Height;
                    auto extra = scale(extraHeight, v.flex, totalFlex);
                    auto fbc = cbc.TightenHeight(v.size.Height + extra);
                    auto size = v.layout->Layout(fbc);
                    v.size = size;
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
    auto n = this->childrenCount();
    if (n == 0) {
        return 0;
    }
    if (this->alignMain == MainAxisAlign::Homogeneous) {
        height = guardInf(height, scale(height, 1, i64(n)));
        auto size = children[0].layout->MinIntrinsicWidth(height);
        for (size_t i = 1; i < n; i++) {
            auto& v = children[i];
            size = std::max(size, v.layout->MinIntrinsicWidth(height));
        }
        return size;
    }
    auto size = children[0].layout->MinIntrinsicWidth(Inf);
    for (size_t i = 1; i < n; i++) {
        auto& v = this->children[i];
        size = std::max(size, v.layout->MinIntrinsicWidth(Inf));
    }
    return size;
}

Length VBox::MinIntrinsicHeight(Length width) {
    auto n = this->childrenCount();
    if (n == 0) {
        return 0;
    }
    auto size = children[0].layout->MinIntrinsicHeight(width);
    if (IsPacked(this->alignMain)) {
        auto previous = children[0].layout;
        for (size_t i = 1; i < n; i++) {
            auto& v = children[i];
            // Add the preferred gap between this pair of widgets
            size += calculateVGap(previous, v.layout);
            previous = v.layout;
            // Find minimum size for this widget, and update
            size += v.layout->MinIntrinsicHeight(width);
        }
        return size;
    }

    if (this->alignMain == MainAxisAlign::Homogeneous) {
        for (size_t i = 1; i < n; i++) {
            auto& v = this->children[i];
            size = std::max(size, v.layout->MinIntrinsicHeight(width));
        }

        // Add a minimum gap between the controls.
        auto vgap = calculateVGap(nullptr, nullptr);
        size = scale(size, i64(n), 1) + scale(vgap, i64(n) - 1, 1);
        return size;
    }

    for (size_t i = 1; i < n; i++) {
        auto& v = this->children[i];
        size += v.layout->MinIntrinsicHeight(width);
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
    auto n = childrenCount();
    if (n == 0) {
        return;
    }

    if (this->alignMain == MainAxisAlign::Homogeneous) {
        auto gap = calculateVGap(nullptr, nullptr);
        auto dy = bounds.Dy() + gap;
        auto count = i64(n);

        for (size_t i = 0; i < n; i++) {
            auto& v = children[i];
            auto y1 = bounds.Min.Y + scale(dy, i, count);
            auto y2 = bounds.Min.Y + scale(dy, i + 1, count) - gap;
            setBoundsForChild(i, v.layout, bounds.Min.X, y1, bounds.Max.X, y2);
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
            case MainAxisAlign::SpaceAround: {
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
        auto& v = children[i];
        if (IsPacked(alignMain)) {
            if (i > 0) {
                posY += calculateVGap(previous, v.layout);
            }
            previous = v.layout;
        }

        auto dy = v.size.Height;
        setBoundsForChild(i, v.layout, bounds.Min.X, posY, bounds.Max.X, posY + dy);
        posY += dy + extraGap;
    }
}

void VBox::setBoundsForChild(size_t i, ILayout* v, Length posX, Length posY, Length posX2, Length posY2) {
    auto dx = children[i].size.Width;
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

boxElementInfo& VBox::addChild(ILayout* child, int flex) {
    boxElementInfo v{};
    v.layout = child;
    v.flex = flex;
    children.Append(v);
    auto n = children.size();
    return children[n - 1];
}

boxElementInfo& VBox::addChild(ILayout* child) {
    return addChild(child, 0);
}

// hbox.go
Kind kindHBox = "hbox";

bool IsHBox(Kind kind) {
    return kind == kindHBox;
}

bool IsHBox(ILayout* l) {
    return IsLayoutOfKind(l, kindHBox);
}

HBox::~HBox() {
    for (auto& c : children) {
        delete c.layout;
    }
}

size_t HBox::childrenCount() {
    size_t n = 0;
    for (auto& c : children) {
        if (c.layout->isVisible) {
            n++;
        }
    }
    return n;
}

Size HBox::Layout(const Constraints bc) {
    auto n = childrenCount();
    if (n == 0) {
        totalWidth = 0;
        return bc.Constrain(Size{});
    }
    totalFlex = updateFlex(this->children, this->alignMain);

    // Determine the constraints for layout of child elements.
    auto cbc = bc;
    if (alignMain == MainAxisAlign::Homogeneous) {
        auto count = i64(n);
        auto gap = calculateHGap(nullptr, nullptr);
        auto maxw = cbc.Max.Width;
        cbc.TightenWidth(scale(maxw, 1, count) - scale(gap, count - 1, count));
    } else {
        cbc.Min.Width = 0;
        cbc.Max.Width = Inf;
    }

    if (alignCross == CrossAxisAlign::Stretch) {
        if (cbc.HasBoundedHeight()) {
            cbc = cbc.TightenHeight(cbc.Max.Height);
        } else {
            cbc = cbc.TightenHeight(MinIntrinsicHeight(Inf));
        }
    } else {
        cbc = cbc.LoosenHeight();
    }
    auto width = Length(0);
    auto height = Length(0);
    ILayout* previous = nullptr;

    for (size_t i = 0; i < n; i++) {
        auto& v = children[i];
        if (!v.layout->isVisible) {
            continue;
        }
        // Determine what gap needs to be inserted between the elements.
        if (i > 0) {
            if (IsPacked(alignMain)) {
                width += calculateHGap(previous, v.layout);
            } else {
                width += calculateHGap(nullptr, nullptr);
            }
        }
        previous = v.layout;

        // Perform layout of the element.  Track impact on width and height.
        auto size = v.layout->Layout(cbc);
        v.size = size;
        width += size.Width;
        height = std::max(height, size.Height);
    }
    totalWidth = width;

    // Need to adjust height to any widgets that have flex
    if (totalFlex > 0) {
        auto extraWidth = Length(0);
        if (bc.HasBoundedWidth() && bc.Max.Width > totalWidth) {
            extraWidth = bc.Max.Width - totalWidth;
        } else if (bc.Min.Width > totalWidth) {
            extraWidth = bc.Min.Width - totalWidth;
        }

        if (extraWidth > 0) {
            for (size_t i = 0; i < n; i++) {
                auto& v = children[i];
                if (v.flex > 0) {
                    auto oldWidth = v.size.Width;
                    auto nw = v.size.Width + extraWidth;
                    auto fbc = cbc.TightenWidth(scale(nw, v.flex, totalFlex));
                    auto size = v.layout->Layout(fbc);
                    v.size = size;
                    totalWidth += size.Width - oldWidth;
                }
            }
        }
    }
    if (alignCross == CrossAxisAlign::Stretch) {
        return bc.Constrain(Size{width, cbc.Min.Height});
    }
    return bc.Constrain(Size{width, height});
}

Length HBox::MinIntrinsicHeight(Length width) {
    auto n = childrenCount();
    if (n == 0) {
        return 0;
    }

    if (alignMain == MainAxisAlign::Homogeneous) {
        width = guardInf(width, scale(width, 1, i64(n)));
        auto size = children[0].layout->MinIntrinsicHeight(width);
        for (size_t i = 1; i < n; i++) {
            auto v = children[i];
            size = std::max(size, v.layout->MinIntrinsicHeight(width));
        }
        return size;
    }

    auto size = children[0].layout->MinIntrinsicHeight(Inf);
    for (size_t i = 1; i < n; i++) {
        auto& v = children[i];
        size = std::max(size, v.layout->MinIntrinsicHeight(Inf));
    }
    return size;
}

Length HBox::MinIntrinsicWidth(Length height) {
    auto n = childrenCount();
    if (n == 0) {
        return 0;
    }

    auto size = children[0].layout->MinIntrinsicWidth(height);
    if (IsPacked(alignMain)) {
        auto previous = children[0].layout;
        for (size_t i = 1; i < n; i++) {
            auto& v = children[i];
            // Add the preferred gap between this pair of widgets
            size += calculateHGap(previous, v.layout);
            previous = v.layout;
            // Find minimum size for this widget, and update
            size += v.layout->MinIntrinsicWidth(height);
        }
        return size;
    }

    if (alignMain == MainAxisAlign::Homogeneous) {
        for (size_t i = 1; i < n; i++) {
            auto& v = children[i];
            size = std::max(size, v.layout->MinIntrinsicWidth(height));
        }

        // Add a minimum gap between the controls.
        auto hgap = calculateHGap(nullptr, nullptr);
        size = scale(size, i64(n), 1) + scale(hgap, i64(n) - 1, 1);
        return size;
    }

    for (size_t i = 1; i < n; i++) {
        auto v = children[i].layout;
        size += v->MinIntrinsicWidth(height);
    }

    // Add a minimum gap between the controls.
    auto hgap = calculateHGap(nullptr, nullptr);
    if (alignMain == MainAxisAlign::SpaceBetween) {
        size += scale(hgap, i64(n) - 1, 1);
    } else {
        size += scale(hgap, i64(n) + 1, 1);
    }

    return size;
}

void HBox::SetBounds(Rect bounds) {
    auto n = childrenCount();
    if (n == 0) {
        return;
    }

    if (alignMain == MainAxisAlign::Homogeneous) {
        auto gap = calculateHGap(nullptr, nullptr);
        auto dx = bounds.Dx() + gap;
        auto count = i64(n);

        for (size_t i = 0; i < n; i++) {
            auto v = children[i].layout;
            auto x1 = bounds.Min.X + scale(dx, i, count);
            auto x2 = bounds.Min.X + scale(dx, i + 1, count) - gap;
            setBoundsForChild(i, v, x1, bounds.Min.Y, x2, bounds.Max.Y);
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
                bounds.Min.X += (bounds.Dx() - totalWidth) / 2;
                break;
            case MainAxisAlign::MainEnd:
                bounds.Min.X = bounds.Max.X - totalWidth;
                break;
            case MainAxisAlign::SpaceAround: {
                auto eg = (bounds.Dx() - totalWidth);
                extraGap = scale(eg, 1, i64(n) + 1);
                bounds.Min.X += extraGap;
                extraGap += calculateHGap(nullptr, nullptr);
            } break;
            case MainAxisAlign::SpaceBetween:
                if (n > 1) {
                    auto eg = (bounds.Dx() - totalWidth);
                    extraGap = scale(eg, 1, i64(n) - 1);
                    extraGap += calculateHGap(nullptr, nullptr);
                } else {
                    // There are no controls between which to put the extra space.
                    // The following essentially convert SpaceBetween to SpaceAround
                    bounds.Min.X += (bounds.Dx() - totalWidth) / 2;
                }
                break;
        }
    }

    // Position all of the child controls.
    auto posX = bounds.Min.X;
    ILayout* previous = nullptr;
    for (size_t i = 0; i < n; i++) {
        auto& v = children[i];
        if (IsPacked(alignMain)) {
            if (i > 0) {
                posX += calculateHGap(previous, v.layout);
            }
            previous = v.layout;
        }

        auto dx = children[i].size.Width;
        setBoundsForChild(i, v.layout, posX, bounds.Min.Y, posX + dx, bounds.Max.Y);
        posX += dx + extraGap;
    }
}

void HBox::setBoundsForChild(size_t i, ILayout* v, Length posX, Length posY, Length posX2, Length posY2) {
    auto dy = children[i].size.Height;
    switch (alignCross) {
        case CrossAxisAlign::CrossStart:
            v->SetBounds(Rect{
                Point{posX, posY},
                Point{posX2, posY + dy},
            });
            break;
        case CrossAxisAlign::CrossCenter:
            v->SetBounds(Rect{
                Point{posX, posY + (posY2 - posY - dy) / 2},
                Point{posX2, posY + (posY2 - posY + dy) / 2},
            });
            break;
        case CrossAxisAlign::CrossEnd:
            v->SetBounds(Rect{
                Point{posX, posY2 - dy},
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

boxElementInfo& HBox::addChild(ILayout* child, int flex) {
    boxElementInfo v{};
    v.layout = child;
    v.flex = flex;
    children.Append(v);
    auto n = children.size();
    return children[n - 1];
}

boxElementInfo& HBox::addChild(ILayout* child) {
    return addChild(child, 0);
}

// align.go

Kind kindAlign = "align";

bool IsAlign(Kind kind) {
    return kind == kindAlign;
}

bool IsAlign(ILayout* l) {
    return IsLayoutOfKind(l, kindAlign);
}

Align::Align(ILayout* c) {
    Child = c;
    kind = kindAlign;
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

// expand.go

Kind kindExpand = "expand";

bool IsExpand(Kind kind) {
    return kind == kindExpand;
}

bool IsExpand(ILayout* l) {
    return IsLayoutOfKind(l, kindExpand);
}

Expand::Expand(ILayout* c, int f) {
    kind = kindExpand;
    child = c;
    factor = f;
}

Expand* CreateExpand(ILayout* child, int factor) {
    return new Expand{child, factor};
}

Expand::~Expand() {
}

Size Expand::Layout(const Constraints bc) {
    return child->Layout(bc);
}

Length Expand::MinIntrinsicHeight(Length width) {
    return child->MinIntrinsicHeight(width);
}

Length Expand::MinIntrinsicWidth(Length height) {
    return child->MinIntrinsicWidth(height);
}

void Expand::SetBounds(Rect bounds) {
    return child->SetBounds(bounds);
}

static Kind kindLabel = "label";

bool IsLabeL(Kind kind) {
    return kind == kindLabel;
}

extern bool IsStatic(Kind);

bool IsLabel(ILayout* l) {
    if (!l) {
        return false;
    }
    return IsLayoutOfKind(l, kindLabel) || IsStatic(l->kind);
}
