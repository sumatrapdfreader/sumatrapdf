/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"

namespace mui {

HorizontalLayout& HorizontalLayout::Add(DirectionalLayoutData& ld, bool ownsElement)
{
    ld.ownsElement = ownsElement;
    elements.Append(ld);
    return *this;
}

Size HorizontalLayout::DesiredSize()
{
    return desiredSize;
}

void HorizontalLayout::Measure(const Size availableSize)
{
    for (DirectionalLayoutData *e = elements.IterStart(); e; e = elements.IterNext()) {
        e->element->Measure(availableSize);
        e->desiredSize = e->element->DesiredSize();
    }
}

void HorizontalLayout::Arrange(const Rect finalRect)
{
    // TODO: here goes the magic
    for (DirectionalLayoutData *e = elements.IterStart(); e; e = elements.IterNext()) {
        e->element->Arrange(e->finalPos);
    }
}

HorizontalLayout::~HorizontalLayout()
{
    for (DirectionalLayoutData *e = elements.IterStart(); e; e = elements.IterNext()) {
        if (e->ownsElement)
            delete e->element;
    }
}

VerticalLayout::~VerticalLayout()
{
    for (DirectionalLayoutData *e = elements.IterStart(); e; e = elements.IterNext()) {
        if (e->ownsElement)
            delete e->element;
    }
}

VerticalLayout& VerticalLayout::Add(DirectionalLayoutData& ld, bool ownsElement)
{
    ld.ownsElement = ownsElement;
    elements.Append(ld);
    return *this;
}

Size VerticalLayout::DesiredSize()
{
    return desiredSize;
}

void VerticalLayout::Measure(const Size availableSize)
{
    for (DirectionalLayoutData *e = elements.IterStart(); e; e = elements.IterNext()) {
        e->element->Measure(availableSize);
        e->desiredSize = e->element->DesiredSize();
    }
}

void VerticalLayout::Arrange(const Rect finalRect)
{
    // TODO: here goes the magic
    for (DirectionalLayoutData *e = elements.IterStart(); e; e = elements.IterNext()) {
        e->element->Arrange(e->finalPos);
    }
}

}
