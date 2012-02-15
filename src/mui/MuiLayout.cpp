/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"

namespace mui {

ElInContainerAlign::ElInContainerAlign(const ElInContainerAlign& other)
{
    elementPoint = other.elementPoint;
    containerPoint = other.containerPoint;
}

void ElInContainerAlign::Set(ElAlign align)
{
    if (ElAlignCenter == align) {
        elementPoint   = .5f;
        containerPoint = .5f;
    } else if ((ElAlignTop == align) || (ElAlignLeft == align)) {
        elementPoint   = 0.f;
        containerPoint = 0.f;
    } else if ((ElAlignBottom == align) || (ElAlignRight == align)) {
        elementPoint   = 1.f;
        containerPoint = 1.f;
    } else {
        CrashIf(true);
    }
}

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

#if 0
class VertAccessor
{
public:
    int& Size(Rect& r) { return r.Height; }
    int& Pos (Rect& r) { return r.Y; }
    int& Size(Size& s) { return s.Height; }
    int& Pos (Size& s) { return s.Y; }
};

class HorizAccessor
{
public:
    int& Size(Rect& r) { return r.Width; }
    int& Pos (Rect& r) { return r.X; }
    int& Size(Size& s) { return s.Width; }
    int& Pos (Size& s) { return s.Y; }
};
#endif

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
        // calc the height of the element
        if (SizeSelf == e->sizeNonLayoutAxis)
            elSize = e->desiredSize.Height;
        else
            elSize = (int)((float)finalRect.Height * e->sizeNonLayoutAxis);
        if (elSize > finalRect.Height)
            elSize = finalRect.Height;
        e->finalPos.Height = elSize;

        // calc y position of the element
        float tmp = (float)finalRect.Height * e->alignNonLayoutAxis.containerPoint;
        int containerPoint = (int)tmp;
        tmp = (float)elSize * e->alignNonLayoutAxis.elementPoint;
        int elementPoint = (int)tmp;
        e->finalPos.Y = containerPoint - elementPoint;
    }

    for (e = els.IterStart(); e; e = els.IterNext()) {
        e->element->Arrange(e->finalPos);
    }
}

// TODO: this is almost identical to HorizontalLayout::Arrange().
// Is there a clever way to parametrize this to have only one implementation?
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
        // calc the height of the element
        if (SizeSelf == e->sizeNonLayoutAxis)
            elSize = e->desiredSize.Width;
        else
            elSize = (int)((float)finalRect.Width * e->sizeNonLayoutAxis);
        if (elSize > finalRect.Width)
            elSize = finalRect.Width;
        e->finalPos.Width = elSize;

        // calc x position of the element
        float tmp = (float)finalRect.Width * e->alignNonLayoutAxis.containerPoint;
        int containerPoint = (int)tmp;
        tmp = (float)elSize * e->alignNonLayoutAxis.elementPoint;
        int elementPoint = (int)tmp;
        e->finalPos.X = containerPoint - elementPoint;
    }

    for (e = els.IterStart(); e; e = els.IterNext()) {
        e->element->Arrange(e->finalPos);
    }
}

}
