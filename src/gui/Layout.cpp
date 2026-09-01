/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "gui/Dpi.h"

#if IS_DEBUG
#include "base/UtAssert.h"
#endif
#include "gui/Layout.h"

static bool gEnableDebugLayout = false;

void dbglayout(Str s) {
    if (!gEnableDebugLayout) {
        return;
    }
    log(s);
}

static void LogAppendNum(str::Builder& s, int n, Str suffix) {
    if (n == Inf) {
        s.Append(StrL("Inf"));
    } else {
        s.Append(fmt("%d", n));
    }
    if (suffix) {
        s.Append(suffix);
    }
}

void LogConstraints(Constraints c, Str suffix) {
    // Debug-only; "dx: Inf - Inf dy: Inf - Inf <suffix>" is tiny.
    char sScratch[128]{};
    str::Builder s;
    str::BuilderUseExternalBuffer(s, Str(sScratch, sizeofi(sScratch)));
    if (c.min.dx == c.max.dx) {
        dbglayout(StrL("dx: "));
        LogAppendNum(s, c.min.dx, StrL(" "));
    } else {
        dbglayout(StrL("dx: "));
        LogAppendNum(s, c.min.dx, StrL(" - "));
        LogAppendNum(s, c.max.dx, StrL(" "));
    }
    if (c.min.dy == c.max.dy) {
        dbglayout(StrL("dy: "));
        LogAppendNum(s, c.min.dy, StrL(" "));
    } else {
        dbglayout(StrL("dy: "));
        LogAppendNum(s, c.min.dy, StrL(" - "));
        LogAppendNum(s, c.max.dy, StrL(" "));
    }
    s.Append(suffix);
    dbglayout(fmt("%s", ToStr(s)));
}

bool IsCollapsed(ILayout* l) {
    return l->GetVisibility() == Visibility::Collapse;
}

// A layout tree is a tree, not a graph: adding a child that (directly or
// through wrappers) is the parent again makes every measuring pass recurse
// until the stack is gone - e.g. box->AddChild(new Padding(box, ...)) instead
// of wrapping the child that needs the padding
// returned when a child is refused, so callers can still tweak "the element"
static boxElementInfo gRefusedBoxElement;

