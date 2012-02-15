/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Mui_h
#error "this is only meant to be included by Mui.h inside mui namespace"
#endif
#ifdef MuiLayout_h
#error "dont include twice!"
#endif
#define MuiLayout_h

// WPF-like layout system. Measure() should update desiredSize
// Then the parent uses it to calculate the size of its children
// and uses Arrange() to set it.
// availableSize can have SizeInfinite as dx or dy to allow
// using as much space as the window wants
// Every Control implements ILayout for calculating their desired
// size but can also have independent ILayout (which is for controls
// that contain other controls). This allows decoupling layout logic
// from controls and implementing generic layouts.
class ILayout
{
public:
    virtual void Measure(const Size availableSize) = 0;
    virtual void Arrange(const Rect finalRect) = 0;
};

class Control;

struct VerticalLayoutData {
    Control *   control;
};

class VerticalLayout : ILayout
{
    Vec<VerticalLayoutData> controls;

public:
    VerticalLayout() {
    }

    VerticalLayout& Add(Control *c);
    virtual ~VerticalLayout() {
    }

    virtual void Measure(const Size availableSize);
    virtual void Arrange(const Rect finalRect);
};

