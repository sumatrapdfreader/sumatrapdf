/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef MuiLayout_h
#define MuiLayout_h

// This is only meant to be included by Mui.h inside mui namespace

// Layout can be optionally set on Control. If set, it'll be
// used to layout this window. This effectively over-rides Measure()/Arrange()
// calls of Control. This allows to decouple layout logic from Control class
// and implement generic layout algorithms.
class Layout
{
public:
    Layout() {
    }

    virtual ~Layout() {
    }

    virtual void Measure(const Size availableSize, Control *wnd) = 0;
    virtual void Arrange(const Rect finalRect, Control *wnd) = 0;
};

struct VerticalLayoutData {
    Control *   control;
};

class VerticalLayout : Layout
{
    Vec<VerticalLayoutData> controls;

public:
    VerticalLayout() {
    }

    VerticalLayout& Add(Control *c);
    virtual ~VerticalLayout() {
    }

    virtual void Measure(const Size availableSize, Control *wnd);
    virtual void Arrange(const Rect finalRect, Control *wnd);
};

#endif
