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
void        RequestRepaint(VirtWnd *w, const Rect *r1 = NULL, const Rect *r2 = NULL);
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

    virtual void Measure(const Size availableSize, VirtWnd *wnd) = 0;
    virtual void Arrange(const Rect finalRect, VirtWnd *wnd) = 0;
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
        WantsMouseMoveBit   = 4,
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

    VirtWnd *GetChild(size_t idx) const {
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
    virtual void Measure(const Size availableSize);
    virtual void Arrange(const Rect finalRect);
    Size            desiredSize;

    // used e.g. by a button to change the look when mouse.The intention
    // is that in response to those a window should only do minimal processing
    // that affects the window itself, not the rest of the system
    virtual void NotifyMouseEnter() {}
    virtual void NotifyMouseLeave() {}
    virtual void NotifyMouseMove(int x, int y) {}

    uint16_t        wantedInputBits; // WndWantedInputBits
    uint16_t        stateBits;       // WndStateBits
    // windows with bigger z-order are painted on top, 0 is default
    int16_t         zOrder;

    bool WantsMouseClick() const {
        return bit::IsSet(wantedInputBits, WantsMouseClickBit);
    }
    bool WantsMouseMove() const {
        return bit::IsSet(wantedInputBits, WantsMouseMoveBit);
    }

    bool IsVisible() const {
        return !bit::IsSet(stateBits, IsHiddenBit);
    }

    void Hide();
    void Show();

    void MeasureChildren(Size availableSize) const;
    void MapRootToMyPos(int& x, int& y) const;

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

    void SetPosition(const Rect& p);

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
        // trigger message queue so that the layout request is processed
        PostMessage(hwndParent, WM_NULL, 0, 0);
    }

    void LayoutIfRequested() {
        if (layoutRequested)
            TopLevelLayout();
    }

    void SetHwnd(HWND hwnd);

    void OnPaint(HWND hwnd) {
        CrashIf(hwnd != hwndParent);
        painter->OnPaint(hwnd);
    }

    virtual void TopLevelLayout();
};

class VirtWndButton : public VirtWnd
{
public:
    VirtWndButton(const TCHAR *s);

    virtual ~VirtWndButton() {
        free(text);
    }

    void SetText(const TCHAR *s);

    void RecalculateSize(bool repaintIfSizeDidntChange);

    virtual void Measure(const Size availableSize);
    virtual void Paint(Graphics *gfx, int offX, int offY);

    virtual void NotifyMouseEnter();
    virtual void NotifyMouseLeave();

    Size GetBorderAndPaddingSize() const;

    void GetStyleForState(Style **first, Style **second) const;
    Prop *GetPropForState(PropType type) const;
    void GetPropsForState(PropToGet *props, size_t propsCount) const;

    Font *GetFontForState() const;

    void SetStyles(Style *def, Style *mouseOver);

    TCHAR *         text;
    size_t          textDx; // cached measured text width

    // gStyleButtonDefault if styleDefault is NULL
    Style *         styleMouseOver; // gStyleButtonMouseOver if NULL
};

}

#endif
