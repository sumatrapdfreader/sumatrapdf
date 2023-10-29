/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/Dpi.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"

#include "Layout.h"

bool gEnableDebugLayout = false;

void dbglayoutf(const char* fmt, ...) {
    if (!gEnableDebugLayout) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    AutoFreeStr s = str::FmtV(fmt, args);
    OutputDebugStringA(s.Get());
    va_end(args);
}

static void LogAppendNum(str::Str& s, int n, const char* suffix) {
    if (n == Inf) {
        s.Append("Inf");
    } else {
        s.AppendFmt("%d", n);
    }
    if (suffix) {
        s.Append(suffix);
    }
}

void LogConstraints(Constraints c, const char* suffix) {
    str::Str s;
    if (c.min.dx == c.max.dx) {
        dbglayoutf("dx: ");
        LogAppendNum(s, c.min.dx, " ");
    } else {
        dbglayoutf("dx: ");
        LogAppendNum(s, c.min.dx, " - ");
        LogAppendNum(s, c.max.dx, " ");
    }
    if (c.min.dy == c.max.dy) {
        dbglayoutf("dy: ");
        LogAppendNum(s, c.min.dy, " ");
    } else {
        dbglayoutf("dy: ");
        LogAppendNum(s, c.min.dy, " - ");
        LogAppendNum(s, c.max.dy, " ");
    }
    s.Append(suffix);
    dbglayoutf("%s", s.Get());
}

bool IsCollapsed(ILayout* l) {
    return l->GetVisibility() == Visibility::Collapse;
}

void PositionRB(const Rect& container, Rect& r) {
    r.x = container.dx - r.dx;
    r.y = container.dy - r.dy;
}

void MoveXY(Rect& r, int x, int y) {
    r.x += x;
    r.y += y;
}

int Clamp(int v, int vmin, int vmax) {
    if (v > vmax) {
        return vmax;
    }
    if (v < vmin) {
        return vmin;
    }
    return v;
}

int Scale(int v, i64 num, i64 den) {
    if (den == 0) {
        return 0;
    }
    i64 res = (i64(v) * num) / den;
    return int(res);
}