static bool LayoutTreeContains(ILayout* l, ILayout* needle) {
    if (!l) {
        return false;
    }
    if (l == needle) {
        return true;
    }
    int n = l->LayoutChildCount();
    for (int i = 0; i < n; i++) {
        if (LayoutTreeContains(l->LayoutChildAt(i), needle)) {
            return true;
        }
    }
    return false;
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

void ILayout::SetVisibility(Visibility newVisibility) {
    visibility = newVisibility;
}

Visibility ILayout::GetVisibility() {
    return visibility;
}

void ILayout::SetBounds(Rect bounds) {
    lastBounds = bounds;
}

bool IsLayoutOfKind(ILayout* l, Kind kind) {
    if (l == nullptr) {
        return false;
    }
    return l->GetKind() == kind;
}

// padding.go

static Kind paddingKind = "padding";
bool IsPadding(Kind kind) {
    return kind == paddingKind;
}
bool IsPadding(ILayout* l) {
    return IsLayoutOfKind(l, paddingKind);
}

int Padding::LayoutChildCount() {
    return child ? 1 : 0;
}

ILayout* Padding::LayoutChildAt(int) {
    return child;
}

Padding::Padding(ILayout* childIn, const Insets& insetsIn) : insets(insetsIn) {
    kind = paddingKind;
    child = childIn;
}

Padding::~Padding() {
    delete child;
}

// ILayout
Size Padding::Layout(const Constraints bc) {
    dbglayout(StrL("Padding::Layout() "));
    LogConstraints(bc, StrL("\n"));

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
    dbglayout(fmt("Padding:SetBounds() %d,%d - %d, %d\n", bounds.x, bounds.y, bounds.dx, bounds.dy));
    lastBounds = bounds;
    bounds.x += insets.left;
    bounds.y += insets.top;
    bounds.dx -= (insets.right + insets.left);
    bounds.dy -= (insets.bottom + insets.top);
    child->SetBounds(bounds);
}

// vbox.go

static Kind kindVBox = "vbox";

VBox::VBox() {
    kind = kindVBox;
}

VBox::~VBox() {
    for (auto& c : children) {
        delete c.layout;
    }
}

int VBox::LayoutChildCount() {
    return len(children);
}

ILayout* VBox::LayoutChildAt(int idx) {
    return children[idx].layout;
}

int VBox::ChildrenCount() const {
    return len(children);
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

static int updateFlex(Vec<boxElementInfo>& children, MainAxisAlign alignMain) {
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

// where a child of size sz sits inside its cell
static Rect AlignInCell(const Rect& cell, Size sz, CrossAxisAlign alignH, CrossAxisAlign alignV) {
    Rect r{cell.x, cell.y, sz.dx, sz.dy};
    switch (alignH) {
        case CrossAxisAlign::Stretch:
            r.dx = cell.dx;
            break;
        case CrossAxisAlign::CrossCenter:
            r.x += (cell.dx - sz.dx) / 2;
            break;
        case CrossAxisAlign::CrossEnd:
            r.x += cell.dx - sz.dx;
            break;
        case CrossAxisAlign::CrossStart:
            break;
    }
    switch (alignV) {
        case CrossAxisAlign::Stretch:
            r.dy = cell.dy;
            break;
        case CrossAxisAlign::CrossCenter:
            r.y += (cell.dy - sz.dy) / 2;
            break;
        case CrossAxisAlign::CrossEnd:
            r.y += cell.dy - sz.dy;
            break;
        case CrossAxisAlign::CrossStart:
            break;
    }
    return r;
}

// ILayout
Size VBox::Layout(const Constraints bc) {
    auto n = ChildrenCount();
    int count = NonCollapsedChildrenCount();
    if (count == 0) {
        totalHeight = 0;
        return bc.Constrain(Size{});
    }
    totalFlex = updateFlex(children, alignMain);

    dbglayout(fmt("VBox::Layout() %d children, %d totalFlex ", n, totalFlex));
    LogConstraints(bc, StrL("\n"));

    // Determine the constraints for layout of child elements.
    auto cbc = bc;

    if (alignMain == MainAxisAlign::Homogeneous) {
        int childDy = Scale(cbc.max.dy, 1, count) - Scale(gap, count - 1, count);
        cbc = cbc.TightenHeight(std::max(childDy, 0));
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
    auto height = 0;
    auto width = 0;
    ILayout* previous = nullptr;

    for (int i = 0; i < n; i++) {
        auto& v = children[i];
        if (IsCollapsed(v.layout)) {
            continue;
        }
        if (previous) {
            height += gap;
        }
        previous = v.layout;

        // Perform layout of the element.  Track impact on width and height.
        auto size = v.layout->Layout(cbc);
        v.size = size;
        height += size.dy;
        width = std::max(width, size.dx);
    }
    totalHeight = height;

    // Need to adjust width to any widgets that have flex
    if (totalFlex > 0) {
        auto extraHeight = 0;
        if (bc.HasBoundedHeight() && bc.max.dy > totalHeight) {
            extraHeight = bc.max.dy - totalHeight;
        } else if (bc.min.dy > totalHeight) {
            extraHeight = bc.min.dy - totalHeight;
        }

        if (extraHeight > 0) {
            for (auto& v : children) {
                // collapsed children are excluded from totalFlex, so they must
                // not receive any of the extra height either
                if (v.flex > 0 && !IsCollapsed(v.layout)) {
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

    // Content overflows the available height: shrink flex children so that
    // fixed-size siblings aren't pushed past the edge and clipped. Symmetric
    // to the grow case above; non-flex children keep their size, and
    // WindowBase::Layout clamps each flex child to the tightened height.
    if (totalFlex > 0 && bc.HasBoundedHeight() && totalHeight > bc.max.dy) {
        int deficit = totalHeight - bc.max.dy;
        for (auto& v : children) {
            if (v.flex <= 0 || IsCollapsed(v.layout)) {
                continue;
            }
            int oldHeight = v.size.dy;
            int nh = std::max(oldHeight - Scale(deficit, v.flex, totalFlex), 0);
            auto fbc = cbc.TightenHeight(nh);
            auto size = v.layout->Layout(fbc);
            v.size = size;
            totalHeight += size.dy - oldHeight;
            height += size.dy - oldHeight;
        }
    }

    if (alignCross == CrossAxisAlign::Stretch) {
        return bc.Constrain(Size{cbc.min.dx, height});
    }

    return bc.Constrain(Size{width, height});
}

int VBox::MinIntrinsicWidth(int height) {
    int count = NonCollapsedChildrenCount();
    if (count == 0) {
        return 0;
    }
    if (alignMain == MainAxisAlign::Homogeneous) {
        int childDy = Scale(height, 1, count) - Scale(gap, count - 1, count);
        height = GuardInf(height, std::max(childDy, 0));
    } else {
        height = Inf;
    }
    int size = 0;
    for (auto& v : children) {
        if (!IsCollapsed(v.layout)) {
            size = std::max(size, v.layout->MinIntrinsicWidth(height));
        }
    }
    return size;
}

int VBox::MinIntrinsicHeight(int width) {
    int count = NonCollapsedChildrenCount();
    if (count == 0) {
        return 0;
    }
    int size = 0;
    if (alignMain == MainAxisAlign::Homogeneous) {
        for (auto& v : children) {
            if (!IsCollapsed(v.layout)) {
                size = std::max(size, v.layout->MinIntrinsicHeight(width));
            }
        }
    } else {
        for (auto& v : children) {
            if (!IsCollapsed(v.layout)) {
                size += v.layout->MinIntrinsicHeight(width);
            }
        }
    }
    return size + (gap * (count - 1));
}

void VBox::SetBounds(Rect bounds) {
    lastBounds = bounds;

    auto n = ChildrenCount();
    int count = NonCollapsedChildrenCount();
    if (count == 0) {
        return;
    }
    dbglayout(fmt("VBox:SetBounds() %d,%d - %d, %d %d children\n", bounds.x, bounds.y, bounds.dx, bounds.dy, n));

    if (alignMain == MainAxisAlign::Homogeneous) {
        auto dy = bounds.dy + gap;
        int visibleIdx = 0;
        for (int i = 0; i < n; i++) {
            auto& v = children[i];
            if (IsCollapsed(v.layout)) {
                continue;
            }
            auto y1 = bounds.y + Scale(dy, visibleIdx, count);
            auto y2 = bounds.y + Scale(dy, visibleIdx + 1, count) - gap;
            SetBoundsForChild(i, v.layout, bounds.x, y1, bounds.Right(), y2);
            visibleIdx++;
        }
        return;
    }

    // Adjust the bounds for main-axis alignment and add any distributed free
    // space to the requested inter-child gap.
    auto between = gap;
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
                int extra = Scale(l, 1, i64(count) + 1);
                bounds.y += extra;
                between += extra;
                break;
            }
            case MainAxisAlign::SpaceBetween:
                if (count > 1) {
                    int l = (bounds.dy - totalHeight);
                    between += Scale(l, 1, i64(count) - 1);
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
        if (previous) {
            posY += IsPacked(alignMain) ? gap : between;
        }
        previous = v.layout;

        auto dy = v.size.dy;
        SetBoundsForChild(i, v.layout, bounds.x, posY, bounds.Right(), posY + dy);
        posY += dy;
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
                Point{posX + ((posX2 - posX - dx) / 2), posY},
                Point{posX + ((posX2 - posX + dx) / 2), posY2},
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
    if (LayoutTreeContains(child, this)) {
        ReportIf(true);
        return gRefusedBoxElement;
    }
    boxElementInfo v{};
    v.layout = child;
    v.flex = flex;
    VecAppend(children, v);
    auto n = len(children);
    return children[n - 1];
}

boxElementInfo& VBox::AddChild(ILayout* child) {
    return AddChild(child, 0);
}

// hbox.go
static Kind kindHBox = "hbox";

HBox::HBox() {
    kind = kindHBox;
}

int HBox::LayoutChildCount() {
    return len(children);
}

ILayout* HBox::LayoutChildAt(int idx) {
    return children[idx].layout;
}

HBox::~HBox() {
    for (auto& c : children) {
        delete c.layout;
    }
}

int HBox::ChildrenCount() const {
    return len(children);
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
    int count = NonCollapsedChildrenCount();
    if (count == 0) {
        totalWidth = 0;
        return bc.Constrain(Size{});
    }
    totalFlex = updateFlex(children, alignMain);
    dbglayout(fmt("HBox::Layout() %d children, %d totalFlex ", n, totalFlex));
    LogConstraints(bc, StrL("\n"));

    // Determine the constraints for layout of child elements.
    auto cbc = bc;
    if (alignMain == MainAxisAlign::Homogeneous) {
        auto maxw = cbc.max.dx;
        int childDx = Scale(maxw, 1, count) - Scale(gap, count - 1, count);
        cbc = cbc.TightenWidth(std::max(childDx, 0));
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
    auto width = 0;
    auto height = 0;
    ILayout* previous = nullptr;

    for (int i = 0; i < n; i++) {
        auto& v = children[i];
        if (IsCollapsed(v.layout)) {
            continue;
        }
        if (previous) {
            width += gap;
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
            // each flex child keeps its natural width plus its share of the extra
            // (matching VBox); Scale(oldWidth + extraWidth, ...) under-fills when
            // flex children have non-zero natural widths
            auto extra = Scale(extraWidth, v.flex, totalFlex);
            auto fbc = cbc.TightenWidth(v.size.dx + extra);
            auto size = v.layout->Layout(fbc);
            v.size = size;
            totalWidth += size.dx - oldWidth;
        }
    }

    // Content overflows the available width: shrink flex children so that
    // fixed-size siblings (e.g. a browse button) aren't pushed past the edge
    // and clipped. Symmetric to the grow case above; non-flex children keep
    // their size, and WindowBase::Layout clamps each flex child to the tightened width.
    if (totalFlex > 0 && bc.HasBoundedWidth() && totalWidth > bc.max.dx) {
        int deficit = totalWidth - bc.max.dx;
        for (int i = 0; i < n; i++) {
            auto& v = children[i];
            if (v.flex <= 0 || IsCollapsed(v.layout)) {
                continue;
            }
            int oldWidth = v.size.dx;
            int nw = std::max(oldWidth - Scale(deficit, v.flex, totalFlex), 0);
            auto fbc = cbc.TightenWidth(nw);
            auto size = v.layout->Layout(fbc);
            v.size = size;
            totalWidth += size.dx - oldWidth;
            width += size.dx - oldWidth;
        }
    }

    if (alignCross == CrossAxisAlign::Stretch) {
        return bc.Constrain(Size{width, cbc.min.dy});
    }
    return bc.Constrain(Size{width, height});
}

int HBox::MinIntrinsicHeight(int width) {
    int count = NonCollapsedChildrenCount();
    if (count == 0) {
        return 0;
    }

    if (alignMain == MainAxisAlign::Homogeneous) {
        int childDx = Scale(width, 1, count) - Scale(gap, count - 1, count);
        width = GuardInf(width, std::max(childDx, 0));
    } else {
        width = Inf;
    }
    int size = 0;
    for (auto& v : children) {
        if (!IsCollapsed(v.layout)) {
            size = std::max(size, v.layout->MinIntrinsicHeight(width));
        }
    }
    return size;
}

int HBox::MinIntrinsicWidth(int height) {
    int count = NonCollapsedChildrenCount();
    if (count == 0) {
        return 0;
    }

    int size = 0;
    if (alignMain == MainAxisAlign::Homogeneous) {
        for (auto& v : children) {
            if (IsCollapsed(v.layout)) {
                continue;
            }
            size = std::max(size, v.layout->MinIntrinsicWidth(height));
        }
        size *= count;
    } else {
        for (auto& v : children) {
            if (!IsCollapsed(v.layout)) {
                size += v.layout->MinIntrinsicWidth(height);
            }
        }
    }
    return size + (gap * (count - 1));
}

// mirror a child's x against the original HBox so MainStart packs to the right
static int MirrorX(const Rect& box, int x, int dx) {
    return box.x + box.dx - (x - box.x) - dx;
}

void HBox::SetBounds(Rect bounds) {
    dbglayout(fmt("HBox:SetBounds() %d,%d - %d, %d\n", bounds.x, bounds.y, bounds.dx, bounds.dy));
    lastBounds = bounds;
    Rect box = bounds;
    auto n = ChildrenCount();
    int count = NonCollapsedChildrenCount();
    if (count == 0) {
        return;
    }

    if (alignMain == MainAxisAlign::Homogeneous) {
        auto dx = bounds.dx + gap;
        int visibleIdx = 0;
        for (int i = 0; i < n; i++) {
            auto* v = children[i].layout;
            if (IsCollapsed(v)) {
                continue;
            }
            auto x1 = bounds.x + Scale(dx, visibleIdx, count);
            auto x2 = bounds.x + Scale(dx, visibleIdx + 1, count) - gap;
            if (rtl) {
                int w = x2 - x1;
                x1 = MirrorX(box, x1, w);
                x2 = x1 + w;
            }
            SetBoundsForChild(i, v, x1, bounds.y, x2, bounds.Bottom());
            visibleIdx++;
        }
        return;
    }

    // Adjust the bounds for main-axis alignment and add any distributed free
    // space to the requested inter-child gap.
    auto between = gap;
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
                int extra = Scale(eg, 1, i64(count) + 1);
                bounds.x += extra;
                between += extra;
            } break;
            case MainAxisAlign::SpaceBetween:
                if (count > 1) {
                    auto eg = (bounds.dx - totalWidth);
                    between += Scale(eg, 1, i64(count) - 1);
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
        // Layout() skipped it, so v.size is stale: position it and posX would
        // both be wrong (VBox::SetBounds has always done this)
        if (IsCollapsed(v.layout)) {
            continue;
        }
        if (previous) {
            posX += IsPacked(alignMain) ? gap : between;
        }
        previous = v.layout;

        auto dx = children[i].size.dx;
        int x1 = posX;
        int x2 = posX + dx;
        if (rtl) {
            x1 = MirrorX(box, posX, dx);
            x2 = x1 + dx;
        }
        SetBoundsForChild(i, v.layout, x1, bounds.y, x2, bounds.Bottom());
        posX += dx;
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
                Point{posX, posY + ((posY2 - posY - dy) / 2)},
                Point{posX2, posY + ((posY2 - posY + dy) / 2)},
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
    if (LayoutTreeContains(child, this)) {
        ReportIf(true);
        return gRefusedBoxElement;
    }
    boxElementInfo v{};
    v.layout = child;
    v.flex = flex;
    VecAppend(children, v);
    auto n = len(children);
    return children[n - 1];
}

boxElementInfo& HBox::AddChild(ILayout* child) {
    return AddChild(child, 0);
}

// align.go

static Kind kindAlign = "align";

Align::Align(ILayout* c) {
    Child = c;
    kind = kindAlign;
}

int Align::LayoutChildCount() {
    return Child ? 1 : 0;
}

ILayout* Align::LayoutChildAt(int) {
    return Child;
}

Align::~Align() {
    delete Child;
}

// ILayout
Size Align::Layout(const Constraints bc) {
    dbglayout(StrL("Align::Layout() "));
    LogConstraints(bc, StrL("\n"));

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
    int width = Child->MinIntrinsicWidth(height);
    auto f = WidthFactor;
    if (f > 0) {
        return int(float(width) * f);
    }
    return width;
}

void Align::SetBounds(Rect bounds) {
    dbglayout(fmt("Align:SetBounds() %d,%d - %d, %d\n", bounds.x, bounds.y, bounds.dx, bounds.dy));

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

//--- Table

static Kind kindTable = "table";

Table::Table() {
    kind = kindTable;
}

// owns every cell's child
Table::~Table() {
    RemoveAllCells();
}

int Table::CellIdx(int row, int col) const {
    ReportIf(row < 0 || row >= rows);
    ReportIf(col < 0 || col >= cols);
    return (row * cols) + col;
}

void Table::SetSize(int nRows, int nCols) {
    ReportIf(nRows < 0 || nCols < 0);
    if (nRows == rows && nCols == cols) {
        return;
    }
    RemoveAllCells();
    rows = nRows;
    cols = nCols;
    VecClear(cells);
    TableCell empty;
    for (int i = 0; i < rows * cols; i++) {
        VecAppend(cells, empty);
    }
    VecClear(colWidths);
    VecClear(rowHeights);
}

void Table::MarkCovered(int row, int col, int rowSpan, int colSpan, bool covered) {
    for (int r = row; r < row + rowSpan; r++) {
        for (int c = col; c < col + colSpan; c++) {
            if (r == row && c == col) {
                continue;
            }
            TableCell& cell = cells[CellIdx(r, c)];
            // a spanned-over cell can't hold a child of its own
            ReportIf(covered && cell.child);
            cell.covered = covered;
        }
    }
}

// (row, col) is the cell's top-left; a spanning cell covers the ones to its
// right / below, which must stay empty
TableCell& Table::SetCell(int row, int col, ILayout* child, int rowSpan, int colSpan) {
    ReportIf(rowSpan < 1 || colSpan < 1);
    ReportIf(row + rowSpan > rows || col + colSpan > cols);
    TableCell& cell = cells[CellIdx(row, col)];
    ReportIf(cell.covered);
    MarkCovered(row, col, cell.rowSpan, cell.colSpan, false);
    if (cell.child && cell.child != child) {
        delete cell.child;
    }
    cell.child = child;
    cell.rowSpan = rowSpan;
    cell.colSpan = colSpan;
    MarkCovered(row, col, rowSpan, colSpan, true);
    return cell;
}

TableCell* Table::CellAt(int row, int col) {
    if (row < 0 || row >= rows || col < 0 || col >= cols) {
        return nullptr;
    }
    return &cells[CellIdx(row, col)];
}

ILayout* Table::GetCell(int row, int col) {
    TableCell* cell = CellAt(row, col);
    return cell ? cell->child : nullptr;
}

void Table::RemoveAllCells() {
    for (int i = 0; i < len(cells); i++) {
        delete cells[i].child;
        cells[i] = TableCell{};
    }
}

int Table::LayoutChildCount() {
    int n = 0;
    for (const TableCell& c : cells) {
        if (c.child) {
            n++;
        }
    }
    return n;
}

ILayout* Table::LayoutChildAt(int idx) {
    int i = 0;
    for (TableCell& c : cells) {
        if (!c.child) {
            continue;
        }
        if (i == idx) {
            return c.child;
        }
        i++;
    }
    return nullptr;
}

// a cell spanning several tracks needs those tracks (plus the gaps between
// them) to be at least as big as the cell; grow them evenly if they aren't
static void GrowTracks(Vec<int>& tracks, int start, int span, int gap, int needed) {
    int have = gap * (span - 1);
    for (int i = start; i < start + span; i++) {
        have += tracks[i];
    }
    int missing = needed - have;
    if (missing <= 0) {
        return;
    }
    for (int i = 0; i < span; i++) {
        // the last track gets what rounding left over
        int add = missing / (span - i);
        tracks[start + i] += add;
        missing -= add;
    }
}

static int TracksSize(Vec<int>& tracks, int start, int span, int gap) {
    if (span < 1) {
        return 0;
    }
    int size = 0;
    int n = 0;
    for (int i = start; i < start + span; i++) {
        if (tracks[i] <= 0) {
            continue;
        }
        if (n > 0) {
            size += gap;
        }
        size += tracks[i];
        n++;
    }
    return size;
}

// where track idx starts, relative to the first track
static int TracksStart(Vec<int>& tracks, int idx, int gap) {
    int pos = 0;
    int n = 0;
    for (int i = 0; i < idx; i++) {
        if (tracks[i] <= 0) {
            continue;
        }
        if (n > 0) {
            pos += gap;
        }
        pos += tracks[i];
        n++;
    }
    if (n > 0 && idx < len(tracks) && tracks[idx] > 0) {
        pos += gap;
    }
    return pos;
}

// a column is as wide as its widest cell, a row as tall as its tallest. Cells
// that span several tracks are applied afterwards, so they only stretch the
// tracks they span when what those already give them isn't enough
void Table::Measure() {
    VecClear(colWidths);
    VecAppendBlanks(colWidths, cols);
    VecClear(rowHeights);
    VecAppendBlanks(rowHeights, rows);

    Constraints loose = ExpandInf();
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            TableCell& cell = cells[CellIdx(row, col)];
            cell.childSize = {0, 0};
            if (!cell.child || IsCollapsed(cell.child)) {
                continue;
            }
            cell.childSize = cell.child->Layout(loose);
            if (cell.colSpan == 1) {
                colWidths[col] = std::max(colWidths[col], cell.childSize.dx);
            }
            if (cell.rowSpan == 1) {
                rowHeights[row] = std::max(rowHeights[row], cell.childSize.dy);
            }
        }
    }

    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            TableCell& cell = cells[CellIdx(row, col)];
            if (!cell.child || IsCollapsed(cell.child)) {
                continue;
            }
            if (cell.colSpan > 1) {
                GrowTracks(colWidths, col, cell.colSpan, colGap, cell.childSize.dx);
            }
            if (cell.rowSpan > 1) {
                GrowTracks(rowHeights, row, cell.rowSpan, rowGap, cell.childSize.dy);
            }
        }
    }
}

Size Table::TotalSize() {
    int dx = padding.left + padding.right + TracksSize(colWidths, 0, cols, colGap);
    int dy = padding.top + padding.bottom + TracksSize(rowHeights, 0, rows, rowGap);
    return {dx, dy};
}

int Table::MinIntrinsicHeight(int) {
    Measure();
    return TotalSize().dy;
}

int Table::MinIntrinsicWidth(int) {
    Measure();
    return TotalSize().dx;
}

Size Table::Layout(Constraints bc) {
    Measure();
    return bc.Constrain(TotalSize());
}

int Table::ColWidth(int col) {
    if (col < 0 || col >= len(colWidths)) {
        return 0;
    }
    return colWidths[col];
}

int Table::RowHeight(int row) {
    if (row < 0 || row >= len(rowHeights)) {
        return 0;
    }
    return rowHeights[row];
}

Rect Table::ContentRect() {
    Rect r = lastBounds;
    r.SubTB(padding.top, padding.bottom);
    r.SubLR(padding.left, padding.right);
    return r;
}

// in the same coords SetBounds() was given
Rect Table::CellRect(int row, int col) {
    TableCell* cell = CellAt(row, col);
    if (!cell || len(colWidths) != cols || len(rowHeights) != rows) {
        return {};
    }
    Rect content = ContentRect();
    int x = content.x + TracksStart(colWidths, col, colGap);
    int y = content.y + TracksStart(rowHeights, row, rowGap);
    int dx = TracksSize(colWidths, col, cell->colSpan, colGap);
    int dy = TracksSize(rowHeights, row, cell->rowSpan, rowGap);
    return {x, y, dx, dy};
}

void Table::SetBounds(Rect r) {
    lastBounds = r;
    if (len(colWidths) != cols || len(rowHeights) != rows) {
        // SetBounds() without a preceding Layout()
        Measure();
    }
    int extraDx = ContentRect().dx - TracksSize(colWidths, 0, cols, colGap);
    if (extraDx > 0 && cols > 0) {
        colWidths[cols - 1] += extraDx;
    }
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            TableCell& cell = cells[CellIdx(row, col)];
            if (!cell.child || IsCollapsed(cell.child)) {
                continue;
            }
            Rect cellRc = CellRect(row, col);
            cell.child->SetBounds(AlignInCell(cellRc, cell.childSize, cell.alignH, cell.alignV));
        }
    }
}

Size LayoutToSize(ILayout* layout, const Size size) {
    dbglayout(fmt("\nLayoutToSize() %d,%d\n", size.dx, size.dy));
    auto c = Tight(size);
    auto newSize = layout->Layout(c);
    Rect bounds{0, 0, newSize.dx, newSize.dy};
    layout->SetBounds(bounds);
    return newSize;
}

Insets DefaultInsets() {
    const int padding = 8;
    return Insets{padding, padding, padding, padding};
}

// DpiScaledInsets has 1-, 2-, and 4-value overloads only (no 3-value form) so a
// mistaken top/right/bottom call without left fails at compile time.
static Insets DpiScaledInsetsAll(int top, int right, int bottom, int left) {
    ReportIf(top < 0);
    ReportIf(right < 0);
    ReportIf(bottom < 0);
    ReportIf(left < 0);
    return {DpiScale(top), DpiScale(right), DpiScale(bottom), DpiScale(left)};
}

Insets DpiScaledInsets(int uniform) {
    return DpiScaledInsetsAll(uniform, uniform, uniform, uniform);
}

Insets DpiScaledInsets(int topBottom, int leftRight) {
    return DpiScaledInsetsAll(topBottom, leftRight, topBottom, leftRight);
}

Insets DpiScaledInsets(int top, int right, int bottom, int left) {
    return DpiScaledInsetsAll(top, right, bottom, left);
}

static Kind kindSpacer = "spacer";

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

int Spacer::MinIntrinsicHeight(int /*width*/) {
    return dy;
}
int Spacer::MinIntrinsicWidth(int /*height*/) {
    return dx;
}
void Spacer::SetBounds(Rect bounds) {
    // a Spacer paints nothing, but callers that use it as a layout slot (e.g.
    // the AI chat panel positions its lazily-created webview into the slot) read
    // lastBounds, so record it
    lastBounds = bounds;
}

//--- Overlay

static Kind kindOverlay = "overlay";

Overlay::Overlay() {
    kind = kindOverlay;
}

Overlay::~Overlay() {
    for (auto& c : children) {
        delete c.child;
    }
}

int Overlay::LayoutChildCount() {
    return len(children);
}

ILayout* Overlay::LayoutChildAt(int idx) {
    return children[idx].child;
}

int Overlay::ChildrenCount() const {
    return len(children);
}

static OverlayChild gRefusedOverlayChild;

OverlayChild& Overlay::AddChild(ILayout* child, CrossAxisAlign alignH, CrossAxisAlign alignV) {
    if (LayoutTreeContains(child, this)) {
        ReportIf(true);
        return gRefusedOverlayChild;
    }
    OverlayChild v{};
    v.child = child;
    v.alignH = alignH;
    v.alignV = alignV;
    VecAppend(children, v);
    return children[len(children) - 1];
}

OverlayChild& Overlay::AddChild(ILayout* child) {
    return AddChild(child, CrossAxisAlign::Stretch, CrossAxisAlign::Stretch);
}

// each child is measured loosely; the overlay is as large as the largest one
Size Overlay::Layout(const Constraints bc) {
    Size maxSz{};
    Constraints loose = bc.Loosen();
    for (auto& c : children) {
        if (!c.child || IsCollapsed(c.child)) {
            c.size = {};
            continue;
        }
        c.size = c.child->Layout(loose);
        maxSz.dx = std::max(maxSz.dx, c.size.dx);
        maxSz.dy = std::max(maxSz.dy, c.size.dy);
    }
    return bc.Constrain(maxSz);
}

int Overlay::MinIntrinsicHeight(int width) {
    int h = 0;
    for (auto& c : children) {
        if (c.child && !IsCollapsed(c.child)) {
            h = std::max(h, c.child->MinIntrinsicHeight(width));
        }
    }
    return h;
}

int Overlay::MinIntrinsicWidth(int height) {
    int w = 0;
    for (auto& c : children) {
        if (c.child && !IsCollapsed(c.child)) {
            w = std::max(w, c.child->MinIntrinsicWidth(height));
        }
    }
    return w;
}

// Stretch children are re-measured to the overlay size; others sit inside it
void Overlay::SetBounds(Rect bounds) {
    lastBounds = bounds;
    for (auto& c : children) {
        if (!c.child || IsCollapsed(c.child)) {
            continue;
        }
        Size sz = c.size;
        bool stretchH = c.alignH == CrossAxisAlign::Stretch;
        bool stretchV = c.alignV == CrossAxisAlign::Stretch;
        if (stretchH) {
            sz.dx = bounds.dx;
        }
        if (stretchV) {
            sz.dy = bounds.dy;
        }
        if (stretchH || stretchV) {
            c.size = c.child->Layout(Tight(sz));
            sz = c.size;
        }
        c.child->SetBounds(AlignInCell(bounds, sz, c.alignH, c.alignV));
    }
}

//--- Wrap

static Kind kindWrap = "wrap";

Wrap::Wrap() {
    kind = kindWrap;
}

Wrap::~Wrap() {
    for (auto& c : children) {
        delete c.layout;
    }
}

int Wrap::LayoutChildCount() {
    return len(children);
}

ILayout* Wrap::LayoutChildAt(int idx) {
    return children[idx].layout;
}

int Wrap::ChildrenCount() const {
    return len(children);
}

boxElementInfo& Wrap::AddChild(ILayout* child, int flex) {
    if (LayoutTreeContains(child, this)) {
        ReportIf(true);
        return gRefusedBoxElement;
    }
    boxElementInfo v{};
    v.layout = child;
    v.flex = flex;
    VecAppend(children, v);
    return children[len(children) - 1];
}

boxElementInfo& Wrap::AddChild(ILayout* child) {
    return AddChild(child, 0);
}

// greedy wrap of already-measured children into rows that fit maxWidth
void Wrap::PackRows(int maxWidth) {
    VecReset(rows);
    int n = ChildrenCount();
    Row cur{};
    bool have = false;
    for (int i = 0; i < n; i++) {
        auto& v = children[i];
        if (!v.layout || IsCollapsed(v.layout)) {
            continue;
        }
        int nextW = have ? (cur.width + colGap + v.size.dx) : v.size.dx;
        if (have && maxWidth < Inf && nextW > maxWidth) {
            VecAppend(rows, cur);
            cur = {};
            have = false;
        }
        if (!have) {
            cur.start = i;
            cur.count = 1;
            cur.width = v.size.dx;
            cur.height = v.size.dy;
            have = true;
        } else {
            cur.count = i - cur.start + 1;
            cur.width = nextW;
            cur.height = std::max(cur.height, v.size.dy);
        }
    }
    if (have) {
        VecAppend(rows, cur);
    }
}

Size Wrap::Layout(const Constraints bc) {
    VecReset(rows);
    int n = ChildrenCount();
    if (n == 0) {
        return bc.Constrain(Size{});
    }

    // first pass: natural size of each child (unbounded width, like HBox)
    Constraints cbc = bc;
    cbc.min.dx = 0;
    cbc.max.dx = Inf;
    cbc = cbc.LoosenHeight();
    for (int i = 0; i < n; i++) {
        auto& v = children[i];
        if (!v.layout || IsCollapsed(v.layout)) {
            v.size = {};
            continue;
        }
        v.size = v.layout->Layout(cbc);
    }

    int maxWidth = bc.HasBoundedWidth() ? bc.max.dx : Inf;
    PackRows(maxWidth);

    // flex extra width per row among that row's flex children
    if (bc.HasBoundedWidth()) {
        Constraints fbc = cbc;
        for (auto& row : rows) {
            int totalFlex = 0;
            for (int i = row.start; i < row.start + row.count; i++) {
                auto& v = children[i];
                if (v.layout && !IsCollapsed(v.layout) && v.flex > 0) {
                    totalFlex += v.flex;
                }
            }
            int extra = maxWidth - row.width;
            if (totalFlex <= 0 || extra <= 0) {
                continue;
            }
            for (int i = row.start; i < row.start + row.count; i++) {
                auto& v = children[i];
                if (!v.layout || IsCollapsed(v.layout) || v.flex <= 0) {
                    continue;
                }
                int old = v.size.dx;
                int nw = old + Scale(extra, v.flex, totalFlex);
                v.size = v.layout->Layout(fbc.TightenWidth(nw));
                row.width += v.size.dx - old;
                row.height = std::max(row.height, v.size.dy);
            }
        }
    }

    int width = 0;
    int height = 0;
    for (int i = 0; i < len(rows); i++) {
        width = std::max(width, rows[i].width);
        if (i > 0) {
            height += rowGap;
        }
        height += rows[i].height;
    }
    return bc.Constrain(Size{width, height});
}

int Wrap::MinIntrinsicWidth(int height) {
    int w = 0;
    for (auto& v : children) {
        if (v.layout && !IsCollapsed(v.layout)) {
            w = std::max(w, v.layout->MinIntrinsicWidth(height));
        }
    }
    return w;
}

// with a bounded width this is the wrapped height; unbounded is one row
int Wrap::MinIntrinsicHeight(int width) {
    int n = ChildrenCount();
    if (n == 0) {
        return 0;
    }
    if (width == Inf || width <= 0) {
        int h = 0;
        for (auto& v : children) {
            if (v.layout && !IsCollapsed(v.layout)) {
                h = std::max(h, v.layout->MinIntrinsicHeight(Inf));
            }
        }
        return h;
    }
    int x = 0;
    int rowH = 0;
    int total = 0;
    bool have = false;
    for (int i = 0; i < n; i++) {
        auto& v = children[i];
        if (!v.layout || IsCollapsed(v.layout)) {
            continue;
        }
        int cw = v.layout->MinIntrinsicWidth(Inf);
        int ch = v.layout->MinIntrinsicHeight(width);
        int next = have ? (x + colGap + cw) : cw;
        if (have && next > width) {
            if (total > 0) {
                total += rowGap;
            }
            total += rowH;
            x = 0;
            rowH = 0;
            have = false;
            next = cw;
        }
        x = next;
        rowH = std::max(rowH, ch);
        have = true;
    }
    if (have) {
        if (total > 0) {
            total += rowGap;
        }
        total += rowH;
    }
    return total;
}

void Wrap::SetBounds(Rect bounds) {
    lastBounds = bounds;
    int y = bounds.y;
    for (int ri = 0; ri < len(rows); ri++) {
        auto& row = rows[ri];
        int x = bounds.x;
        if (rtl) {
            x = bounds.x + bounds.dx;
        }
        for (int i = row.start; i < row.start + row.count; i++) {
            auto& v = children[i];
            if (!v.layout || IsCollapsed(v.layout)) {
                continue;
            }
            if (rtl) {
                x -= v.size.dx;
            }
            Rect cell{x, y, v.size.dx, row.height};
            v.layout->SetBounds(AlignInCell(cell, v.size, CrossAxisAlign::CrossStart, alignCross));
            if (rtl) {
                x -= colGap;
            } else {
                x += v.size.dx + colGap;
            }
        }
        y += row.height + rowGap;
    }
}

#if IS_DEBUG

// Unit tests for the core layout engine. They use Spacer as a pure, HWND-free
// leaf (fixed intrinsic size, and its SetBounds records lastBounds), so whole
// layout trees can be exercised and their computed geometry asserted.

static bool LayoutRectEq(const Rect& r, int x, int y, int dx, int dy) {
    return r.x == x && r.y == y && r.dx == dx && r.dy == dy;
}

static void Layout_TestPrimitives() {
    utassert(Clamp(5, 0, 10) == 5);
    utassert(Clamp(-3, 0, 10) == 0);
    utassert(Clamp(50, 0, 10) == 10);

    utassert(Scale(10, 3, 2) == 15);
    utassert(Scale(10, 1, 0) == 0);   // divide-by-zero is guarded
    utassert(Scale(100, 1, 3) == 33); // truncates toward zero

    utassert(GuardInf(Inf, 5) == Inf);
    utassert(GuardInf(3, 5) == 5);

    Constraints t = Tight(Size{100, 50});
    utassert(t.IsTight() && t.HasBoundedWidth() && t.HasBoundedHeight());
    Size ts = t.Constrain(Size{999, 1});
    utassert(ts.dx == 100 && ts.dy == 50); // tight clamps both ways

    Constraints l = Loose(Size{100, 50});
    utassert(!l.IsTight());
    Size ls = l.Constrain(Size{200, 10});
    utassert(ls.dx == 100 && ls.dy == 10);

    Constraints e = ExpandInf();
    utassert(!e.HasBoundedWidth() && !e.HasBoundedHeight());

    Constraints tw = Loose(Size{100, 50}).TightenWidth(40);
    utassert(tw.min.dx == 40 && tw.max.dx == 40 && tw.max.dy == 50);

    // Inset shrinks max, leaving Inf as Inf
    Constraints ins = ExpandInf().Inset(20, 10);
    utassert(!ins.HasBoundedWidth() && !ins.HasBoundedHeight());
    Constraints ins2 = Loose(Size{100, 80}).Inset(20, 10);
    utassert(ins2.max.dx == 80 && ins2.max.dy == 70);
}

static void Layout_TestSpacer() {
    Spacer s(30, 20);
    Size sz = s.Layout(Loose(Size{100, 100}));
    utassert(sz.dx == 30 && sz.dy == 20);
    Size sz2 = s.Layout(Tight(Size{50, 60}));
    utassert(sz2.dx == 50 && sz2.dy == 60); // tight constraints win
    // regression: SetBounds must record lastBounds (used to be a no-op, which
    // left the AI chat panel's webview slot sized 0x0)
    s.SetBounds(Rect{5, 6, 40, 41});
    utassert(LayoutRectEq(s.lastBounds, 5, 6, 40, 41));
}

static void Layout_TestPadding() {
    auto* sp = new Spacer(40, 20);
    auto* pad = new Padding(sp, Insets{5, 10, 5, 10}); // top,right,bottom,left
    Size sz = pad->Layout(Loose(Size{200, 200}));
    utassert(sz.dx == 60 && sz.dy == 30); // child + horizontal/vertical insets
    pad->SetBounds(Rect{0, 0, 60, 30});
    utassert(LayoutRectEq(sp->lastBounds, 10, 5, 40, 20)); // inset by left/top
    delete pad;
}

static void Layout_TestVBoxFixed() {
    auto* s0 = new Spacer(50, 10);
    auto* s1 = new Spacer(30, 20);
    auto* vb = new VBox();
    vb->AddChild(s0);
    vb->AddChild(s1);
    LayoutToSize(vb, Size{100, 100});
    // CrossStart (default): natural width, x=0, stacked vertically
    utassert(LayoutRectEq(s0->lastBounds, 0, 0, 50, 10));
    utassert(LayoutRectEq(s1->lastBounds, 0, 10, 30, 20));
    delete vb;
}

static void Layout_TestVBoxAlign() {
    // CrossStretch: children stretched to full width
    {
        auto* s0 = new Spacer(50, 10);
        auto* s1 = new Spacer(30, 20);
        auto* vb = new VBox();
        vb->alignCross = CrossAxisAlign::Stretch;
        vb->AddChild(s0);
        vb->AddChild(s1);
        LayoutToSize(vb, Size{100, 100});
        utassert(LayoutRectEq(s0->lastBounds, 0, 0, 100, 10));
        utassert(LayoutRectEq(s1->lastBounds, 0, 10, 100, 20));
        delete vb;
    }
    // MainCenter: total height 30 centered in 100 -> offset 35
    {
        auto* s0 = new Spacer(50, 10);
        auto* s1 = new Spacer(30, 20);
        auto* vb = new VBox();
        vb->alignMain = MainAxisAlign::MainCenter;
        vb->AddChild(s0);
        vb->AddChild(s1);
        LayoutToSize(vb, Size{100, 100});
        utassert(s0->lastBounds.y == 35 && s1->lastBounds.y == 45);
        delete vb;
    }
    // MainEnd: bottom-aligned
    {
        auto* s0 = new Spacer(50, 10);
        auto* s1 = new Spacer(30, 20);
        auto* vb = new VBox();
        vb->alignMain = MainAxisAlign::MainEnd;
        vb->AddChild(s0);
        vb->AddChild(s1);
        LayoutToSize(vb, Size{100, 100});
        utassert(s0->lastBounds.y == 70 && s1->lastBounds.y == 80);
        delete vb;
    }
}

static void Layout_TestVBoxFlex() {
    // one flex slot fills the remaining height (the AI chat panel layout)
    {
        auto* top = new Spacer(0, 10);
        auto* slot = new Spacer(0, 0);
        auto* bottom = new Spacer(0, 10);
        auto* vb = new VBox();
        vb->alignCross = CrossAxisAlign::Stretch;
        vb->AddChild(top);
        vb->AddChild(slot, 1);
        vb->AddChild(bottom);
        LayoutToSize(vb, Size{200, 100});
        utassert(LayoutRectEq(top->lastBounds, 0, 0, 200, 10));
        utassert(LayoutRectEq(slot->lastBounds, 0, 10, 200, 80));
        utassert(LayoutRectEq(bottom->lastBounds, 0, 90, 200, 10));
        delete vb;
    }
    // two flex children with non-zero natural height split the extra evenly
    {
        auto* a = new Spacer(0, 10);
        auto* b = new Spacer(0, 30);
        auto* vb = new VBox();
        vb->AddChild(a, 1);
        vb->AddChild(b, 1);
        LayoutToSize(vb, Size{50, 100});
        // extra = 100-40 = 60, split 30/30 -> 40 and 60
        utassert(a->lastBounds.y == 0 && a->lastBounds.dy == 40);
        utassert(b->lastBounds.y == 40 && b->lastBounds.dy == 60);
        delete vb;
    }
}

static void Layout_TestHBoxFlex() {
    // two flex children with non-zero natural width split the extra evenly.
    // Regression: the old grow formula Scale(w+extra, flex, total) under-filled
    // when flex children had non-zero natural widths.
    auto* a = new Spacer(10, 0);
    auto* b = new Spacer(30, 0);
    auto* hb = new HBox();
    hb->AddChild(a, 1);
    hb->AddChild(b, 1);
    LayoutToSize(hb, Size{100, 20});
    // extra = 100-40 = 60, split 30/30 -> 40 and 60
    utassert(a->lastBounds.x == 0 && a->lastBounds.dx == 40);
    utassert(b->lastBounds.x == 40 && b->lastBounds.dx == 60);
    delete hb;
}

static void Layout_TestHBoxCrossCenter() {
    auto* a = new Spacer(20, 10);
    auto* b = new Spacer(30, 40);
    auto* hb = new HBox();
    hb->alignCross = CrossAxisAlign::CrossCenter;
    hb->AddChild(a);
    hb->AddChild(b);
    LayoutToSize(hb, Size{100, 50});
    // a: height 10 vertically centered in 50 -> y=20; b: height 40 -> y=5
    utassert(LayoutRectEq(a->lastBounds, 0, 20, 20, 10));
    utassert(LayoutRectEq(b->lastBounds, 20, 5, 30, 40));
    delete hb;
}

static void Layout_TestBoxGap() {
    {
        auto* a = new Spacer(20, 10);
        auto* b = new Spacer(30, 20);
        auto* hb = new HBox();
        hb->gap = 7;
        hb->AddChild(a);
        hb->AddChild(b);
        Size size = hb->Layout(Loose(Size{100, 100}));
        utassert(size.dx == 57 && size.dy == 20);
        hb->SetBounds(Rect{0, 0, size.dx, size.dy});
        utassert(LayoutRectEq(a->lastBounds, 0, 0, 20, 10));
        utassert(LayoutRectEq(b->lastBounds, 27, 0, 30, 20));
        delete hb;
    }
    {
        auto* a = new Spacer(20, 10);
        auto* collapsed = new Spacer(40, 40);
        auto* b = new Spacer(30, 20);
        auto* vb = new VBox();
        vb->gap = 7;
        vb->AddChild(a);
        vb->AddChild(collapsed);
        vb->AddChild(b);
        collapsed->SetVisibility(Visibility::Collapse);
        Size size = vb->Layout(Loose(Size{100, 100}));
        utassert(size.dx == 30 && size.dy == 37);
        vb->SetBounds(Rect{0, 0, size.dx, size.dy});
        utassert(LayoutRectEq(a->lastBounds, 0, 0, 20, 10));
        utassert(LayoutRectEq(b->lastBounds, 0, 17, 30, 20));
        delete vb;
    }
    {
        auto* a = new Spacer(20, 10);
        auto* b = new Spacer(30, 10);
        auto* hb = new HBox();
        hb->alignMain = MainAxisAlign::Homogeneous;
        hb->gap = 10;
        hb->AddChild(a);
        hb->AddChild(b);
        LayoutToSize(hb, Size{100, 10});
        utassert(LayoutRectEq(a->lastBounds, 0, 0, 45, 10));
        utassert(LayoutRectEq(b->lastBounds, 55, 0, 45, 10));
        delete hb;
    }
}

static void Layout_TestCollapsed() {
    auto* s0 = new Spacer(50, 10);
    auto* s1 = new Spacer(50, 20);
    auto* s2 = new Spacer(50, 30);
    auto* vb = new VBox();
    vb->AddChild(s0);
    vb->AddChild(s1);
    vb->AddChild(s2);
    s1->SetVisibility(Visibility::Collapse);
    LayoutToSize(vb, Size{50, 40});
    // collapsed s1 takes no space; s2 stacks right after s0
    utassert(LayoutRectEq(s0->lastBounds, 0, 0, 50, 10));
    utassert(s2->lastBounds.y == 10 && s2->lastBounds.dy == 30);
    utassert(LayoutRectEq(s1->lastBounds, 0, 0, 0, 0)); // never positioned
    delete vb;
}

static void Layout_TestAlign() {
    auto* child = new Spacer(20, 10);
    auto* al = new Align(child);
    al->HAlign = AlignCenter;
    al->VAlign = AlignCenter;
    al->Layout(Loose(Size{100, 100}));
    al->SetBounds(Rect{0, 0, 100, 100});
    // 20x10 centered in 100x100 -> x=40, y=45
    utassert(LayoutRectEq(child->lastBounds, 40, 45, 20, 10));
    delete al;
}

static void Layout_TestOverlay() {
    auto* body = new Spacer(80, 20);
    auto* close = new Spacer(10, 10);
    auto* ov = new Overlay();
    ov->AddChild(body);
    ov->AddChild(close, CrossAxisAlign::CrossEnd, CrossAxisAlign::CrossCenter);
    LayoutToSize(ov, Size{80, 20});
    // body fills the overlay; close sits at the right, vertically centered
    utassert(LayoutRectEq(body->lastBounds, 0, 0, 80, 20));
    utassert(LayoutRectEq(close->lastBounds, 70, 5, 10, 10));
    delete ov;
}

static void Layout_TestWrap() {
    // three 40-wide children in a 90-wide box wrap to two + one
    {
        auto* a = new Spacer(40, 10);
        auto* b = new Spacer(40, 10);
        auto* c = new Spacer(40, 10);
        auto* w = new Wrap();
        w->AddChild(a);
        w->AddChild(b);
        w->AddChild(c);
        LayoutToSize(w, Size{90, 100});
        utassert(LayoutRectEq(a->lastBounds, 0, 0, 40, 10));
        utassert(LayoutRectEq(b->lastBounds, 40, 0, 40, 10));
        utassert(LayoutRectEq(c->lastBounds, 0, 10, 40, 10));
        delete w;
    }
    // flex extra on a row goes to the flex child
    {
        auto* a = new Spacer(40, 10);
        auto* b = new Spacer(40, 10);
        auto* w = new Wrap();
        w->AddChild(a, 1);
        w->AddChild(b);
        LayoutToSize(w, Size{100, 20});
        utassert(LayoutRectEq(a->lastBounds, 0, 0, 60, 10));
        utassert(LayoutRectEq(b->lastBounds, 60, 0, 40, 10));
        delete w;
    }
    // rtl: each row packs from the right
    {
        auto* a = new Spacer(40, 10);
        auto* b = new Spacer(40, 10);
        auto* c = new Spacer(40, 10);
        auto* w = new Wrap();
        w->rtl = true;
        w->AddChild(a);
        w->AddChild(b);
        w->AddChild(c);
        LayoutToSize(w, Size{90, 100});
        utassert(LayoutRectEq(a->lastBounds, 50, 0, 40, 10));
        utassert(LayoutRectEq(b->lastBounds, 10, 0, 40, 10));
        utassert(LayoutRectEq(c->lastBounds, 50, 10, 40, 10));
        delete w;
    }
}

static void Layout_TestHBoxRtl() {
    // MainStart packs to the right: first child at the right edge
    {
        auto* a = new Spacer(20, 10);
        auto* b = new Spacer(30, 10);
        auto* hb = new HBox();
        hb->rtl = true;
        hb->AddChild(a);
        hb->AddChild(b);
        LayoutToSize(hb, Size{100, 10});
        utassert(LayoutRectEq(a->lastBounds, 80, 0, 20, 10));
        utassert(LayoutRectEq(b->lastBounds, 50, 0, 30, 10));
        delete hb;
    }
    // MainEnd packs toward the left (the RTL end)
    {
        auto* a = new Spacer(20, 10);
        auto* b = new Spacer(30, 10);
        auto* hb = new HBox();
        hb->rtl = true;
        hb->alignMain = MainAxisAlign::MainEnd;
        hb->AddChild(a);
        hb->AddChild(b);
        LayoutToSize(hb, Size{100, 10});
        utassert(LayoutRectEq(a->lastBounds, 30, 0, 20, 10));
        utassert(LayoutRectEq(b->lastBounds, 0, 0, 30, 10));
        delete hb;
    }
}

void Layout_UnitTests() {
    Layout_TestPrimitives();
    Layout_TestSpacer();
    Layout_TestPadding();
    Layout_TestVBoxFixed();
    Layout_TestVBoxAlign();
    Layout_TestVBoxFlex();
    Layout_TestHBoxFlex();
    Layout_TestHBoxCrossCenter();
    Layout_TestBoxGap();
    Layout_TestCollapsed();
    Layout_TestAlign();
    Layout_TestOverlay();
    Layout_TestWrap();
    Layout_TestHBoxRtl();
}
#endif
