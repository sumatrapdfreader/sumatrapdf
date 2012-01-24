/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef mui_h
#define mui_h

#include <stdint.h>
#include "BaseUtil.h"
#include "Vec.h"
#include "BitManip.h"
#include "GeomUtil.h"
#include "MuiCss.h"

namespace mui {

using namespace Gdiplus;
using namespace css;

class VirtWnd;
class VirtWndHwnd;
class IClickHandler;

#define SizeInfinite ((INT)-1)

void        Initialize();
void        Destroy();

Graphics *  GetGraphicsForMeasureText();
Rect        MeasureTextWithFont(Font *f, const TCHAR *s);
void        RequestRepaint(VirtWnd *w);
void        RequestLayout(VirtWnd *w);
void        RegisterForClickEvent(VirtWnd *w, IClickHandler *clickHandlers);

class IClickHandler
{
public:
    IClickHandler() {
    };
    virtual void Clicked(VirtWnd *w) = 0;
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
    virtual void Arrange(Rect finalRect, VirtWnd *wnd) = 0;
};

// Manages painting process of VirtWnd window and all its children.
// Automatically does double-buffering for less flicker.
// Create one object for each HWND-backed VirtWnd and keep it around.
class VirtWndPainter
{
    VirtWndHwnd *wnd;
    // bitmap for double-buffering
    Bitmap *cacheBmp;

    virtual void PaintBackground(Graphics *g, Rect r);
public:
    VirtWndPainter(VirtWndHwnd *wnd) : wnd(wnd), cacheBmp(NULL) {
    }

    void OnPaint(HWND hwnd);
};

class EventMgr
{
    VirtWndHwnd *   wndRoot;
    // current window over which the mouse is
    VirtWnd *       currOver;

    struct ClickHandler {
        VirtWnd *        wndSource;
        IClickHandler *  clickHandler;
    };

    Vec<ClickHandler> clickHandlers;

    LRESULT OnMouseMove(WPARAM keys, int x, int y, bool& handledOut);
    LRESULT OnLButtonUp(WPARAM keys, int x, int y, bool& handledOut);
public:
    EventMgr(VirtWndHwnd *wndRoot) : wndRoot(wndRoot), currOver(NULL)
    {
        CrashIf(!wndRoot);
        //CrashIf(wndRoot->hwnd);
    }
    ~EventMgr() {}

    LRESULT OnMessage(UINT msg, WPARAM wParam, LPARAM lParam, bool& handledOut);

    void RegisterForClickEvent(VirtWnd *wndSource, IClickHandler *clickeEvent);
    IClickHandler *GetClickHandlerFor(VirtWnd *wndSource);
};

class VirtWnd
{
public:
    // allows a window to opt-out from being notified about
    // input events, stored in wantedInputBits
    enum WndWantedInputBits : int {
        WantsMouseOverBit   = 0,
        WantsMouseDownBit   = 1,
        WantsMouseUpBit     = 2,
        WantsMouseClickBit  = 3,
        WndWantedInputBitLast
    };

    // describes current state of a window, stored in stateBits
    enum WndStateBits : int {
        MouseOverBit = 0,
        IsPressedBit = 1,
        // using IsHidden and not IsVisible so that 0 is default, visible state
        IsHiddenBit  = 2,
        WndStateBitLast
    };

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

    virtual void Paint(Graphics *gfx, int offX, int offY);

    // WPF-like layout system. Measure() should update desiredSize
    // Then the parent uses it to calculate the size of its children
    // and uses Arrange() to set it.

    // availableSize can have SizeInfinite as dx or dy to allow
    // using as much space as the window wants
    virtual void Measure(Size availableSize);
    virtual void Arrange(Rect finalRect);
    Size            desiredSize;

    // used e.g. by a button to change the look when mouse.The intention
    // is that in response to those a window should only do minimal processing
    // that affects the window itself, not the rest of the system
    virtual void NotifyMouseEnter() {}
    virtual void NotifyMouseLeave() {}