int GuardInf(int a, int b) {
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

Constraints ExpandHeight(int width) {
    Size min{width, 0};
    Size max{width, Inf};
    return Constraints{min, max};
}

Constraints ExpandWidth(int height) {
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

Constraints TightHeight(int height) {
    Size min{0, height};
    Size max{Inf, height};
    return Constraints{min, max};
}

Size Constraints::Constrain(Size size) const {
    int w = Clamp(size.dx, min.dx, max.dx);
    int h = Clamp(size.dy, min.dy, max.dy);
    return Size{w, h};
}

Size Constraints::ConstrainAndAttemptToPreserveAspectRatio(const Size size) const {
    if (IsTight()) {
        return min;
    }

    int width = size.dx;
    int height = size.dy;

    if (width > max.dx) {
        width = max.dx;
        height = Scale(width, size.dy, size.dx);
    }
    if (height > max.dy) {
        height = max.dy;
        width = Scale(height, size.dx, size.dy);
    }

    if (width < min.dx) {
        width = min.dx;
        height = Scale(width, size.dy, size.dx);
    }

    if (height < min.dy) {
        height = min.dy;
        width = Scale(height, size.dx, size.dy);
    }

    Size c{width, height};
    return Constrain(c);
}

int Constraints::ConstrainHeight(int height) const {
    return Clamp(height, min.dy, max.dy);
}

int Constraints::ConstrainWidth(int width) const {
    return Clamp(width, min.dx, max.dx);
}

bool Constraints::HasBoundedHeight() const {
    return max.dy < Inf;
}

bool Constraints::HasBoundedWidth() const {
    return max.dx < Inf;
}

bool Constraints::HasTightWidth() const {
    return min.dx >= max.dx;
}

bool Constraints::HasTightHeight() const {
    return min.dy >= max.dy;
}

Constraints Constraints::Inset(int width, int height) const {
    int minw = min.dx;
    int deflatedMinWidth = GuardInf(minw, std::max(0, minw - width));
    int minh = min.dy;
    int deflatedMinHeight = GuardInf(minh, std::max(0, minh - height));
    Size min2{deflatedMinWidth, deflatedMinHeight};
    int maxw = max.dx;
    int maxh = max.dy;
    Size max2{
        std::max(deflatedMinWidth, GuardInf(maxw, maxw - width)),
        std::max(deflatedMinHeight, GuardInf(maxh, maxh - height)),
    };
    return Constraints{min2, max2};
}

bool Constraints::IsBounded() const {
    return HasBoundedWidth() && HasBoundedHeight();
}

bool Constraints::IsNormalized() const {
    return min.dx >= 0.0 && min.dx <= max.dx && min.dy >= 0.0 && min.dy <= max.dy;
}

bool Constraints::IsSatisfiedBy(Size size) const {
    return min.dx <= size.dx && size.dx <= max.dx && min.dy <= size.dy && size.dy <= max.dy && size.dx != Inf &&
           size.dy != Inf;
}

bool Constraints::IsTight() const {
    return HasTightWidth() && HasTightHeight();
}

bool Constraints::IsZero() const {
    return min.dx == 0 && min.dy == 0 && max.dx == 0 && max.dy == 0;
}

Constraints Constraints::Loosen() const {
    return Constraints{Size{}, max};
}

Constraints Constraints::LoosenHeight() const {
    return Constraints{Size{min.dx, 0}, max};
}

Constraints Constraints::LoosenWidth() const {
    return Constraints{Size{0, min.dy}, max};
}

Constraints Constraints::Tighten(Size size) const {
    Constraints bc = *this;
    bc.min.dx = Clamp(size.dx, bc.min.dx, bc.max.dx);
    bc.max.dx = bc.min.dx;
    bc.min.dy = Clamp(size.dy, bc.min.dy, bc.max.dy);
    bc.max.dy = bc.min.dy;
    return bc;
}

Constraints Constraints::TightenHeight(int height) const {
    Constraints bc = *this;
    bc.min.dy = Clamp(height, bc.min.dy, bc.max.dy);
    bc.max.dy = bc.min.dy;
    return bc;
}

Constraints Constraints::TightenWidth(int width) const {
    Constraints bc = *this;

    bc.min.dx = Clamp(width, bc.min.dx, bc.max.dx);
    bc.max.dx = bc.min.dx;
    return bc;
}

LayoutBase::LayoutBase(Kind k) {
    kind = k;
}

Kind LayoutBase::GetKind() {
    return kind;
}

void LayoutBase::SetVisibility(Visibility newVisibility) {
    visibility = newVisibility;
}

Visibility LayoutBase::GetVisibility() {
    return visibility;
}
void LayoutBase::SetBounds(Rect bounds) {
    lastBounds = bounds;
}

bool IsLayoutOfKind(ILayout* l, Kind kind) {
    if (l == nullptr) {
        return false;
    }
    return l->GetKind() == kind;
}

// padding.go

Kind paddingKind = "padding";
bool IsPadding(Kind kind) {
    return kind == paddingKind;
}
bool IsPadding(ILayout* l) {
    return IsLayoutOfKind(l, paddingKind);
}

Padding::Padding(ILayout* childIn, const Insets& insetsIn) : insets(insetsIn) {
    kind = paddingKind;
    child = childIn;
}

Padding::~Padding() {
    delete child;
}

Size Padding::Layout(const Constraints bc) {
    dbglayoutf("Padding::Layout() ");
    LogConstraints(bc, "\n");

    auto hinset = insets.left + insets.right;
    auto vinset = insets.top + insets.bottom;

    auto innerConstraints = bc.Inset(hinset, vinset);
    childSize = child->Layout(innerConstraints);
    return Size{
        childSize.dx + hinset,
        childSize.dy + vinset,
    };
}

int Padding::MinIntrinsicHeight(int width) {
    auto vinset = insets.top + insets.bottom;
    return child->MinIntrinsicHeight(width) + vinset;
}

int Padding::MinIntrinsicWidth(int height) {
    auto hinset = insets.left + insets.right;
    return child->MinIntrinsicWidth(height) + hinset;
}

void Padding::SetBounds(Rect bounds) {
    dbglayoutf("Padding:SetBounds() %d,%d - %d, %d\n", bounds.x, bounds.y, bounds.dx, bounds.dy);
    lastBounds = bounds;
    bounds.x += insets.left;
    bounds.y += insets.top;
    bounds.dx -= (insets.right + insets.left);
    bounds.dy -= (insets.bottom + insets.top);
    child->SetBounds(bounds);
}

// vbox.go

Kind kindVBox = "vbox";

VBox::VBox() {
    kind = kindVBox;
}

VBox::~VBox() {
    for (auto& c : children) {
        delete c.layout;
    }
}

int VBox::ChildrenCount() const {
    return children.isize();
}

int VBox::NonCollapsedChildrenCount() {
    int n = 0;
    for (const auto& c : children) {
        if (!IsCollapsed(c.layout)) {
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
        if (!IsCollapsed(i.layout)) {
            totalFlex += i.flex;
        }
    }
    return totalFlex;
}

// TODO: remove calculateVGap() and calculateHGap()
static int CalculateVGap(ILayout*, ILayout*) {
    return 0;
}

static int CalculateHGap(ILayout*, ILayout*) {
    return 0;
}

Size VBox::Layout(const Constraints bc) {
    auto n = ChildrenCount();
    if (n == 0) {
        totalHeight = 0;
        return bc.Constrain(Size{});
    }
    totalFlex = updateFlex(children, alignMain);

    dbglayoutf("VBox::Layout() %d children, %d totalFlex ", n, totalFlex);
    LogConstraints(bc, "\n");

    // Determine the constraints for layout of child elements.
    auto cbc = bc;

    if (alignMain == MainAxisAlign::Homogeneous) {
        auto count = (i64)NonCollapsedChildrenCount();
        auto gap = CalculateVGap(nullptr, nullptr);
        cbc = cbc.TightenHeight(Scale(cbc.max.dy, 1, count) - Scale(gap, count - 1, count));
    } else {
        cbc.min.dy = 0;
        cbc.max.dy = Inf;
    }

    if (alignCross == CrossAxisAlign::Stretch) {
        if (cbc.HasBoundedWidth()) {
            cbc = cbc.TightenWidth(cbc.max.dx);
        } else {
            cbc = cbc.TightenWidth(MinIntrinsicWidth(Inf));
        }
    } else {
        cbc = cbc.LoosenWidth();
    }
    auto height = int(0);
    auto width = int(0);
    ILayout* previous = nullptr;

    for (int i = 0; i < n; i++) {
        auto& v = children.at(i);
        if (IsCollapsed(v.layout)) {
            continue;
        }
        // Determine what gap needs to be inserted between the elements.
        if (i > 0) {
            if (IsPacked(alignMain)) {
                height += CalculateVGap(previous, v.layout);
            } else {
                height += CalculateVGap(nullptr, nullptr);
            }
        }
        previous = v.layout;

        // Perform layout of the element.  Track impact on width and height.
        auto size = v.layout->Layout(cbc);
        v.size = size; // TODO: does that work?
        height += size.dy;
        width = std::max(width, size.dx);
    }
    totalHeight = height;

    // Need to adjust width to any widgets that have flex
    if (totalFlex > 0) {
        auto extraHeight = int(0);
        if (bc.HasBoundedHeight() && bc.max.dy > totalHeight) {
            extraHeight = bc.max.dy - totalHeight;
        } else if (bc.min.dy > totalHeight) {
            extraHeight = bc.min.dy - totalHeight;
        }

        if (extraHeight > 0) {
            for (auto& v : children) {
                if (v.flex > 0) {
                    auto oldHeight = v.size.dy;
                    auto extra = Scale(extraHeight, v.flex, totalFlex);
                    auto fbc = cbc.TightenHeight(v.size.dy + extra);
                    auto size = v.layout->Layout(fbc);
                    v.size = size;
                    totalHeight += size.dy - oldHeight;
                }
            }
        }
    }
    if (alignCross == CrossAxisAlign::Stretch) {
        return bc.Constrain(Size{cbc.min.dx, height});
    }

    return bc.Constrain(Size{width, height});
}

int VBox::MinIntrinsicWidth(int height) {
    auto n = ChildrenCount();
    if (n == 0) {
        return 0;
    }
    if (alignMain == MainAxisAlign::Homogeneous) {
        height = GuardInf(height, Scale(height, 1, i64(n)));
        auto size = children[0].layout->MinIntrinsicWidth(height);
        for (int i = 1; i < n; i++) {
            auto& v = children[i];
            size = std::max(size, v.layout->MinIntrinsicWidth(height));
        }
        return size;
    }
    auto size = children[0].layout->MinIntrinsicWidth(Inf);
    for (int i = 1; i < n; i++) {
        auto& v = children[i];
        size = std::max(size, v.layout->MinIntrinsicWidth(Inf));
    }
    return size;
}

int VBox::MinIntrinsicHeight(int width) {
    auto n = ChildrenCount();
    if (n == 0) {
        return 0;
    }
    auto size = children[0].layout->MinIntrinsicHeight(width);
    if (IsPacked(alignMain)) {
        auto previous = children[0].layout;
        for (int i = 1; i < n; i++) {
            auto& v = children[i];
            // Add the preferred gap between this pair of widgets
            size += CalculateVGap(previous, v.layout);
            previous = v.layout;
            // Find minimum size for this widget, and update
            size += v.layout->MinIntrinsicHeight(width);
        }
        return size;
    }

    if (alignMain == MainAxisAlign::Homogeneous) {
        for (int i = 1; i < n; i++) {
            auto& v = children[i];
            size = std::max(size, v.layout->MinIntrinsicHeight(width));
        }

        // Add a minimum gap between the controls.
        auto vgap = CalculateVGap(nullptr, nullptr);
        size = Scale(size, i64(n), 1) + Scale(vgap, i64(n) - 1, 1);
        return size;
    }

    for (int i = 1; i < n; i++) {
        auto& v = children[i];
        size += v.layout->MinIntrinsicHeight(width);
    }

    // Add a minimum gap between the controls.
    auto vgap = CalculateVGap(nullptr, nullptr);
    if (alignMain == MainAxisAlign::SpaceBetween) {
        size += Scale(vgap, i64(n) - 1, 1);
    } else {
        size += Scale(vgap, i64(n) + 1, 1);
    }

    return size;
}

void VBox::SetBounds(Rect bounds) {
    lastBounds = bounds;

    auto n = ChildrenCount();
    if (n == 0) {
        return;
    }
    dbglayoutf("VBox:SetBounds() %d,%d - %d, %d %d children\n", bounds.x, bounds.y, bounds.dx, bounds.dy, n);

    if (alignMain == MainAxisAlign::Homogeneous) {
        auto gap = CalculateVGap(nullptr, nullptr);
        auto dy = bounds.dy + gap;
        auto count = i64(n);

        for (int i = 0; i < n; i++) {
            auto& v = children[i];
            auto y1 = bounds.y + Scale(dy, i, count);
            auto y2 = bounds.y + Scale(dy, i + 1, count) - gap;
            SetBoundsForChild(i, v.layout, bounds.x, y1, bounds.Right(), y2);
        }
        return;
    }

    // Adjust the bounds so that the minimum Y handles vertical alignment
    // of the controls.  We also calculate 'extraGap' which will adjust
    // spacing of the controls for non-packed alignments.
    auto extraGap = int(0);
    if (totalFlex == 0) {
        switch (alignMain) {
            case MainAxisAlign::MainStart:
                // Do nothing
                break;
            case MainAxisAlign::MainCenter:
                bounds.y += (bounds.dy - totalHeight) / 2;
                break;
            case MainAxisAlign::MainEnd:
                bounds.y = bounds.Bottom() - totalHeight;
                break;
            case MainAxisAlign::SpaceAround: {
                int l = (bounds.dy - totalHeight);
                extraGap = Scale(l, 1, i64(n) + 1);
                bounds.y += extraGap;
                extraGap += CalculateVGap(nullptr, nullptr);
                break;
            }
            case MainAxisAlign::SpaceBetween:
                if (n > 1) {
                    int l = (bounds.dy - totalHeight);
                    extraGap = Scale(l, 1, i64(n) - 1);
                    extraGap += CalculateVGap(nullptr, nullptr);
                } else {
                    // There are no controls between which to put the extra space.
                    // The following essentially convert SpaceBetween to SpaceAround
                    bounds.y += (bounds.dy - totalHeight) / 2;
                }
                break;
        }
    }

    // Position all of the child controls.
    auto posY = bounds.y;
    ILayout* previous = nullptr;
    for (int i = 0; i < n; i++) {
        auto& v = children[i];
        if (IsCollapsed(v.layout)) {
            continue;
        }
        if (IsPacked(alignMain)) {
            if (i > 0) {
                posY += CalculateVGap(previous, v.layout);
            }
            previous = v.layout;
        }

        auto dy = v.size.dy;
        SetBoundsForChild(i, v.layout, bounds.x, posY, bounds.Right(), posY + dy);
        posY += dy + extraGap;
    }
}

void VBox::SetBoundsForChild(int i, ILayout* v, int posX, int posY, int posX2, int posY2) const {
    auto dx = children[i].size.dx;
    Rect r{};
    switch (alignCross) {
        case CrossAxisAlign::CrossStart:
            r = Rect{
                Point{posX, posY},
                Point{posX + dx, posY2},
            };
            break;
        case CrossAxisAlign::CrossCenter:
            r = Rect{
                Point{posX + (posX2 - posX - dx) / 2, posY},
                Point{posX + (posX2 - posX + dx) / 2, posY2},
            };
            break;
        case CrossAxisAlign::CrossEnd:
            r = Rect{
                Point{posX2 - dx, posY},
                Point{posX2, posY2},
            };
            break;
        case CrossAxisAlign::Stretch:
            r = Rect{
                Point{posX, posY},
                Point{posX2, posY2},
            };
            break;
    }
    v->SetBounds(r);
}

boxElementInfo& VBox::AddChild(ILayout* child, int flex) {
    boxElementInfo v{};
    v.layout = child;
    v.flex = flex;
    children.Append(v);
    auto n = children.size();
    return children[n - 1];
}

boxElementInfo& VBox::AddChild(ILayout* child) {
    return AddChild(child, 0);
}

// hbox.go
Kind kindHBox = "hbox";

HBox::~HBox() {
    for (auto& c : children) {
        delete c.layout;
    }
}

int HBox::ChildrenCount() const {
    return children.isize();
}

int HBox::NonCollapsedChildrenCount() {
    int n = 0;
    for (const auto& c : children) {
        if (!IsCollapsed(c.layout)) {
            n++;
        }
    }
    return n;
}

Size HBox::Layout(const Constraints bc) {
    auto n = ChildrenCount();
    if (n == 0) {
        totalWidth = 0;
        return bc.Constrain(Size{});
    }
    totalFlex = updateFlex(children, alignMain);
    dbglayoutf("HBox::Layout() %d children, %d totalFlex ", n, totalFlex);
    LogConstraints(bc, "\n");

    // Determine the constraints for layout of child elements.
    auto cbc = bc;
    if (alignMain == MainAxisAlign::Homogeneous) {
        auto count = (i64)NonCollapsedChildrenCount();
        auto gap = CalculateHGap(nullptr, nullptr);
        auto maxw = cbc.max.dx;
        cbc = cbc.TightenWidth(Scale(maxw, 1, count) - Scale(gap, count - 1, count));
    } else {
        cbc.min.dx = 0;
        cbc.max.dx = Inf;
    }

    if (alignCross == CrossAxisAlign::Stretch) {
        if (cbc.HasBoundedHeight()) {
            cbc = cbc.TightenHeight(cbc.max.dy);
        } else {
            cbc = cbc.TightenHeight(MinIntrinsicHeight(Inf));
        }
    } else {
        cbc = cbc.LoosenHeight();
    }
    auto width = int(0);
    auto height = int(0);
    ILayout* previous = nullptr;

    for (int i = 0; i < n; i++) {
        auto& v = children[i];
        if (IsCollapsed(v.layout)) {
            continue;
        }
        // Determine what gap needs to be inserted between the elements.
        if (i > 0) {
            if (IsPacked(alignMain)) {
                width += CalculateHGap(previous, v.layout);
            } else {
                width += CalculateHGap(nullptr, nullptr);
            }
        }
        previous = v.layout;

        // Perform layout of the element.  Track impact on width and height.
        auto size = v.layout->Layout(cbc);
        v.size = size;
        width += size.dx;
        height = std::max(height, size.dy);
    }
    totalWidth = width;

    // Need to adjust height to any widgets that have flex
    int extraWidth = 0;
    if (totalFlex > 0) {
        if (bc.HasBoundedWidth() && bc.max.dx > totalWidth) {
            extraWidth = bc.max.dx - totalWidth;
        } else if (bc.min.dx > totalWidth) {
            extraWidth = bc.min.dx - totalWidth;
        }
    }
    if (extraWidth > 0) {
        for (int i = 0; i < n; i++) {
            auto& v = children[i];
            if (v.flex <= 0 || IsCollapsed(v.layout)) {
                continue;
            }
            auto oldWidth = v.size.dx;
            auto nw = v.size.dx + extraWidth;
            auto fbc = cbc.TightenWidth(Scale(nw, v.flex, totalFlex));
            auto size = v.layout->Layout(fbc);
            v.size = size;
            totalWidth += size.dx - oldWidth;
        }
    }

    if (alignCross == CrossAxisAlign::Stretch) {
        return bc.Constrain(Size{width, cbc.min.dy});
    }
    return bc.Constrain(Size{width, height});
}

int HBox::MinIntrinsicHeight(int width) {
    auto n = ChildrenCount();
    if (n == 0) {
        return 0;
    }

    if (alignMain == MainAxisAlign::Homogeneous) {
        width = GuardInf(width, Scale(width, 1, i64(n)));
        auto size = children[0].layout->MinIntrinsicHeight(width);
        for (int i = 1; i < n; i++) {
            auto v = children[i];
            size = std::max(size, v.layout->MinIntrinsicHeight(width));
        }
        return size;
    }

    auto size = children[0].layout->MinIntrinsicHeight(Inf);
    for (int i = 1; i < n; i++) {
        auto& v = children[i];
        size = std::max(size, v.layout->MinIntrinsicHeight(Inf));
    }
    return size;
}

int HBox::MinIntrinsicWidth(int height) {
    auto n = ChildrenCount();
    if (n == 0) {
        return 0;
    }

    auto size = children[0].layout->MinIntrinsicWidth(height);
    if (IsPacked(alignMain)) {
        for (int i = 1; i < n; i++) {
            auto& v = children[i];
            // Add the preferred gap between this pair of widgets
            if (IsCollapsed(v.layout)) {
                continue;
            }
            // Find minimum size for this widget, and update
            size += v.layout->MinIntrinsicWidth(height);
        }
        return size;
    }

    if (alignMain == MainAxisAlign::Homogeneous) {
        for (int i = 1; i < n; i++) {
            auto& v = children[i];
            if (IsCollapsed(v.layout)) {
                continue;
            }
            size = std::max(size, v.layout->MinIntrinsicWidth(height));
        }

        // Add a minimum gap between the controls.
        auto hgap = CalculateHGap(nullptr, nullptr);
        size = Scale(size, i64(n), 1) + Scale(hgap, i64(n) - 1, 1);
        return size;
    }

    for (int i = 1; i < n; i++) {
        auto l = children[i].layout;
        if (IsCollapsed(l)) {
            continue;
        }
        size += l->MinIntrinsicWidth(height);
    }

    // Add a minimum gap between the controls.
    auto hgap = CalculateHGap(nullptr, nullptr);
    if (alignMain == MainAxisAlign::SpaceBetween) {
        size += Scale(hgap, i64(n) - 1, 1);
    } else {
        size += Scale(hgap, i64(n) + 1, 1);
    }

    return size;
}

void HBox::SetBounds(Rect bounds) {
    dbglayoutf("HBox:SetBounds() %d,%d - %d, %d\n", bounds.x, bounds.y, bounds.dx, bounds.dy);
    lastBounds = bounds;
    auto n = ChildrenCount();
    if (n == 0) {
        return;
    }

    if (alignMain == MainAxisAlign::Homogeneous) {
        auto gap = CalculateHGap(nullptr, nullptr);
        auto dx = bounds.dx + gap;
        auto count = i64(n);

        for (int i = 0; i < n; i++) {
            auto v = children[i].layout;
            auto x1 = bounds.x + Scale(dx, i, count);
            auto x2 = bounds.x + Scale(dx, i + 1, count) - gap;
            SetBoundsForChild(i, v, x1, bounds.y, x2, bounds.Bottom());
        }
        return;
    }

    // Adjust the bounds so that the minimum Y handles vertical alignment
    // of the controls.  We also calculate 'extraGap' which will adjust
    // spacing of the controls for non-packed alignments.
    auto extraGap = int(0);
    if (totalFlex == 0) {
        switch (alignMain) {
            case MainAxisAlign::MainStart:
                // Do nothing
                break;
            case MainAxisAlign::MainCenter:
                bounds.x += (bounds.dx - totalWidth) / 2;
                break;
            case MainAxisAlign::MainEnd:
                bounds.x = bounds.Right() - totalWidth;
                break;
            case MainAxisAlign::SpaceAround: {
                auto eg = (bounds.dx - totalWidth);
                extraGap = Scale(eg, 1, i64(n) + 1);
                bounds.x += extraGap;
                extraGap += CalculateHGap(nullptr, nullptr);
            } break;
            case MainAxisAlign::SpaceBetween:
                if (n > 1) {
                    auto eg = (bounds.dx - totalWidth);
                    extraGap = Scale(eg, 1, i64(n) - 1);
                    extraGap += CalculateHGap(nullptr, nullptr);
                } else {
                    // There are no controls between which to put the extra space.
                    // The following essentially convert SpaceBetween to SpaceAround
                    bounds.x += (bounds.dx - totalWidth) / 2;
                }
                break;
        }
    }

    // Position all of the child controls.
    auto posX = bounds.x;
    ILayout* previous = nullptr;
    for (int i = 0; i < n; i++) {
        auto& v = children[i];
        if (IsPacked(alignMain)) {
            if (i > 0) {
                posX += CalculateHGap(previous, v.layout);
            }
            previous = v.layout;
        }

        auto dx = children[i].size.dx;
        SetBoundsForChild(i, v.layout, posX, bounds.y, posX + dx, bounds.Bottom());
        posX += dx + extraGap;
    }
}

void HBox::SetBoundsForChild(int i, ILayout* v, int posX, int posY, int posX2, int posY2) const {
    auto dy = children[i].size.dy;
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

boxElementInfo& HBox::AddChild(ILayout* child, int flex) {
    boxElementInfo v{};
    v.layout = child;
    v.flex = flex;
    children.Append(v);
    auto n = children.size();
    return children[n - 1];
}

boxElementInfo& HBox::AddChild(ILayout* child) {
    return AddChild(child, 0);
}

// align.go

Kind kindAlign = "align";

Align::Align(ILayout* c) {
    Child = c;
    kind = kindAlign;
}

Align::~Align() {
    delete Child;
}

Size Align::Layout(const Constraints bc) {
    dbglayoutf("Align::Layout() ");
    LogConstraints(bc, "\n");

    Size size = Child->Layout(bc.Loosen());
    childSize = size;
    auto f = WidthFactor;
    if (f > 0) {
        size.dx = i32(float(size.dx) * f);
    }
    f = HeightFactor;
    if (f > 0) {
        size.dy = i32(float(size.dy) * f);
    }
    return bc.Constrain(size);
}

int Align::MinIntrinsicHeight(int width) {
    int height = Child->MinIntrinsicHeight(width);
    auto f = HeightFactor;
    if (f > 0) {
        return int(float(height) * f);
    }
    return height;
}

int Align::MinIntrinsicWidth(int height) {
    int width = Child->MinIntrinsicHeight(height);
    auto f = WidthFactor;
    if (f > 0) {
        return int(float(width) * f);
    }
    return width;
}

void Align::SetBounds(Rect bounds) {
    dbglayoutf("Align:SetBounds() %d,%d - %d, %d\n", bounds.x, bounds.y, bounds.dx, bounds.dy);

    lastBounds = bounds;
    int bminx = bounds.x;
    int bmaxx = bounds.Right();
    int cw = childSize.dx;
    i64 twm = AlignStart - AlignEnd;
    i64 tw = AlignEnd - AlignStart;
    int x = Scale(bminx, HAlign - AlignEnd, twm) + Scale(bmaxx - cw, HAlign - AlignStart, tw);
    int ch = childSize.dy;
    int bminy = bounds.y;
    int bmaxy = bounds.Bottom();
    int y = Scale(bminy, VAlign - AlignEnd, twm) + Scale(bmaxy - ch, VAlign - AlignStart, tw);
    Rect b{Point{x, y}, Point{x + cw, y + ch}};
    Child->SetBounds(b);
}

void LayoutAndSizeToContent(ILayout* layout, int minDx, int minDy, HWND hwnd) {
    dbglayoutf("\nLayoutAndSizeToContent() %d,%d\n", minDx, minDy);

    Constraints c = ExpandInf();
    c.min = {minDx, minDy};
    auto size = layout->Layout(c);
    Point min{0, 0};
    Point max{size.dx, size.dy};
    Rect bounds{min, max};
    layout->SetBounds(bounds);
    ResizeHwndToClientArea(hwnd, size.dx, size.dy, false);
    InvalidateRect(hwnd, nullptr, false);
}

Size LayoutToSize(ILayout* layout, const Size size) {
    dbglayoutf("\nLayoutToSize() %d,%d\n", size.dx, size.dy);
    auto c = Tight(size);
    auto newSize = layout->Layout(c);
    Rect bounds{0, 0, newSize.dx, newSize.dy};
    layout->SetBounds(bounds);
    return newSize;
}

// TODO: probably not needed
Insets DefaultInsets() {
    const int padding = 8;
    return Insets{padding, padding, padding, padding};
}

Insets DpiScaledInsets(HWND hwnd, int top, int right, int bottom, int left) {
    CrashIf(top < 0);
    if (right == -1) {
        // only first given, consider all to be the same
        right = top;
        bottom = top;
        left = top;
    }
    if (bottom == -1) {
        // first 2 given, consider them top / bottom, left / right
        bottom = top;
        left = right;
    }
    CrashIf(left == -1);
    Insets res = {DpiScale(hwnd, top), DpiScale(hwnd, right), DpiScale(hwnd, bottom), DpiScale(hwnd, left)};
    return res;
}

Kind kindSpacer = "spacer";

Spacer::Spacer(int dx, int dy) {
    kind = kindSpacer;
    this->dx = dx;
    this->dy = dy;
}

Spacer::~Spacer() {
    // do nothing
}

Size Spacer::Layout(const Constraints bc) {
    // do nothing
    return bc.Constrain({dx, dy});
}

int Spacer::MinIntrinsicHeight(int width) {
    return dy;
}
int Spacer::MinIntrinsicWidth(int height) {
    return dx;
}
void Spacer::SetBounds(Rect) {
    // do nothing
}

Kind kindTableLayout = "tableLayout";

TableLayout::TableLayout() {
    kind = kindTableLayout;
}

TableLayout::~TableLayout() {
    int n = rows * cols;
    for (int i = 0; i < n; i++) {
        auto child = cells[i].child;
        delete child;
    }
    free(cells);
    free(maxColWidths);
}

Size TableLayout::Layout(Constraints bc) {
    // TODO: implement me
    CrashMe();
    return {};
}

int TableLayout::MinIntrinsicHeight(int width) {
    // calc max height of each row, min height is sum of those
    int minHeight = 0;
    for (int row = 0; row < rows; row++) {
        int maxRowHeight = 0;
        for (int col = 0; col < cols; col++) {
            ILayout* el = GetCell(row, col);
            if (!el || IsCollapsed(el)) {
                continue;
            }
            // TODO: width should probably be different for each cell
            // i.e. if it's non-infinite then use width / cols or some
            // more complicated scheme for non-uniformly sized columns
            // or maybe not
            int h = el->MinIntrinsicHeight(width);
            if (h > maxRowHeight) {
                maxRowHeight = h;
            }
        }
        minHeight += maxRowHeight;
    }
    return minHeight;
}

int TableLayout::MinIntrinsicWidth(int height) {
    // calc max width of each column, min width is sum of those
    int minWidth = 0;
    for (int col = 0; col < cols; col++) {
        int maxColWidth = 0;
        for (int row = 0; row < rows; row++) {
            ILayout* el = GetCell(row, col);
            if (!el || IsCollapsed(el)) {
                continue;
            }
            // TODO: height should probably be different for each cell
            // i.e. if it's non-infinite then use height / rows or some
            // more complicated scheme for non-uniformly sized rows
            // or maybe not
            int h = el->MinIntrinsicWidth(height);
            if (h > maxColWidth) {
                maxColWidth = h;
            }
        }
        minWidth += maxColWidth;
    }
    return minWidth;
}

void TableLayout::SetBounds(Rect) {
    // TODO: implement me
    CrashMe();
}

void TableLayout::SetSize(int rows, int cols) {
    CrashIf(cells);     // TODO: maybe allow re-sizing
    CrashIf(rows <= 0); // TODO: maybe allow empty
    CrashIf(cols <= 0); // TODO: maybe allow empty
    int n = rows * cols;
    cells = AllocArray<Cell>(n);
    maxColWidths = AllocArray<int>(cols);
}

int TableLayout::CellIdx(int row, int col) {
    CrashIf(!cells);
    CrashIf(row < 0 || row >= rows);
    CrashIf(col < 0 || col >= cols);
    int idx = col * cols + row;
    return idx;
}

void TableLayout::SetCell(int row, int col, ILayout* child) {
    int idx = CellIdx(row, col);
    auto& cell = cells[idx];
    if (cell.child) {
        // delete existing child
        delete cell.child;
    }
    cell.child = child;
}

ILayout* TableLayout::GetCell(int row, int col) {
    int idx = CellIdx(row, col);
    auto child = cells[idx].child;
    return child;
}
