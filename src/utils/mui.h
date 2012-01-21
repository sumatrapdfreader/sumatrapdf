/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef mui_h
#define mui_h

#include "BaseUtil.h"
#include "Vec.h"
#include "BitManip.h"

namespace mui {

using namespace Gdiplus;

class VirtWnd;

#define InfiniteDx ((INT)-1)
#define InfintieDy ((INT)-1)

void        Initialize();
void        Destroy();

Graphics *  GetGraphicsForMeasureText();
Rect        MeasureTextWithFont(Font *f, const TCHAR *s);
void        RequestRepaint(VirtWnd *w);

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

    // used e.g. by a button to change the look when mouse.The intention
    // is that in response to those a window should only do minimal processing
    // that affects the window itself, not the rest of the system
    virtual void NotifyMouseEnter() {}
    virtual void NotifyMouseLeave() {}

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
    // button state flags that influence how a button looks
    enum StateBits : int {
        MouseOverBit = 0,
        IsPressedBit = 1
        // IsDefaultBit = 2 (?)
    };

    short state; // StateBits

public:
    VirtWndButton(const TCHAR *s) {
        text = NULL;
        state = 0;
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

    virtual void NotifyMouseEnter() {
        bit::Set(state, MouseOverBit);
        RequestRepaint(this);
    }

    virtual void NotifyMouseLeave() {
        bit::Clear(state, MouseOverBit);
        RequestRepaint(this);
    }

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

class EventMgr
{
    VirtWnd *   rootWnd;
    // current window over which mouse is
    VirtWnd *   currOver;

    LRESULT OnMouseMove(WPARAM keys, int x, int y, bool& handledOut);
public:
    EventMgr(VirtWnd *rootWnd = NULL) {
        currOver = NULL;
        SetRootWindow(rootWnd);
    }
    ~EventMgr() {}

    void SetRootWindow(VirtWnd *root) {
        rootWnd = root;
        if (rootWnd) {
            // must be window backed by HWND
            CrashIf(!rootWnd->hwndParent);
        }
    }

    LRESULT OnMessage(UINT msg, WPARAM wParam, LPARAM lParam, bool& handledOut);
};

}

#endif
