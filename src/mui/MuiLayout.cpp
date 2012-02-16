/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"

namespace mui {

DirectionalLayout::~DirectionalLayout()
{
    for (DirectionalLayoutData *e = els.IterStart(); e; e = els.IterNext()) {
        if (e->ownsElement)
            delete e->element;
    }
}

DirectionalLayout& DirectionalLayout::Add(DirectionalLayoutData& ld, bool ownsElement)
{
    ld.ownsElement = ownsElement;
    els.Append(ld);
    return *this;
}

void DirectionalLayout::Measure(const Size availableSize)
{
    for (DirectionalLayoutData *e = els.IterStart(); e; e = els.IterNext()) {
        e->element->Measure(availableSize);
        e->desiredSize = e->element->DesiredSize();
    }
}

static int CalcScaledClippedSize(int size, float scale, int selfSize)
{
    int scaledSize = selfSize;
    if (SizeSelf != scale)
        scaledSize = (int)((float)size * scale);
    if (scaledSize > size)
        scaledSize = size;
    return scaledSize;
}

void HorizontalLayout::Arrange(const Rect finalRect)
{
    DirectionalLayoutData *e;

    float toDistributeTotal = 0.f;
    int remainingSpace = finalRect.Width;
    for (e = els.IterStart(); e; e = els.IterNext()) {
        if (SizeSelf == e->sizeLayoutAxis)
            remainingSpace -= e->desiredSize.Width;
        else
            toDistributeTotal += e->sizeLayoutAxis;
    }

    int x = 0;
    int elSize;
    for (e = els.IterStart(); e; e = els.IterNext()) {
        e->finalPos.X = x;
        if (SizeSelf == e->sizeLayoutAxis) {
            e->finalPos.Width = e->desiredSize.Width;
        } else {
            elSize = 0;
            if ((remainingSpace > 0) && (0.f != toDistributeTotal)) {
                float tmp = ((float)remainingSpace * e->sizeLayoutAxis) / toDistributeTotal;
                elSize = (int)tmp;
            }
            e->finalPos.Width = elSize;
        }
        x += e->finalPos.Width;
    }

    for (e = els.IterStart(); e; e = els.IterNext()) {
        e->finalPos.Height = CalcScaledClippedSize(finalRect.Height, e->sizeNonLayoutAxis, e->desiredSize.Height);
        e->finalPos.Y = e->alignNonLayoutAxis.CalcOffset(e->finalPos.Height, finalRect.Height);
        e->element->Arrange(e->finalPos);
    }
}

void VerticalLayout::Arrange(const Rect finalRect)
{
    DirectionalLayoutData *e;

    float toDistributeTotal = 0.f;
    int remainingSpace = finalRect.Height;
    for (e = els.IterStart(); e; e = els.IterNext()) {
        if (SizeSelf == e->sizeLayoutAxis)
            remainingSpace -= e->desiredSize.Height;
        else
            toDistributeTotal += e->sizeLayoutAxis;
    }

    int y = 0;
    int elSize;
    for (e = els.IterStart(); e; e = els.IterNext()) {
        e->finalPos.Y = y;
        if (SizeSelf == e->sizeLayoutAxis) {
            e->finalPos.Height = e->desiredSize.Height;
        } else {
            elSize = 0;
            if ((remainingSpace > 0) && (0.f != toDistributeTotal)) {
                float tmp = ((float)remainingSpace * e->sizeLayoutAxis) / toDistributeTotal;
                elSize = (int)tmp;
            }
            e->finalPos.Height = elSize;
        }
        y += e->finalPos.Height;
    }

    for (e = els.IterStart(); e; e = els.IterNext()) {
        e->finalPos.Width = CalcScaledClippedSize(finalRect.Width, e->sizeNonLayoutAxis, e->desiredSize.Width);
        e->finalPos.X = e->alignNonLayoutAxis.CalcOffset(e->finalPos.Width, finalRect.Width);
        e->element->Arrange(e->finalPos);
    }
}

}
