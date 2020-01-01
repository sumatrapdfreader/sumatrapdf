/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/HtmlParserLookup.h"
#include "Mui.h"

namespace mui {

DirectionalLayout::~DirectionalLayout() {
}

DirectionalLayout& DirectionalLayout::Add(const DirectionalLayoutData& ld) {
    els.Append(ld);
    return *this;
}

Size DirectionalLayout::Measure(const Size availableSize) {
    for (DirectionalLayoutData& e : els) {
        e.element->Measure(availableSize);
        e.desiredSize = e.element->DesiredSize();
    }
    // TODO: this is wrong
    return desiredSize;
}

static int CalcScaledClippedSize(int size, float scale, int selfSize) {
    int scaledSize = selfSize;
    if (SizeSelf != scale)
        scaledSize = (int)((float)size * scale);
    if (scaledSize > size)
        scaledSize = size;
    return scaledSize;
}

struct SizeInfo {
    int size;
    float scale;

    int finalPos;
    int finalSize;
};

static void RedistributeSizes(Vec<SizeInfo>& sizes, int totalSize) {
    float toDistributeTotal = 0.f;
    int remainingSpace = totalSize;

    for (SizeInfo& si : sizes) {
        if (SizeSelf == si.scale)
            remainingSpace -= si.size;
        else
            toDistributeTotal += si.scale;
    }

    int pos = 0;
    for (SizeInfo& si : sizes) {
        if (SizeSelf == si.scale)
            si.finalSize = si.size;
        else if (remainingSpace > 0 && toDistributeTotal != 0.f)
            si.finalSize = (int)(((float)remainingSpace * si.scale) / toDistributeTotal);
        else
            si.finalSize = 0;
        si.finalPos = pos;
        pos += si.finalSize;
    }
}

void HorizontalLayout::Arrange(const Rect finalRect) {
    Vec<SizeInfo> sizes;

    for (DirectionalLayoutData& e : els) {
        SizeInfo sizeInfo = {e.desiredSize.Width, e.sizeLayoutAxis, 0, 0};
        sizes.Append(sizeInfo);
    }
    RedistributeSizes(sizes, finalRect.Width);

    auto si = sizes.begin();
    for (DirectionalLayoutData& e : els) {
        int dy = CalcScaledClippedSize(finalRect.Height, e.sizeNonLayoutAxis, e.desiredSize.Height);
        int y = e.alignNonLayoutAxis.CalcOffset(dy, finalRect.Height);
        e.element->Arrange(Rect((*si).finalPos, y, (*si).finalSize, dy));
        ++si;
    }
    CrashIf(si != sizes.end());
}

void VerticalLayout::Arrange(const Rect finalRect) {
    Vec<SizeInfo> sizes;

    for (DirectionalLayoutData& e : els) {
        SizeInfo sizeInfo = {e.desiredSize.Height, e.sizeLayoutAxis, 0, 0};
        sizes.Append(sizeInfo);
    }
    RedistributeSizes(sizes, finalRect.Height);

    auto si = sizes.begin();
    for (DirectionalLayoutData& e : els) {
        int dx = CalcScaledClippedSize(finalRect.Width, e.sizeNonLayoutAxis, e.desiredSize.Width);
        int x = e.alignNonLayoutAxis.CalcOffset(dx, finalRect.Width);
        e.element->Arrange(Rect(x, (*si).finalPos, dx, (*si).finalSize));
        ++si;
    }
    CrashIf(si != sizes.end());
}

} // namespace mui
