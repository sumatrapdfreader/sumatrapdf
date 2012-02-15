/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"

namespace mui {

HorizontalLayout& HorizontalLayout::Add(DirectionalLayoutData& ld, bool ownsElement)
{
    ld.ownsElement = ownsElement;
    els.Append(ld);
    return *this;
}

Size HorizontalLayout::DesiredSize()
{
    return desiredSize;
}

void HorizontalLayout::Measure(const Size availableSize)
{
    for (DirectionalLayoutData *e = els.IterStart(); e; e = els.IterNext()) {
        e->element->Measure(availableSize);
        e->desiredSize = e->element->DesiredSize();
    }
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
        e->finalPos.Y = 0;
        // TODO: use sizeNonLayoutAxis and alignNonLayoutAxis to calculate
        // the y position and height
        elSize = e->desiredSize.Height;
        if (finalRect.Height > elSize)
            elSize = finalRect.Height;
        e->finalPos.Height = elSize;
    }

    for (e = els.IterStart(); e; e = els.IterNext()) {
        e->element->Arrange(e->finalPos);
    }
}

HorizontalLayout::~HorizontalLayout()
{
    for (DirectionalLayoutData *e = els.IterStart(); e; e = els.IterNext()) {
        if (e->ownsElement)
            delete e->element;
    }
}

VerticalLayout::~VerticalLayout()
{
    for (DirectionalLayoutData *e = els.IterStart(); e; e = els.IterNext()) {
        if (e->ownsElement)
            delete e->element;
    }
}

VerticalLayout& VerticalLayout::Add(DirectionalLayoutData& ld, bool ownsElement)
{
    ld.ownsElement = ownsElement;
    els.Append(ld);
    return *this;
}

Size VerticalLayout::DesiredSize()
{
    return desiredSize;
}

void VerticalLayout::Measure(const Size availableSize)
{
    for (DirectionalLayoutData *e = els.IterStart(); e; e = els.IterNext()) {
        e->element->Measure(availableSize);
        e->desiredSize = e->element->DesiredSize();
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
        e->finalPos.X = 0;
        // TODO: use sizeNonLayoutAxis and alignNonLayoutAxis to calculate
        // the x position and width
        elSize = e->desiredSize.Width;
        if (finalRect.Width > elSize)
            elSize = finalRect.Width;
        e->finalPos.Width = elSize;
    }

    for (e = els.IterStart(); e; e = els.IterNext()) {
        e->element->Arrange(e->finalPos);
    }
}

}
