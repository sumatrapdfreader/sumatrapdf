/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"

namespace mui {

HorizontalLayout& HorizontalLayout::Add(DirectionalLayoutData& ld)
{
    elements.Append(ld);
    return *this;
}

Size HorizontalLayout::DesiredSize()
{
    return desiredSize;
}

void HorizontalLayout::Measure(const Size availableSize)
{

}

void HorizontalLayout::Arrange(const Rect finalRect)
{

}

VerticalLayout& VerticalLayout::Add(DirectionalLayoutData& ld)
{
    elements.Append(ld);
    return *this;
}

Size VerticalLayout::DesiredSize()
{
    return desiredSize;
}

void VerticalLayout::Measure(const Size availableSize)
{

}

void VerticalLayout::Arrange(const Rect finalRect)
{

}

}
