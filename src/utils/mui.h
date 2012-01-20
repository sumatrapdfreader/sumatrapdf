/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef mui_h
#define mui_h

#include "BaseUtil.h"
#include "Vec.h"

namespace mui {

using namespace Gdiplus;

class VirtWnd;

#define InfiniteDx ((INT)-1)
#define InfintieDy ((INT)-1)

void Initialize();
void Destroy();

Graphics *GetGraphicsForMeasureText();
Rect MeasureTextWithFont(Font *f, const TCHAR *s);

struct Padding {
    Padding() : left(0), right(0), top(0), bottom(0) {
    }

    Padding(int n) {
        left = right = top = bottom;
    }

    Padding(int x, int y) {
        left = right = x;
        top = bottom = y;
    }

    void operator =(const Padding& other) {
        left = other.left;
        right = other.right;
        top = other.top;
        bottom = other.bottom;
    }

    int left, right, top, bottom;
};

// Layout can be optionally set on VirtWnd. If set, it'll be
// used to layout this window. This effectively over-rides Measure()/Arrange()
// calls of VirtWnd. This allows to decouple layout logic from VirtWnd class
// and implement generic layout algorithms.
class Layout
{
public:
    Layout() {
    }

    virtual ~Layout() {
    }

    virtual void Measure(Size availableSize, VirtWnd *wnd) = 0;
    virtual void Arrange(Rect finalSize, VirtWnd *wnd) = 0;
};

class VirtWnd
{
public:
    VirtWnd(VirtWnd *parent=NULL);
    virtual ~VirtWnd();

    void SetParent(VirtWnd *parent) {
        this->parent = parent;
    }

    void AddChild(VirtWnd *wnd, int pos = -1);
    VirtWnd *GetChild(size_t idx) {
        return children.At(idx);
    }

    size_t GetChildCount() const {
        return children.Count();
    }

    HWND GetHwndParent() const;

    virtual void Paint(Graphics *gfx, int offX, int offY);

    // WPF-like layout system. Measure() should update desiredSize
    // Then the parent uses it to calculate the size of its children
    // and uses Arrange() to set it.

    // availableSize can have InfiniteDx or InfiniteDy to allow
    // using as much space as the window wants
    virtual void Measure(Size availableSize);
    virtual void Arrange(Rect finalRect);
    Size            desiredSize;

    Layout *        layout;

    VirtWnd *       parent;

    // can be backed by a HWND
    HWND            hwndParent;

    // position and size (relative to parent, might be outside of parent's bounds)
    Rect            pos;

    bool            isVisible;

    Padding         padding;
private:
    Vec<VirtWnd*>   children;

};

class VirtWndButton : public VirtWnd
{
public:
    VirtWndButton(const TCHAR *s) {
        text = NULL;
        padding = Padding(8, 4);
        SetText(s);
    }

    virtual ~VirtWndButton() {
        free(text);
    }

    void SetText(const TCHAR *s);

    void RecalculateSize();

    virtual void Measure(Size availableSize);
    virtual void Paint(Graphics *gfx, int offX, int offY);

    TCHAR *         text;
};

// Manages painting process of VirtWnd window and all its children.
// Automatically does double-buffering for less flicker.
// Create one object for each HWND-backed VirtWnd and keep it around.
class VirtWndPainter
{
    // bitmap for double-buffering
    Bitmap *cacheBmp;

    void PaintRecursively(Graphics *g, VirtWnd *wnd, int offX, int offY);

    virtual void PaintBackground(Graphics *g, Rect r);
public:
    VirtWndPainter() : cacheBmp(NULL)
    {
    }

    void OnPaint(HWND hwnd, VirtWnd *hwndWnd);
};

}

#endif