    uint16_t        wantedInputBits; // WndWantedInputBits
    uint16_t        stateBits;       // WndStateBits
    // windows with bigger z-order are painted on top, 0 is default
    int16_t         zOrder;

    bool WantsMouseClick() const {
        return bit::IsSet(wantedInputBits, WantsMouseClickBit);
    }

    bool IsVisible() const {
        return !bit::IsSet(stateBits, IsHiddenBit);
    }

    void Show() {
        if (IsVisible())
            return; // perf: avoid unnecessary repaints
        bit::Clear(stateBits, IsHiddenBit);
        RequestRepaint(this);
    }

    void Hide() {
        if (!IsVisible())
            return;
        RequestRepaint(this); // request repaint before hiding, to trigger repaint
        bit::Set(stateBits, IsHiddenBit);
    }

    Layout *        layout;

    VirtWnd *       parent;

    // VirtWnd doesn't own this object to allow sharing the same
    // instance among multiple windows. Whoever allocates them
    // must delete them
    Style *         styleDefault;

    // only used by VirtWndHwnd but we need it here
    HWND            hwndParent;
    HWND            GetHwndParent() const;

    // position and size (relative to parent, might be outside of parent's bounds)
    Rect            pos;

private:
    Vec<VirtWnd*>   children;
};

// VirtWnd that has to be the root of window tree and is
// backed by a HWND. It combines a painter and event
// manager for this HWND
class VirtWndHwnd : public VirtWnd
{
    bool                layoutRequested;

public:
    VirtWndPainter *    painter;
    EventMgr *          evtMgr;

    VirtWndHwnd(HWND hwnd = NULL) : painter(NULL), evtMgr(NULL), layoutRequested(false) {
        if (hwnd)
            SetHwnd(hwnd);
    }

    virtual ~VirtWndHwnd() {
        delete evtMgr;
        delete painter;
    }

    // mark for re-layout at the earliest convenience
    void RequestLayout() {
        layoutRequested = true;
    }

    void LayoutIfRequested() {
        if (layoutRequested)
            TopLevelLayout();
    }

    void SetHwnd(HWND hwnd) {
        CrashIf(NULL != hwndParent);
        hwndParent = hwnd;
        evtMgr = new EventMgr(this);
        painter = new VirtWndPainter(this);
    }

    void OnPaint(HWND hwnd) {
        CrashIf(hwnd != hwndParent);
        painter->OnPaint(hwnd);
    }

    // called when either the window size changed (as a result
    // of WM_SIZE) or when window tree changes
    virtual void TopLevelLayout() {
        CrashIf(!hwndParent);
        ClientRect rc(hwndParent);
        Size availableSize(rc.dx, rc.dy);
        Measure(availableSize);
        Size s = desiredSize;
        // TODO: a hack, center the window based on size
        // should have some other way of doing this, like a margin
        // alternatively, can make adjustments in custom Arrange()
        // (although that would be hacky too)
        int x = (rc.dx - s.Width) / 2;
        int y = (rc.dy - s.Height) / 2;
        Rect r(x, y, s.Width, s.Height);
        Arrange(r);
        layoutRequested = false;
    }
};

class VirtWndButton : public VirtWnd
{
public:
    VirtWndButton(const TCHAR *s);

    virtual ~VirtWndButton() {
        free(text);
    }

    void SetText(const TCHAR *s);

    void RecalculateSize();

    virtual void Measure(Size availableSize);
    virtual void Paint(Graphics *gfx, int offX, int offY);

    virtual void NotifyMouseEnter() {
        bit::Set(stateBits, MouseOverBit);
        RequestRepaint(this);
    }

    virtual void NotifyMouseLeave() {
        bit::Clear(stateBits, MouseOverBit);
        RequestRepaint(this);
    }

    void GetStyleForState(Style **first, Style **second) const;
    Prop *GetPropForState(PropType type) const;
    void GetPropsForState(PropToFind *propsToFind, size_t propsToFindCount) const;

    Font *GetFontForState() const;

    TCHAR *         text;

    // gStyleButtonDefault if styleDefault is NULL
    Style *         styleMouseOver; // gStyleButtonMouseOver if NULL
};

}

#endif
