#ifndef MuiLayout_h
#define MuiLayout_h

// This is only meant to be included by Mui.h inside mui namespace

class Control;

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
#endif
